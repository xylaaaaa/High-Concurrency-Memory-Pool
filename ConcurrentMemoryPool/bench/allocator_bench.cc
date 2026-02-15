#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ConcurrentAlloc.hpp"

namespace
{
using SteadyClock = std::chrono::steady_clock;
using SystemClock = std::chrono::system_clock;

struct Config
{
    std::string allocator = "pool";      // pool | malloc
    std::string mode = "immediate";      // immediate | window
    std::string size_dist = "fixed";     // fixed | mixed
    std::string csv_path;                // optional
    std::string label = "default";       // optional scenario label
    size_t threads = 1;
    size_t size = 64;
    size_t window = 4096;
    size_t sample_rate = 1024;           // one latency sample every N ops
    int warmup_seconds = 2;
    int measure_seconds = 10;
};

struct WorkerStats
{
    uint64_t alloc_ops = 0;
    uint64_t free_ops = 0;
    uint64_t alloc_ns_total = 0;
    uint64_t free_ns_total = 0;
    std::vector<uint32_t> alloc_samples;
    std::vector<uint32_t> free_samples;
};

struct LatencySummary
{
    double avg_ns = 0.0;
    uint64_t p50_ns = 0;
    uint64_t p95_ns = 0;
    uint64_t p99_ns = 0;
    size_t samples = 0;
};

struct BenchmarkResult
{
    Config config;
    int64_t timestamp_unix = 0;
    double measured_seconds = 0.0;
    uint64_t alloc_ops = 0;
    uint64_t free_ops = 0;
    uint64_t total_ops = 0;
    double ops_per_sec = 0.0;
    LatencySummary alloc_latency;
    LatencySummary free_latency;
};

typedef void *(*AllocFn)(size_t);
typedef void (*FreeFn)(void *, size_t);

static uint64_t XorShift64(uint64_t &state)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

static uint64_t ToNs(SteadyClock::duration duration)
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
}

static std::string ToLower(std::string s)
{
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] >= 'A' && s[i] <= 'Z')
        {
            s[i] = static_cast<char>(s[i] - 'A' + 'a');
        }
    }
    return s;
}

static bool ParseUInt64(const std::string &s, uint64_t &value)
{
    if (s.empty())
    {
        return false;
    }

    char *end = nullptr;
    unsigned long long parsed = std::strtoull(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0')
    {
        return false;
    }

    value = static_cast<uint64_t>(parsed);
    return true;
}

static void PrintUsage(const char *prog)
{
    std::cout
        << "Usage:\n"
        << "  " << prog
        << " [--allocator=pool|malloc]"
        << " [--threads=N]"
        << " [--size=BYTES]"
        << " [--size-dist=fixed|mixed]"
        << " [--mode=immediate|window]"
        << " [--window=N]"
        << " [--warmup=SECONDS]"
        << " [--seconds=SECONDS]"
        << " [--sample-rate=N]"
        << " [--label=NAME]"
        << " [--csv=/path/file.csv]\n\n"
        << "Examples:\n"
        << "  " << prog << " --allocator=pool --threads=8 --size=64 --seconds=10\n"
        << "  " << prog << " --allocator=malloc --threads=8 --size-dist=mixed --mode=window --window=4096\n";
}

static bool ParseArgs(int argc, char **argv, Config &config, std::string &error)
{
    unsigned int hw = std::thread::hardware_concurrency();
    config.threads = hw == 0 ? 1 : hw;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            PrintUsage(argv[0]);
            std::exit(0);
        }

        if (arg.rfind("--", 0) != 0)
        {
            error = "Invalid argument: " + arg;
            return false;
        }

        size_t eq = arg.find('=');
        if (eq == std::string::npos || eq <= 2 || eq + 1 >= arg.size())
        {
            error = "Expected --key=value format: " + arg;
            return false;
        }

        std::string key = arg.substr(2, eq - 2);
        std::string value = arg.substr(eq + 1);
        key = ToLower(key);

        uint64_t n = 0;
        if (key == "allocator")
        {
            config.allocator = ToLower(value);
        }
        else if (key == "threads")
        {
            if (!ParseUInt64(value, n) || n == 0)
            {
                error = "Invalid --threads value: " + value;
                return false;
            }
            config.threads = static_cast<size_t>(n);
        }
        else if (key == "size")
        {
            if (!ParseUInt64(value, n) || n == 0)
            {
                error = "Invalid --size value: " + value;
                return false;
            }
            config.size = static_cast<size_t>(n);
        }
        else if (key == "size-dist")
        {
            config.size_dist = ToLower(value);
        }
        else if (key == "mode")
        {
            config.mode = ToLower(value);
        }
        else if (key == "window")
        {
            if (!ParseUInt64(value, n) || n == 0)
            {
                error = "Invalid --window value: " + value;
                return false;
            }
            config.window = static_cast<size_t>(n);
        }
        else if (key == "warmup")
        {
            if (!ParseUInt64(value, n))
            {
                error = "Invalid --warmup value: " + value;
                return false;
            }
            config.warmup_seconds = static_cast<int>(n);
        }
        else if (key == "seconds")
        {
            if (!ParseUInt64(value, n) || n == 0)
            {
                error = "Invalid --seconds value: " + value;
                return false;
            }
            config.measure_seconds = static_cast<int>(n);
        }
        else if (key == "sample-rate")
        {
            if (!ParseUInt64(value, n) || n == 0)
            {
                error = "Invalid --sample-rate value: " + value;
                return false;
            }
            config.sample_rate = static_cast<size_t>(n);
        }
        else if (key == "csv")
        {
            config.csv_path = value;
        }
        else if (key == "label")
        {
            config.label = value;
        }
        else
        {
            error = "Unknown argument: --" + key;
            return false;
        }
    }

    if (config.allocator != "pool" && config.allocator != "malloc")
    {
        error = "Unsupported allocator: " + config.allocator;
        return false;
    }

    if (config.mode != "immediate" && config.mode != "window")
    {
        error = "Unsupported mode: " + config.mode;
        return false;
    }

    if (config.size_dist != "fixed" && config.size_dist != "mixed")
    {
        error = "Unsupported size distribution: " + config.size_dist;
        return false;
    }

    if (config.allocator == "pool" && config.size_dist == "fixed" && config.size > MAX_BYTES)
    {
        error = "pool allocator only supports size <= MAX_BYTES (256KB).";
        return false;
    }

    return true;
}

static const std::vector<size_t> &MixedSizes()
{
    static const std::vector<size_t> kSizes = {
        8, 16, 32, 64, 128, 256, 512, 1024, 4 * 1024, 16 * 1024, 64 * 1024, 128 * 1024, 256 * 1024};
    return kSizes;
}

static size_t PickSize(const Config &config, uint64_t &rng_state)
{
    if (config.size_dist == "fixed")
    {
        return config.size;
    }

    const std::vector<size_t> &sizes = MixedSizes();
    size_t idx = static_cast<size_t>(XorShift64(rng_state) % sizes.size());
    return sizes[idx];
}

static void *PoolAlloc(size_t size)
{
    return ConcurrentAlloc(size);
}

static void PoolFree(void *ptr, size_t size)
{
    ConcurrentFree(ptr, size);
}

static void *MallocAlloc(size_t size)
{
    return std::malloc(size);
}

static void MallocFree(void *ptr, size_t)
{
    std::free(ptr);
}

static uint64_t PercentileNs(std::vector<uint32_t> &samples, double ratio)
{
    if (samples.empty())
    {
        return 0;
    }
    std::sort(samples.begin(), samples.end());
    const size_t n = samples.size();
    size_t idx = static_cast<size_t>(ratio * static_cast<double>(n - 1));
    if (idx >= n)
    {
        idx = n - 1;
    }
    return static_cast<uint64_t>(samples[idx]);
}

static LatencySummary BuildLatencySummary(std::vector<uint32_t> &samples, uint64_t total_ns, uint64_t ops)
{
    LatencySummary summary;
    summary.samples = samples.size();
    if (ops > 0)
    {
        summary.avg_ns = static_cast<double>(total_ns) / static_cast<double>(ops);
    }
    summary.p50_ns = PercentileNs(samples, 0.50);
    summary.p95_ns = PercentileNs(samples, 0.95);
    summary.p99_ns = PercentileNs(samples, 0.99);
    return summary;
}

static void WorkerMain(size_t worker_id,
                       const Config &config,
                       AllocFn alloc_fn,
                       FreeFn free_fn,
                       std::atomic<size_t> &ready_count,
                       std::atomic<bool> &start_flag,
                       std::atomic<int> &phase, // 0:warmup, 1:measure, 2:stop
                       WorkerStats &out_stats)
{
    WorkerStats stats;
    stats.alloc_samples.reserve(4096);
    stats.free_samples.reserve(4096);

    std::vector<void *> ring_ptrs;
    std::vector<size_t> ring_sizes;
    size_t ring_index = 0;
    if (config.mode == "window")
    {
        ring_ptrs.assign(config.window, nullptr);
        ring_sizes.assign(config.window, 0);
    }

    uint64_t rng_state = 1469598103934665603ULL ^ (worker_id + 1) * 1099511628211ULL;

    ready_count.fetch_add(1, std::memory_order_release);
    while (!start_flag.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }

    while (phase.load(std::memory_order_acquire) != 2)
    {
        int current_phase = phase.load(std::memory_order_relaxed);
        size_t size = PickSize(config, rng_state);

        auto alloc_begin = SteadyClock::now();
        void *ptr = alloc_fn(size);
        auto alloc_end = SteadyClock::now();

        if (ptr != nullptr)
        {
            *reinterpret_cast<volatile char *>(ptr) = static_cast<char>(worker_id);
        }

        bool did_free = false;
        uint64_t free_ns = 0;
        if (config.mode == "immediate")
        {
            auto free_begin = SteadyClock::now();
            free_fn(ptr, size);
            auto free_end = SteadyClock::now();
            free_ns = ToNs(free_end - free_begin);
            did_free = true;
        }
        else
        {
            size_t slot = ring_index % config.window;
            void *old_ptr = ring_ptrs[slot];
            size_t old_size = ring_sizes[slot];
            ring_ptrs[slot] = ptr;
            ring_sizes[slot] = size;
            ++ring_index;

            if (old_ptr != nullptr)
            {
                auto free_begin = SteadyClock::now();
                free_fn(old_ptr, old_size);
                auto free_end = SteadyClock::now();
                free_ns = ToNs(free_end - free_begin);
                did_free = true;
            }
        }

        if (current_phase == 1)
        {
            uint64_t alloc_ns = ToNs(alloc_end - alloc_begin);
            stats.alloc_ops++;
            stats.alloc_ns_total += alloc_ns;

            if (stats.alloc_ops % config.sample_rate == 0)
            {
                uint32_t clipped = alloc_ns > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(alloc_ns);
                stats.alloc_samples.push_back(clipped);
            }

            if (did_free)
            {
                stats.free_ops++;
                stats.free_ns_total += free_ns;
                if (stats.free_ops % config.sample_rate == 0)
                {
                    uint32_t clipped = free_ns > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(free_ns);
                    stats.free_samples.push_back(clipped);
                }
            }
        }
    }

    if (config.mode == "window")
    {
        for (size_t i = 0; i < ring_ptrs.size(); ++i)
        {
            if (ring_ptrs[i] != nullptr)
            {
                free_fn(ring_ptrs[i], ring_sizes[i]);
            }
        }
    }

    out_stats = std::move(stats);
}

static bool AppendCsv(const BenchmarkResult &result, std::string &error)
{
    if (result.config.csv_path.empty())
    {
        return true;
    }

    bool write_header = true;
    {
        std::ifstream existing(result.config.csv_path.c_str(), std::ios::in);
        if (existing.good())
        {
            existing.peek();
            if (!existing.eof())
            {
                write_header = false;
            }
        }
    }

    std::ofstream out(result.config.csv_path.c_str(), std::ios::app);
    if (!out.is_open())
    {
        error = "Failed to open csv file: " + result.config.csv_path;
        return false;
    }

    if (write_header)
    {
        out << "timestamp,label,allocator,mode,size_dist,size,threads,warmup_s,measure_s,window,sample_rate,alloc_ops,free_ops,total_ops,ops_per_sec,alloc_avg_ns,alloc_p50_ns,alloc_p95_ns,alloc_p99_ns,alloc_samples,free_avg_ns,free_p50_ns,free_p95_ns,free_p99_ns,free_samples\n";
    }

    out << result.timestamp_unix << ','
        << result.config.label << ','
        << result.config.allocator << ','
        << result.config.mode << ','
        << result.config.size_dist << ','
        << result.config.size << ','
        << result.config.threads << ','
        << result.config.warmup_seconds << ','
        << result.config.measure_seconds << ','
        << result.config.window << ','
        << result.config.sample_rate << ','
        << result.alloc_ops << ','
        << result.free_ops << ','
        << result.total_ops << ','
        << result.ops_per_sec << ','
        << result.alloc_latency.avg_ns << ','
        << result.alloc_latency.p50_ns << ','
        << result.alloc_latency.p95_ns << ','
        << result.alloc_latency.p99_ns << ','
        << result.alloc_latency.samples << ','
        << result.free_latency.avg_ns << ','
        << result.free_latency.p50_ns << ','
        << result.free_latency.p95_ns << ','
        << result.free_latency.p99_ns << ','
        << result.free_latency.samples << '\n';

    return true;
}

static BenchmarkResult RunBenchmark(const Config &config)
{
    AllocFn alloc_fn = config.allocator == "pool" ? PoolAlloc : MallocAlloc;
    FreeFn free_fn = config.allocator == "pool" ? PoolFree : MallocFree;

    std::vector<std::thread> workers;
    workers.reserve(config.threads);

    std::vector<WorkerStats> worker_stats(config.threads);

    std::atomic<size_t> ready_count(0);
    std::atomic<bool> start_flag(false);
    std::atomic<int> phase(0);

    for (size_t i = 0; i < config.threads; ++i)
    {
        workers.push_back(std::thread(
            WorkerMain,
            i,
            std::ref(config),
            alloc_fn,
            free_fn,
            std::ref(ready_count),
            std::ref(start_flag),
            std::ref(phase),
            std::ref(worker_stats[i])));
    }

    while (ready_count.load(std::memory_order_acquire) != config.threads)
    {
        std::this_thread::yield();
    }

    start_flag.store(true, std::memory_order_release);

    if (config.warmup_seconds > 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(config.warmup_seconds));
    }

    phase.store(1, std::memory_order_release);
    SteadyClock::time_point measured_start = SteadyClock::now();
    std::this_thread::sleep_for(std::chrono::seconds(config.measure_seconds));
    SteadyClock::time_point measured_end = SteadyClock::now();
    phase.store(2, std::memory_order_release);

    for (size_t i = 0; i < workers.size(); ++i)
    {
        workers[i].join();
    }

    BenchmarkResult result;
    result.config = config;
    result.timestamp_unix = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            SystemClock::now().time_since_epoch())
            .count());
    result.measured_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(
                                  measured_end - measured_start)
                                  .count();

    uint64_t alloc_ns_total = 0;
    uint64_t free_ns_total = 0;
    std::vector<uint32_t> alloc_samples;
    std::vector<uint32_t> free_samples;

    for (size_t i = 0; i < worker_stats.size(); ++i)
    {
        result.alloc_ops += worker_stats[i].alloc_ops;
        result.free_ops += worker_stats[i].free_ops;
        alloc_ns_total += worker_stats[i].alloc_ns_total;
        free_ns_total += worker_stats[i].free_ns_total;
        alloc_samples.insert(
            alloc_samples.end(),
            worker_stats[i].alloc_samples.begin(),
            worker_stats[i].alloc_samples.end());
        free_samples.insert(
            free_samples.end(),
            worker_stats[i].free_samples.begin(),
            worker_stats[i].free_samples.end());
    }

    result.total_ops = result.alloc_ops + result.free_ops;
    if (result.measured_seconds > 0.0)
    {
        result.ops_per_sec = static_cast<double>(result.total_ops) / result.measured_seconds;
    }
    result.alloc_latency = BuildLatencySummary(alloc_samples, alloc_ns_total, result.alloc_ops);
    result.free_latency = BuildLatencySummary(free_samples, free_ns_total, result.free_ops);

    return result;
}

static void PrintResult(const BenchmarkResult &result)
{
    std::cout << "=== allocator_bench ===\n";
    std::cout << "label: " << result.config.label << '\n';
    std::cout << "allocator: " << result.config.allocator
              << ", mode: " << result.config.mode
              << ", size_dist: " << result.config.size_dist
              << ", size: " << result.config.size
              << ", threads: " << result.config.threads << '\n';
    std::cout << "warmup_s: " << result.config.warmup_seconds
              << ", measure_s: " << result.config.measure_seconds
              << ", measured_s: " << result.measured_seconds << '\n';
    std::cout << "alloc_ops: " << result.alloc_ops
              << ", free_ops: " << result.free_ops
              << ", total_ops: " << result.total_ops
              << ", throughput_ops_per_sec: " << result.ops_per_sec << '\n';

    std::cout << "alloc_latency_ns(avg/p50/p95/p99): "
              << result.alloc_latency.avg_ns << " / "
              << result.alloc_latency.p50_ns << " / "
              << result.alloc_latency.p95_ns << " / "
              << result.alloc_latency.p99_ns
              << " (samples=" << result.alloc_latency.samples << ")\n";

    std::cout << "free_latency_ns(avg/p50/p95/p99): "
              << result.free_latency.avg_ns << " / "
              << result.free_latency.p50_ns << " / "
              << result.free_latency.p95_ns << " / "
              << result.free_latency.p99_ns
              << " (samples=" << result.free_latency.samples << ")\n";
}
} // namespace

int main(int argc, char **argv)
{
    Config config;
    std::string error;
    if (!ParseArgs(argc, argv, config, error))
    {
        std::cerr << "Argument error: " << error << '\n';
        PrintUsage(argv[0]);
        return 1;
    }

    BenchmarkResult result = RunBenchmark(config);
    PrintResult(result);

    if (!AppendCsv(result, error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    return 0;
}
