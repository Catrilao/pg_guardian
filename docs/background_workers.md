# PostgreSQL Background Workers

## 1. Loading the Extension

### How Postgres knows to load the library

Add the extension name to shared_preload_libraries in `postgresql.conf`:

```c
shared_preload_libraries = 'pg_guardian'
```

#### Initialization Hook

- Postgres looks for `void _PG_init(void)`, which is called right after loading the shared library.
- `PG_MODULE_MAGIC` should be declared before `_PG_init()` to allow the server to detect incompatibilities, like compiled code from a different PostgreSQL version.
- Always check `process_shared_preload_libraries_in_progress` is true before registering a worker.

## 2. Registering the Worker

Call `RegisterBackgroundWorker(BackgroundWorker *worker)` inside `_PG_init()`.

### Key Fields:

- `bgw_start_time` should be `BgWorker_ConsistentState` because the worker needs to read data to it needs to wait until the database is in a consistent state.

- `bgw_library_name` is the entry point in which the worker will be sought

- `bgw_function_name` will be used to identify the function to be called for the new worker. If it's dinamically called, it must be marked `PGDLLEXPORT` and not `static`.

Set `bgw_flags` to:

- `BGWORKER_BACKEND_DATABASE_CONNECTION` to allow database connections.
- `BGWORKER_SHMEM_ACCESS` to allow shared memory access or worker start-up will fail.

## 3. Connecting to a Database

Once `worker_main()` starts running, it attaches to a database with:

```c
void BackgroundWorkerInitializeConnection(char *dbname, char *username, uint32 flags);
```

```c
static char *guardian_database;
BackgroundWorkerInitializeConnection(guardian_database, NULL, BGWORKER_BYPASS_ROLELOGINCHECK);
```

- Passing `NULL` for `username` makes the worker connect as superuser (created by `initdb`).
- `BGWORKER_BYPASS_ROLELOGINCHECK` skips the normal role login check, so the connection doesn't need an actual login.
- `guardian_database` holds the name of the database the worker operates on.

**Making the database name configurable.** Rather than hardcoding it, `_PG_init()` registers a custom GUC ("Grand Unified Configuration") using `DefineCustomStringVariable()`. Postgres owns and manages the memory behind the string, the static pointer

```c
static char *guardian_database;
```

doesn't allocate anything itself. `DefineCustomStringVariable()` just keeps it pointed at Postgres's internally-managed copy of the current value. Because this GUC's context is `PGC_POSTMASTER`, `pg_guardian.database` can only be changed by restarting the server, so the pointer stays valid for the entire lifetime of the postmaster.

## 5. Sleeping Safely with Latches

The worker runs a while loop, but a C `sleep()` ignores shutdown signals, so it could make the database hang on shutdown while it waits out the clock. Postgres's uses the latch.

A latch is an interruptible wake-up signal that allows a proccess to sleep until another proccess tells it to wake up, rather than using a fixed duration.

- `WaitLatch()` blocks until either the timeout expires or the latch is set.
- `SetLatch()` sets a latch, immediately waking anything blocked inside `WaitLatch()` on it, instead of making it wait out the timeout.
- `ResetLatch()` clears the latch back to "unset" right after waking up, if this is skiped the next loop iteration can fall straight through instead of sleeping.

### Implementation Pattern

#### 1. Unblock signals in the worker's main function

```c
BackgroundWorkerUnblockSignals();

```

#### 2. Declare a signal-handling variable

```c
static volatile sig_atomic_t got_sigshutdown = false;
```

- `volatile`, forces reads from main memory.

- `sig_atomic_t` guarantees atomic read/write, preventing corruption.

#### 3. Create a signal handler

```c
static void
handle_shutdown(SIGNAL_ARGS)
{
    got_sigshutdown = true;
    SetLatch(MyLatch);
}
```

#### 4. Main Loop using `WaitLatch()`

```c
void worker_main(Datum main_arg)
{

    pqsignal(SIGTERM, handle_shutdown); /* Termination signal */
    BackgroundWorkerUnblockSignals();
    BackgroundWorkerInitializeConnection(guardian_database, NULL, BGWORKER_BYPASS_ROLELOGINCHECK);

    while(!got_sigshutdown) {
        WaitLatch(MyLatch,
                  WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
                  1000L, /* timeout in miliseconds */
                  PG_WAIT_EXTENSION);

        ResetLatch(MyLatch);

        /* Some work */
    }
    proc_exit(0); /* Runs cleaning callbacks before terminating the backend */
}
```

## References

[C-Language Functions](https://www.postgresql.org/docs/18/xfunc-c.html)

[Background Worker Processes](https://www.postgresql.org/docs/18/bgworker.html)
