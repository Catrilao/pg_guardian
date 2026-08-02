#include "postgres.h"

#include "fmgr.h"
#include "miscadmin.h"

#include "access/xact.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/waiteventset.h"
#include "utils/elog.h"
#include "utils/guc.h"
#include "utils/palloc.h"
#include "utils/resowner.h"
#include "utils/snapmgr.h"
#include "utils/wait_classes.h"

#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>

#include "analyzer.h"

PG_MODULE_MAGIC;

PGDLLEXPORT void worker_main(Datum);
PGDLLEXPORT void _PG_init(void);

static volatile sig_atomic_t got_sigterm = false;
static char *guardian_database;

void
_PG_init(void)
{
    BackgroundWorker worker;

    MemSet(&worker, 0, sizeof(worker));

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
                               "Database to monitor",
                               NULL,
                               &guardian_database,
                               "postgres",
                               PGC_POSTMASTER,
                               0,
                               NULL,
                               NULL,
                               NULL);
}

static void
handle_shutdown(SIGNAL_ARGS)
{
    got_sigterm = true;
    SetLatch(MyLatch);
}

void
worker_main(Datum main_arg)
{
    GuardianAnalyzer *analyzers[10];
    int num_analyzers = 0;

    analyzers[num_analyzers++] = get_storage_analyzer();

    pqsignal(SIGTERM, handle_shutdown);
    BackgroundWorkerUnblockSignals();
    BackgroundWorkerInitializeConnection(guardian_database, NULL, BGWORKER_BYPASS_ROLELOGINCHECK);

    for (size_t i = 0; i < num_analyzers; i++)
    {
        MemoryContext oldcontext = CurrentMemoryContext;

        PG_TRY();
        {
            SetCurrentStatementStartTimestamp();
            StartTransactionCommand();
            PushActiveSnapshot(GetTransactionSnapshot());
            SPI_connect();

            analyzers[i]->init_plan(analyzers[i]);

            SPI_finish();
            PopActiveSnapshot();
            CommitTransactionCommand();
        }
        PG_CATCH();
        {
            ErrorData *edata;

            MemoryContextSwitchTo(oldcontext);

            edata = CopyErrorData();
            FlushErrorState();

            AbortCurrentTransaction();

            elog(WARNING, "analyzer init error: %s", edata->message);
            FreeErrorData(edata);
        }
        PG_END_TRY();
    }

    while (!got_sigterm)
    {
        MemoryContext oldcontext = CurrentMemoryContext;

        WaitLatch(MyLatch,
                  WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
                  5000L,
                  PG_WAIT_EXTENSION);
        ResetLatch(MyLatch);

        PG_TRY();
        {
            SetCurrentStatementStartTimestamp();
            StartTransactionCommand();
            PushActiveSnapshot(GetTransactionSnapshot());
            SPI_connect();

            for (size_t i = 0; i < num_analyzers; i++)
            {
                MemoryContext callercontext = CurrentMemoryContext;
                ResourceOwner callerowner = CurrentResourceOwner;

                if (!analyzers[i]->plan)
                    continue;

                BeginInternalSubTransaction(NULL);

                PG_TRY();
                {
                    analyzers[i]->execute(analyzers[i]);

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

                    elog(WARNING, "%s", edata->message);
                    FreeErrorData(edata);
                }
                PG_END_TRY();
            }

            SPI_finish();
            PopActiveSnapshot();
            CommitTransactionCommand();
        }
        PG_CATCH();
        {
            ErrorData *edata;

            MemoryContextSwitchTo(oldcontext);

            edata = CopyErrorData();
            FlushErrorState();

            AbortCurrentTransaction();

            elog(WARNING, "worker error: %s", edata->message);
            FreeErrorData(edata);
        }
        PG_END_TRY();
    }
    proc_exit(0);
}
