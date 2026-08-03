#include "analyzer.h"

void
analyzer_init_plan(struct GuardianAnalyzer *self)
{
    Oid argtypes[GUARDIAN_MAX_PARAMETERS];

    if (self->nargs < 0 || self->nargs > GUARDIAN_MAX_PARAMETERS)
        ereport(ERROR,
                errmsg("invalid or too many parameters for analyzer \"%s\"", self->name),
                errdetail("max: %d, given: %d", GUARDIAN_MAX_PARAMETERS, self->nargs));

    for (int i = 0; i < self->nargs; i++)
    {
        argtypes[i] = self->params[i].type;
    }

    if (self->plan)
    {
        SPI_freeplan(self->plan);
        self->plan = NULL;
    }

    self->plan = SPI_prepare(self->query, self->nargs, argtypes);
    if (self->plan == NULL)
    {
        ereport(WARNING,
                errmsg("SPI_prepare failed for analyzer \"%s\": %s", self->name, SPI_result_code_string(SPI_result)));
    }
    else
    {
        int rc = SPI_keepplan(self->plan);
        if (rc != 0)
            ereport(ERROR,
                    errmsg("SPI_keepplan failed for analyzer \"%s\"", self->name));
    }
}

void
analyzer_execute_plan(struct GuardianAnalyzer *self)
{
    int rc;
    Datum values[GUARDIAN_MAX_PARAMETERS];
    char nulls[GUARDIAN_MAX_PARAMETERS];

    if (self->pre_execute != NULL)
        self->pre_execute(self);

    if (self->nargs < 0 || self->nargs > GUARDIAN_MAX_PARAMETERS)
        ereport(ERROR,
                errmsg("invalid or too many parameters for analyzer \"%s\"", self->name),
                errdetail("max: %d, given: %d", GUARDIAN_MAX_PARAMETERS, self->nargs));

    for (int i = 0; i < self->nargs; i++)
    {
        values[i] = self->params[i].value;
        nulls[i] = self->params[i].isnull ? 'n' : ' ';
    }

    rc = SPI_execute_plan(self->plan,
                          values,
                          nulls,
                          true,
                          0);

    if (rc < 0)
        ereport(ERROR,
                errmsg("SPI_execute_plan returned %s", SPI_result_code_string(rc)));

    if (SPI_processed > 0 && SPI_tuptable != NULL && self->process_result != NULL)
        self->process_result(self);
}
