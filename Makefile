MODULES	= pg_guardian

EXTENSION = pg_guardian

DATA = pg_guardian--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
