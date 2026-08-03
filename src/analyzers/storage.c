#include "../analyzer.h"

#include "catalog/pg_type_d.h"
#include "utils/guc.h"

#include <limits.h>

static int storage_page_threshold;
static int storage_limit_response;

static GuardianParameter params[2] = {
    { .type = INT4OID, .value = 0, .isnull = false },
    { .type = INT4OID, .value = 0, .isnull = false },
};

/* TODO: add specific processing */
static inline void
storage_process_result(struct GuardianAnalyzer *self)
{
    for (uint64 i = 0; i < SPI_processed; i++)
    {
        char *tuple = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1);
        if (tuple)
        {
            ereport(LOG,
                    errmsg("relname: %s", tuple));
            pfree(tuple);
        }
    }
}

static void
storage_pre_execute(struct GuardianAnalyzer *self)
{
    self->params[0].value = Int32GetDatum(storage_page_threshold);
    self->params[1].value = Int32GetDatum(storage_limit_response);
}

static void
storage_register_gucs(void)
{
    DefineCustomIntVariable("pg_guardian.storage_page_threshold",
                            "Minimum pages for storage analyzer",
                            NULL,
                            &storage_page_threshold,
                            25,
                            0,
                            INT_MAX,
                            PGC_SIGHUP,
                            0,
                            NULL,
                            NULL,
                            NULL);

    DefineCustomIntVariable("pg_guardian.storage_limit_response",
                            "Maximum number of rows to be returned",
                            NULL,
                            &storage_limit_response,
                            3,
                            1,
                            INT_MAX,
                            PGC_SIGHUP,
                            0,
                            NULL,
                            NULL,
                            NULL);
}

/* TODO: make a complex query with many parameters
 * this would make the query "dynamic" for the user
 */
static GuardianAnalyzer storage_analyzer = {
    .name = "storage_analyzer",
    .query = "SELECT relname FROM pg_class WHERE relpages > $1 LIMIT $2",
    .nargs = 2,
    .plan = NULL,
    .params = params,
    .init_plan = analyzer_init_plan,
    .register_gucs = storage_register_gucs,
    .pre_execute = storage_pre_execute,
    .execute = analyzer_execute_plan,
    .process_result = storage_process_result
};

GuardianAnalyzer *
get_storage_analyzer(void)
{
    return &storage_analyzer;
}
