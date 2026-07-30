#include "postgres.h"
#include "executor/spi.h"
#include "analyzer.h"
#include "utils/elog.h"

void
analyzer_init_plan(struct GuardianAnalyzer *self)
{
    int rc;

    self->plan = SPI_prepare(self->query, 0, NULL);
    if (self->plan == NULL)
    {
        ereport(WARNING,
            errmsg("SPI_prepare failed"));
    }
    else
    {
        rc = SPI_keepplan(self->plan);
        if (rc != 0)
            ereport(ERROR,
                errmsg("SPI_keepplan failed"));
    }
}

void
analyzer_execute(struct GuardianAnalyzer *self)
{
    int rc;
    char *tuple = NULL;

    rc = SPI_execute_plan(self->plan, NULL, NULL, true, 1);
    if (rc != SPI_OK_SELECT)
        ereport(ERROR,
            errmsg("failed to fetch data"),
            errdetail("SPI_execute_plan returned: %d", rc));

    if (SPI_processed > 0 && SPI_tuptable != NULL)
        tuple = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);

    if (tuple)
    {
        ereport(LOG, errmsg("relname: %s, name: %s", tuple, self->name));
        pfree(tuple);
    }
}
