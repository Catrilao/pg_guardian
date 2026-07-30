#ifndef ANALYZER_H
#define ANALYZER_H

#include "postgres.h"
#include "executor/spi.h"


typedef struct GuardianAnalyzer {	
    char *name;
    char *query;
    SPIPlanPtr plan;
    void (*analyzer_init_plan)(struct GuardianAnalyzer *self);
    void (*analyzer_execute)(struct GuardianAnalyzer *self);
} GuardianAnalyzer;

void analyzer_init_plan(struct GuardianAnalyzer *self);
void analyzer_execute(struct GuardianAnalyzer *self);

#endif
