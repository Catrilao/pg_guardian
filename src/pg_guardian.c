#include "c.h"
#include "miscadmin.h"
#include "postgres.h"
#include "fmgr.h"
#include "postmaster/bgworker.h"
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/waiteventset.h"
#include "utils/memutils.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#include "utils/snapmgr.h"
#include "utils/wait_classes.h"
#include "utils/guc.h"
#include "executor/spi.h"

PG_MODULE_MAGIC;

PGDLLEXPORT void worker_main(Datum);
PGDLLEXPORT void _PG_init(void);

static volatile sig_atomic_t got_sigshutdown = false;
static char* guardian_database;

void _PG_init(void)
{
    BackgroundWorker worker;
    memset(&worker, 0, sizeof(worker));

    snprintf(worker.bgw_name, BGW_MAXLEN, "pg_guardian: main");
    snprintf(worker.bgw_type, BGW_MAXLEN, "pg_guardian");
    worker.bgw_flags = BGWORKER_BACKEND_DATABASE_CONNECTION | BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_ConsistentState;
    worker.bgw_restart_time = 3;
    snprintf(worker.bgw_library_name, MAXPGPATH, "pg_guardian");
    snprintf(worker.bgw_function_name, BGW_MAXLEN, "worker_main");
    worker.bgw_main_arg = (Datum)0;
    worker.bgw_notify_pid = 0;

    if (!process_shared_preload_libraries_in_progress)
        return;

    RegisterBackgroundWorker(&worker);

    DefineCustomStringVariable("pg_guardian.database",
        "Database to monitor", NULL,
        &guardian_database, "postgres",
        PGC_POSTMASTER,
        0, NULL, NULL, NULL);
}

static void
handle_shutdown(SIGNAL_ARGS)
{
    got_sigshutdown = true;
    SetLatch(MyLatch);
}

void worker_main(Datum main_arg)
{

    pqsignal(SIGTERM, handle_shutdown);
    BackgroundWorkerUnblockSignals();
    BackgroundWorkerInitializeConnection(guardian_database, NULL, BGWORKER_BYPASS_ROLELOGINCHECK);

    while (!got_sigshutdown) {
        WaitLatch(MyLatch,
            WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
            20000L,
            PG_WAIT_EXTENSION);

        ResetLatch(MyLatch);

        PG_TRY();
        {
            int rc;
            char* tuple = NULL;

            SetCurrentStatementStartTimestamp();
            StartTransactionCommand();
            PushActiveSnapshot(GetTransactionSnapshot());
            SPI_connect();

            rc = SPI_execute("SELECT relname FROM pg_class", true, 1);
            if (rc != SPI_OK_SELECT)
                ereport(ERROR,
                    errmsg("Failed to fetch data"),
                    errdetail("SPI returned: %d", rc));

            if (SPI_processed > 0) {
                tuple = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
            }
            if (tuple) {
                ereport(LOG, errmsg("relname: %s", tuple));
                pfree(tuple);
            }

            SPI_finish();
            PopActiveSnapshot();
            CommitTransactionCommand();
        }
        PG_CATCH();
        {
            MemoryContext oldcontext;
            ErrorData* edata;

            oldcontext = MemoryContextSwitchTo(TopMemoryContext);
            edata = CopyErrorData();
            MemoryContextSwitchTo(oldcontext);

            FlushErrorState();

            AbortCurrentTransaction();

            elog(WARNING, "worker error: %s", edata->message);
            FreeErrorData(edata);
        }
        PG_END_TRY();
    }
    proc_exit(0);
}
