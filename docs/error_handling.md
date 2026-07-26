# Error Handling in PostgreSQL

In standard C, exceptions do not exist. Normally when a function fails, it returns `-1` or `NULL`, so the caller has to check the return value.

## API for catching `ereport(ERROR)` exits

PostgreSQL uses some macros to achieve the behavior of exceptions.

```c
PG_TRY();
{
    /* code that can throw. ereport(ERROR) */
}
PG_CATCH();
{
    /* cleanup code */
}
PG_END_TRY();
```

### Implementation Details

When `PG_TRY()` is is entered, Postgres calls the `sigsetjmp` function to take a snapshot of the current execution state. This saves the instruction pointer, stack pointer, and CPU registers into a global buffer.

If Postgres encounters `ereport(ERROR, ...)`, it does not return `-1` or `NULL` to the caller. Instead, it calls `siglongjmp`, pointing to the saved buffer. The `siglongjmp` resores the saved execution context and continues execution at the `PG_CATCH()` block.

> [!NOTE]
> This skips all the `return` statements. If a function allocated memory or acquired a file lock before the error occurred, any cleanup code, including calls to `pfree()` or functions that release locks, will be skipped.

## Cleanup Sequence

Inside `PG_CATCH()` the database is in an unstable state. To safely recover, the following steps must be followed:

Copy --> Flush --> Abort/Recover --> Report/Rethrow

1. **Copy:**

When an error occurs, Postgres switches to a global context called `ErrorContext`, it store the error information inside the `ErrorData` struct. When a transaction is aborted, it deletes everything associated with it, including the `ErrorContext`.

So, before deleting the error, we need to change the context we're in using `MemoryContextSwitchTo(TopMemoryContext)`. `TopMemoryContext` is the root of the graph, the postmaster creates it when boots and is never destroyed.

After this, we can safely copy the error using `ErrorData *CopyErrorData(void)`.

2. **Flush:**

After copying the error, we reset the `ErrorContext` signaling that the backend is not in an error state. This is done with `FlushErrorState()`.

3. **Abort/Recover:**

To continue with the normal execution, we switch back to the context that existed before the error with `MemoryContextSwitchTo(MemoryContext oldcontext)`.

4. **Report/Rethrow:**

After that we call `AbortCurrentTransaction()` to reset the transaction state machine (releasing locks, unpinning buffers, etc).

At this point, we can choose to just report using `Elog(WARNING, ...)` using the error data copied in the first step and let the code continue its normal cycle. Or throw an exception to propagate the error outwards with `PG_RE_THROW` which will look for the next `PG_TRY()`.

Finally, we deallocate the memory of the copied error from stage one using `FreeErrorData(ErrorData *edata)`.

### Rethrowing vs Swallowing

This decision depends on the purpose of the code. For a standard extension used by a user's query, `PG_RE_THROW` is preferred.
However, this extension is a background worker. So it needs to keep running as there is no client waiting for a response. If the error was re-thrown it would reach the worker's root execution context, unhandled, crashing the worker process. The postmaster would then refer to the `bgw_restart_time`, respawning the process, which could lead to an infinite loop.

## Example

```c
PG_CATCH();
{
    MemoryContext oldcontext;
    ErrorData* edata;

    oldcontext = MemoryContextSwitchTo(TopMemoryContext);
    edata = CopyErrorData();
    FlushErrorState();

    MemoryContextSwitchTo(oldcontext);

    AbortCurrentTransaction();

    elog(WARNING, "error: %s", edata->message);
    FreeErrorData(edata);
}
```

# References

- [elog.h](https://github.com/postgres/postgres/blob/master/src/include/utils/elog.h)
- [elog.c](https://github.com/postgres/postgres/blob/master/src/backend/utils/error/elog.c)
