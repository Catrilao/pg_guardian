# PG GUARDIAN

PostgreSQL extension focused on database reliability, health analysis and operational diagnostics.

It focuses on the structural health of the database. Possible modules include:

- Configuration Analyzer
- Storage Analyzer
- Vacuum Analyzer
- Index Analyzer
- Transaction Analyzer
- Replication Analyzer
- WAL Analyzer
- Lock Analyzer
- Statistics Analyzer
- Schema Analyzer

It should help answer questions such as:

- Which tables are most at risk of transaction ID wraparound?
- Are there indexes consuming disk space that are never used?
- Is autovacuum keeping up with database growth?
- Are there long-running transactions preventing cleanup?
- Are replication slots retaining excessive WAL?
- Is the current configuration appropriate for production?
- Which tables are experiencing abnormal dead tuple growth?
- Is the database likely to experience bloat?
- Are there schema design decisions that may reduce reliability?

## Architecture

pg_guardian is a C-based Background Worker

## Development

This project uses Nix for reproducible builds.

For accessing the Nix shell:

```sh
nix develop
```

Compilation:

```sh
make
```

## Local Testing

The background worker is loaded through `dynamic_library_path`, so you need a local PostgreSQL cluster before starting the server.

### 1. Enter the development shell

```sh
nix develop
```

### 2. Build the extension

```sh
make
```

### 3. Initialize a local PostgreSQL cluster

```sh
mkdir -p .pgdata
initdb -D .pgdata
```

### 4. Configure PostgreSQL

Edit `.pgdata/postgresql.conf` and add:

```conf
shared_preload_libraries = 'pg_guardian'
dynamic_library_path = '$libdir:/<path-to-extension>/pg_guardian'
```

> **Note:** PostgreSQL must be able to locate `pg_guardian.so`. Depending on your development environment, you may need to install the extension into PostgreSQL's library path or configure PostgreSQL to load it from your build output.

### 5. Start PostgreSQL

```sh
pg_ctl \
  -D .pgdata \
  -l .logfile \
  start
```

### 6. Stop PostgreSQL

```sh
pg_ctl -D .pgdata stop
```
