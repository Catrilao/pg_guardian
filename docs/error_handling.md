# Error Handling in PostgreSQL

In standard C, exceptions do not exist. Normally when a function fails, it returns `-1` or `NULL`, so the caller has to check the return value.

## API for catching `ereport(ERROR)` exits

PostgreSQL implements exception-like error handling using the `PG_TRY()` macros.

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

When `PG_TRY()` is is entered, PostgreSQL calls `sigsetjmp()` to save the current execution environment into a `sigjmp_buf`.

If PostgreSQL encounters `ereport(ERROR, ...)`, it does not return `-1` or `NULL` to the caller. Instead, it calls `siglongjmp()` causing the `sigsetjmp` to return a non-zero value, which makes the execution continue in the `PG_CATCH()` branch.

`PG_FINALLY()` is used when the same cleanup steps will run regardless of whether an error ocurred. Unlike `PG_CATCH()`, it cannot suppress the error, always rethrows the original error after the cleanup block finishes.

> [!NOTE]
> This skips normal control flow. Any code between the error and the rest of the `PG_TRY()` block will be skipped. Resource cleanup must happen inside `PG_CATCH()` or `PG_FINALLY()`.

> [!NOTE]
> If a local variable is modified inside `PG_TRY()` and used inside `PG_CATCH()`, it must be declared `volatile`.

## Cleanup Sequence

Inside `PG_CATCH()` the database is in an unstable state. To safely recover, the following steps must be followed:

Copy --> Flush --> Abort/Recover --> Report/Rethrow

This sequence follows the pattern used throughout PostgreSQL's backend (e.g., PL/Python).

1. **Copy:**

When an error occurs, PostgreSQL switches from `CurrentMemoryContext` to `ErrorContext` so that the error recovery can proceed even if the backend has exhausted memory. `ErrorData` describing the error is allocated here. `ErrorContext` is permanent, its contents are reset during error recovery. Therefore, if the error information needs to survive after `FlushErrorState()`, it must be copied into another memory context.

We switch to the caller's saved memory context before calling `CopyErrorData`. This ensures the copied error is allocated in a context with a known lifetime.

After this, we can safely copy the error using `ErrorData *CopyErrorData(void)`.

2. **Flush:**

`FlushErrorState()` discards the current error state and resets `ErrorContext`. After this call, the original `ErrorData` is no longer available. This is why `CopyErrorData()` must be called before.

3. **Abort/Recover:**

After that we call `AbortCurrentTransaction()` to abort the transaction, allowing PostgreSQL to perform its normal transaction cleanup, including releasing resources and destroying transaction-local memory contexts.

The error must be copied before aborting the transaction, because transaction cleanup may destroy the memory context in which the error lived.

4. **Report/Rethrow:**

At this point, we can choose to just report using `elog(WARNING, ...)` using the error data copied in the first step and let the code continue its normal cycle. Or throw an exception to propagate the error outwards with `PG_RE_THROW` which will call `siglongjmp`, execution continues in the nearest enclosing `PG_TRY()`.

Finally, we deallocate the memory of the copied error from stage one using `FreeErrorData(ErrorData *edata)`.

> [!WARNING]
> Never continue execution inside `PG_CATCH()` without aborting the current transaction, releasing the current subtransaction or rethrowing the error.
> After `ereport(ERROR)`, the transaction is left in an aborted state until PostgreSQL's transaction cleanup functions are called. Continuing without cleanup leaves backend state inconsistent.

### Rethrowing vs Swallowing

This decision depends on the purpose of the code. For a standard extension used by a user's query, `PG_RE_THROW` is preferred.
However, this extension is a background worker. So it needs to keep running as there is no client waiting for a response. If the error was re-thrown it would reach the worker's root execution context, unhandled, terminating the worker process. The postmaster would then refer to the `bgw_restart_time`, respawning the process, which could lead to an infinite loop.
Swallowing an error is appropriate only when the code can safely recover and continue execution.

## Examples

The caller's `MemoryContext` (and for subtransactions `ResourceOwner`) is captured **before** entering the `PG_TRY` block because the code inside the the block may change the current execution environment before raising an error.

### Transaction

```c
MemoryContext oldcontext = CurrentMemoryContext;

PG_TRY();
{
    ...
}
PG_CATCH();
{
    ErrorData *edata;

    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

    AbortCurrentTransaction();

    elog(WARNING, "error: %s", edata->message);
    FreeErrorData(edata);
}
PG_END_TRY();
```

### Subtransaction

`CurrentResourceOwner` tracks the resources such as relation references, snapshots, and other objects that must be released when a transaction or subtransaction ends. Many PostgreSQL APIs implicitly associate acquired resources with `CurrentResourceOwner`.

`BeginInternalSubTransaction()` creates a new transaction state, includind a new `MemoryContext` and `ResourceOwner`, so it's common to save and restore the caller's execution environment.

```c
MemoryContext callercontext = CurrentMemoryContext;
ResourceOwner callerowner = CurrentResourceOwner;

BeginInternalSubTransaction(NULL);

PG_TRY();
{
    ...

    ReleaseCurrentSubTransaction();

    MemoryContextSwitchTo(callercontext);
    CurrentResourceOwner = callerowner;
}
PG_CATCH();
{
    ErrorData *edata;

    MemoryContextSwitchTo(callercontext);
    edata = CopyErrorData();
    FlushErrorState();

    RollbackAndReleaseCurrentSubTransaction();

    MemoryContextSwitchTo(callercontext);
    CurrentResourceOwner = callerowner;

    elog(WARNING, "error: %s", edata->message);
    FreeErrorData(edata);
}
PG_END_TRY();
```

# References

- [elog.h](https://github.com/postgres/postgres/blob/master/src/include/utils/elog.h)
- [elog.c](https://github.com/postgres/postgres/blob/master/src/backend/utils/error/elog.c)
