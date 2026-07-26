# Server Programming Interface (SPI)

This gives the ability to execute SQL queries inside C code. SPI is a set of interface that gives access to the parser, planner and executor, it also does a little of memory management.

## Error Handling

If a command call via SPI fails, the error is not returned to the C code but the SQL transaction rollback. The errors that are returned are inside the SPI functions. If a transaction is established surrounding a SPI call, then is possible to recover control.

SPI functions return negative values of `NULL` for errors.
On success, a nonnegative value is returned, it could be in an integer value or in the global variable `SPI_result`.

## Execution Lifecycle

1. **Start Transaction**
   Call these functions in this order.

   - `SetCurrentStatementStartTimestamp()` sets the time for the statement that is going to be executed and the transaction start time, so that the statement is always up to date.

```c
SetCurrentStatementStartTimestamp();
StartTransactionCommand();
```

2. **Connection to SPI**

To connect a function to the SPI manager and be able to execute commands via SPI one of these functions should be called.

```c
int SPI_connect(void)
```

```c
int SPI_connect_ext(int options)
```

- Flag for `SPI_connect_ext()`:

* `SPI_OPT_NONATOMIC`: Transaction control calls (`SPI_commit`, `SPI_rollback`) are allowed.

This function calculates the state of all running and committed transaction in at this moment.

```c
PushActiveSnapshot(GetTransactionSnapshot());
```

3. **Execution**

```c
int SPI_execute(const * command, bool read_only, long count)
```

- Parameters:

* `command`: SQL query
* `read_only`: If set to `true` the command must be read-only, execution overhead is reduced a bit
* `count`: Number of rows that will be returned, 0 for no limit. It works like `LIMIT`

> [!WARNING]
> Do not mix read-only and read-write commands in the same SPI. The read-only queries would not see the result of the read-write queries.

4. **Finishing:**
   To disconnect from the SPI manager call

```c
int SPI_finish(void)
```

After that, clean the snapshot and commit the transaction

`PushActiveSnapshot()` pushes the state to the stack so that the SPI executor know which tuples are visible to the executed query and which ones should be hidden.

```c
PopActiveSnapshot();
CommitTransactionCommand();
```

### Example

```c
SetCurrentStatementStartTimestamp();
StartTransactionCommand();
SPI_connect();
PushActiveSnapshot(GetTransactionSnapshot());

ret = SPI_execute(sql_query, read_only, limit);

/* Error handling */

/* some work */

SPI_finish();
PopActiveSnapshot();
CommitTransactionCommand();
```

# References

- [Server Programming Interface](https://www.postgresql.org/docs/current/spi.html)
- [worker_spi.c](https://github.com/postgres/postgres/blob/master/src/test/modules/worker_spi/worker_spi.c)
