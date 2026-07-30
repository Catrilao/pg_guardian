# Benchmarking Code

PostgreSQL provides the `instr_time` type and a set of macros for measuring elapsed time.

## Example

```c
instr_time start, end;

INSTR_TIME_SET_CURRENT(start);

/* work to benchmark */

INSTR_TIME_SET_CURRENT(end);
INSTR_TIME_SUBTRACT(end, start);

elog(LOG, "Time: %.3f", INSTR_TIME_GET_MILLISEC(end));
```

## Explanation

1. Record the current timestamp.

```c
INSTR_TIME_SET_CURRENT(start);
```

2. Execute the code being measured.

```c
/* work to benchmark */
```

3. Record the current timestamp.

```c
INSTR_TIME_SET_CURRENT(end);
```

4. Compute the elapsed time.

```c
INSTR_TIME_SUBTRACT(end, start); /* <=> end = end - start */
```

5. Convert the elapsed time to the desired representation.

```c
INSTR_TIME_GET_MILLISEC(end);
```

Available conversion macros include:

- `INSTR_TIME_GET_MILLISEC()`
- `INSTR_TIME_GET_MICROSEC()`
- `INSTR_TIME_GET_NANOSEC()`
- `INSTR_TIME_GET_DOUBLE()`

## Note

- `instr_time` is an opaque PostgreSQL type. It should only be manipulated via `INSTR_TIME_*` macros.
- The timing overhead is small enough for most extensions and background workers.
- This measure real-world elapsed time, not CPU time.

## References

- [instr_time.h](https://doxygen.postgresql.org/instr__time_8h_source.html)
