# High-Concurrency-Memory-Pool

A learning project for a high-concurrency memory pool (TCMalloc-like layered cache design).

## Structure

- `ConcurrentMemoryPool/ConcurrentAlloc.hpp`: public allocation API (`ConcurrentAlloc`/`ConcurrentFree`)
- `ConcurrentMemoryPool/ThreadCache.hpp`: thread-local freelists
- `ConcurrentMemoryPool/CentralCache.hpp`: shared central cache
- `ConcurrentMemoryPool/PageCache.hpp`: span/page management
- `ConcurrentMemoryPool/bench/allocator_bench.cc`: benchmark entry

## Build

```bash
cd /Users/chenjunwei/project/High-Concurrency-Memory-Pool/ConcurrentMemoryPool
make
./build/UnitTest
```

## Benchmark

Build benchmark:

```bash
cd /Users/chenjunwei/project/High-Concurrency-Memory-Pool/ConcurrentMemoryPool
make bench
```

Single-case example:

```bash
./build/allocator_bench \
  --allocator=pool \
  --threads=8 \
  --size=64 \
  --size-dist=fixed \
  --mode=immediate \
  --warmup=1 \
  --seconds=2 \
  --sample-rate=4096 \
  --label=pool_t8_s64
```

Matrix run:

```bash
./bench/run_matrix.sh
```

More benchmark options are documented in:

- `ConcurrentMemoryPool/bench/README.md`

## Local Performance Snapshot

### Fixed size (64B), immediate free, warmup=1s, measure=2s

| Threads | pool (ops/s) | malloc (ops/s) | Throughput gain |
| --- | ---: | ---: | ---: |
| 1 | 3.13e7 | 2.54e7 | +23.3% |
| 2 | 6.07e7 | 4.90e7 | +23.9% |
| 4 | 1.13e8 | 8.93e7 | +26.8% |
| 8 | 1.53e8 | 1.30e8 | +17.7% |

In this scenario, average alloc/free latency is also lower than `malloc/free`.

### Mixed sizes + window mode (current status)

Parameters: `size-dist=mixed`, `mode=window`, `window=1024`, `warmup=1s`, `measure=2s`.

- 4 threads: pool throughput is about `-64.6%` vs malloc
- 8 threads: pool throughput is about `-48.7%` vs malloc

This indicates mixed-size high-contention paths still need optimization.
