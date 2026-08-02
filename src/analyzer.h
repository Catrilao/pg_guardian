#ifndef ANALYZER_H
#define ANALYZER_H

#include "postgres.h"
#include "executor/spi.h"

#define GUARDIAN_MAX_PARAMETERS 10

typedef struct GuardianParameter
{
    Oid type;
    Datum value;
    bool isnull;
} GuardianParameter;

typedef struct GuardianAnalyzer {	
    const char *name;
    const char *query;

    int nargs;
    GuardianParameter *params;

    SPIPlanPtr plan;
    void (*init_plan)(struct GuardianAnalyzer *self);
    void (*execute)(struct GuardianAnalyzer *self);

    void (*process_result)(struct GuardianAnalyzer *self);
} GuardianAnalyzer;

void analyzer_init_plan(struct GuardianAnalyzer *self);
void analyzer_execute_plan(struct GuardianAnalyzer *self);


GuardianAnalyzer *get_storage_analyzer(void);

#endif
