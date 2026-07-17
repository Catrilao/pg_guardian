# Core Functions

- `void _PG_init(void)`: Initialization hook called immediately after the shared library loads.

- `void RegisterBackgroundWorker(BackgroundWorker *worker)`: Register the worker struct with the postmaster.

- `void BackgroundWorkerInitializeConnection(char *dbname, char *username, int32 flags)`: Attaches the running worker to a database.

- `void BackgroundWorkerUnblockSignals(void)`: Allows the use of signals for the worker.

- `int WaitLatch(volatile Latch *latch, int wakeEvents, long timeout, uint32 wait_event_info)`: Blocks execution until a timeout passes or a latch is set.

- `void SetLatch(volatile Latch *latch)`: Immediately wakes up any process blocked by a specific latch.

- `void ResetLatch(volatile Latch *latch)`: Clears the latch state to "unset" after waking up.

- `pqsigfunc pqsignal(int signum, pqsigfunc handler)`: Assigns a signal handler.

- `void proc_exit(int code)`: Runs cleanup callbacks before terminating the backend.

- `void DefineCustomStringVariable(...)`: Register a custom GUC parameter.

# Macros and Flags

- `PG_MODULE_MAGIC`: Must be declared before `_PG_init()` to verify PostgreSQL version.

- `PGDLLEXPORT`: Marks dynamically called functions so they are exposed to the PostgreSQL loader.

- `BGWORKER_BACKEND_DATABASE_CONNECTION`: Allows the worker to be connected to a database.

- `BGWORKER_SHMEM_ACCESS`: Allows the worker to access shared memory.

- `BGWORKER_BYPASS_ROLELOGINCHECK`: Allows the worker to skip login.

- `WL_LATCH_SET`: Tells the process that it should wake up when the latch is set.

- `WL_TIMEOUT`: Tells the process to wake up after a timeout.

- `WL_EXIT_ON_PM_DEATH`: Ensures the worker terminates if the postmaster dies.

# Types and Globals

- `process_shared_preload_libraries_in_progress`: Global variable that must be `true` before registering a worker.

- `MyLatch`: Global pointer to the current process's latch.

- `BgWorkerStart_ConsistentState`: Start time enum indicating the database is fully recovered and readable.

- `sig_atomic_t`: C type guaranteeing atomic read/write for signal handlers.

- `Datum`: Generic PostgreSQL data type. Contains either a value or a pointer to a value.
