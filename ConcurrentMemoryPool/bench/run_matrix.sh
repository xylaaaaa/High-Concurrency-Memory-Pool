#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${ROOT_DIR}/build/allocator_bench"

WARMUP_SECONDS="${WARMUP_SECONDS:-2}"
MEASURE_SECONDS="${MEASURE_SECONDS:-5}"
WINDOW_SIZE="${WINDOW_SIZE:-4096}"
SAMPLE_RATE="${SAMPLE_RATE:-1024}"

ALLOCATORS=(${ALLOCATORS:-pool malloc})
THREADS=(${THREADS:-1 2 4 8})
SIZES=(${SIZES:-8 64 256 1024 4096})
MODES=(${MODES:-immediate window})

mkdir -p "${ROOT_DIR}/bench/results/raw"
TS="$(date +%Y%m%d_%H%M%S)"
CSV="${ROOT_DIR}/bench/results/raw/allocator_bench_${TS}.csv"

cd "${ROOT_DIR}"
make bench

echo "Writing benchmark output to: ${CSV}"

for allocator in "${ALLOCATORS[@]}"; do
  for mode in "${MODES[@]}"; do
    for threads in "${THREADS[@]}"; do
      for size in "${SIZES[@]}"; do
        label="${allocator}_${mode}_t${threads}_s${size}"
        "${BIN}" \
          --allocator="${allocator}" \
          --threads="${threads}" \
          --size="${size}" \
          --size-dist=fixed \
          --mode="${mode}" \
          --window="${WINDOW_SIZE}" \
          --warmup="${WARMUP_SECONDS}" \
          --seconds="${MEASURE_SECONDS}" \
          --sample-rate="${SAMPLE_RATE}" \
          --label="${label}" \
          --csv="${CSV}"
      done
    done
  done
done

for allocator in "${ALLOCATORS[@]}"; do
  for threads in "${THREADS[@]}"; do
    label="${allocator}_mixed_window_t${threads}"
    "${BIN}" \
      --allocator="${allocator}" \
      --threads="${threads}" \
      --size=64 \
      --size-dist=mixed \
      --mode=window \
      --window="${WINDOW_SIZE}" \
      --warmup="${WARMUP_SECONDS}" \
      --seconds="${MEASURE_SECONDS}" \
      --sample-rate="${SAMPLE_RATE}" \
      --label="${label}" \
      --csv="${CSV}"
  done
done

echo "Done."
echo "CSV path: ${CSV}"
