MODULE_big = pg_guardian

OBJS = \
       src/pg_guardian.o \
       src/analyzer.o \
	src/analyzers/storage.o

EXTENSION = pg_guardian

DATA = sql/pg_guardian--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
