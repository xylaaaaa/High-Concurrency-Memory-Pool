# High-Concurrency-Memory-Pool

A learning project for a high-concurrency memory pool (TCMalloc-like layered cache design).

## Structure

- `ConcurrentMemoryPool/ConcurrentAlloc.hpp`: public allocation API (`ConcurrentAlloc`/`ConcurrentFree`)
- `ConcurrentMemoryPool/AllocatorWrapper.hpp`: integration wrapper (RAII + STL allocator adapter)
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

## Integrate Into Other Projects

Build integration demo:

```bash
cd /Users/chenjunwei/project/High-Concurrency-Memory-Pool/ConcurrentMemoryPool
make demo
./build/allocator_demo
```

Example usage in your code:

```cpp
#include "AllocatorWrapper.hpp"
#include <vector>

struct Order {
    int id;
    explicit Order(int oid) : id(oid) {}
};

void UsePool()
{
    // RAII object
    auto order = cmp::MakeUnique<Order>(42);

    // STL container allocator adapter
    std::vector<int, cmp::PoolAllocator<int>> values;
    values.push_back(order->id);
}
```

Recommended migration path:

1. Replace malloc/free or new/delete on hot paths with `cmp::MakeUnique` and `cmp::PoolAllocator`.
2. Keep non-hot or very large objects on the default path.
3. Run `bench/allocator_bench.cc` before/after to verify throughput and latency.

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

- 4 threads: pool throughput is about `+19.3%` vs malloc
- 8 threads: pool throughput is about `+14.2%` vs malloc

This indicates mixed-size high-contention performance has improved and now beats malloc in this test setup.
