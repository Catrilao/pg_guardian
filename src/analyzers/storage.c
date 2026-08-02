#include "postgres.h"

#include "catalog/pg_type_d.h"

#include "../analyzer.h"

static GuardianParameter params[1];

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
super_mega_complex_init(struct GuardianAnalyzer *self)
{
    ereport(LOG,
            errmsg("complex, name: %s", self->name));
}

/* TODO: make a complex query with many parameters
 * this would make the query "dynamic" for the user
 */
static GuardianAnalyzer storage_analyzer = {
    .name = "storage_analyzer",
    .query = "SELECT relname FROM pg_class WHERE relpages > $1",
    .nargs = 1000000,
    .plan = NULL,
    .params = params,
    .init_plan = super_mega_complex_init,
    .execute = analyzer_execute_plan,
    .process_result = storage_process_result
};

/* TODO: change the parameters during live execution */
GuardianAnalyzer *
get_storage_analyzer(void)
{
    static bool initialized = false;

    if (!initialized)
    {
        params[0].type = INT4OID;
        params[0].value = Int32GetDatum(25);
        params[0].isnull = false;
        initialized = true;
    }

    return &storage_analyzer;
}
