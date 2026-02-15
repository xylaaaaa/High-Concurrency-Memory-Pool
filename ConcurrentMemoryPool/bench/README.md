# allocator_bench

## Build

```bash
cd /Users/chenjunwei/project/High-Concurrency-Memory-Pool/ConcurrentMemoryPool
make bench
```

Binary output:

```bash
/Users/chenjunwei/project/High-Concurrency-Memory-Pool/ConcurrentMemoryPool/build/allocator_bench
```

## Single scenario examples

```bash
# pool allocator, fixed-size, immediate free
./build/allocator_bench \
  --allocator=pool \
  --threads=8 \
  --size=64 \
  --size-dist=fixed \
  --mode=immediate \
  --warmup=2 \
  --seconds=10 \
  --sample-rate=1024 \
  --label=pool_t8_s64

# malloc baseline, fixed-size, window mode
./build/allocator_bench \
  --allocator=malloc \
  --threads=8 \
  --size=64 \
  --size-dist=fixed \
  --mode=window \
  --window=4096 \
  --warmup=2 \
  --seconds=10 \
  --sample-rate=1024 \
  --label=malloc_t8_s64_window
```

## CSV output

```bash
mkdir -p bench/results/raw
./build/allocator_bench \
  --allocator=pool \
  --threads=8 \
  --size=64 \
  --seconds=10 \
  --csv=bench/results/raw/sample.csv \
  --label=pool_t8_s64
```

CSV columns include:

- scenario metadata (`label/allocator/mode/size_dist/size/threads`)
- throughput (`ops_per_sec`)
- alloc/free op counts
- alloc/free latency (`avg/p50/p95/p99`, ns)

## Matrix run script

```bash
chmod +x bench/run_matrix.sh
./bench/run_matrix.sh
```

Optional environment overrides:

```bash
MEASURE_SECONDS=8 THREADS="1 2 4 8 16" SIZES="8 64 256 1024" ./bench/run_matrix.sh
```
