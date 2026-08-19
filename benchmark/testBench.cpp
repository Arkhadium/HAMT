#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "hamt.hpp"

namespace bench
{
using Clock = std::chrono::steady_clock;
using Nanoseconds = std::chrono::nanoseconds;

constexpr std::uint64_t kSeed = 0xC0FFEE123456789ULL;
constexpr std::size_t kWarmupRuns = 3;
constexpr std::size_t kMeasuredRuns = 30;
constexpr std::size_t kFragmentationAllocations = 10'000;

constexpr std::size_t kSmallMin = 16;
constexpr std::size_t kSmallMax = 256;
constexpr std::size_t kMediumMin = 257;
constexpr std::size_t kMediumMax = 4 * 1024;
constexpr std::size_t kLargeMin = 4 * 1024 + 1;
constexpr std::size_t kLargeMax = 64 * 1024;

const std::vector<std::size_t> kSizes{ 1'000, 10'000, 100'000, 1'000'000 };

struct FragmentedHeap
{
    std::vector<std::byte*> liveBlocks;

    FragmentedHeap() = default;
    FragmentedHeap(const FragmentedHeap&) = delete;
    FragmentedHeap& operator=(const FragmentedHeap&) = delete;

    FragmentedHeap(FragmentedHeap&& other) noexcept
        : liveBlocks(std::move(other.liveBlocks))
    {
        other.liveBlocks.clear();
    }

    FragmentedHeap& operator=(FragmentedHeap&& other) noexcept
    {
        if (this != &other)
        {
            release();
            liveBlocks = std::move(other.liveBlocks);
            other.liveBlocks.clear();
        }
        return *this;
    }

    ~FragmentedHeap()
    {
        release();
    }

private:
    void release() noexcept
    {
        for (auto* ptr : liveBlocks)
            delete[] ptr;
        liveBlocks.clear();
    }
};

std::size_t randomAllocationSize(std::mt19937_64& rng)
{
    // 70% petites allocations, 20% moyennes, 10% grosses.
    std::uniform_int_distribution<int> bucketDist(0, 99);
    const int bucket = bucketDist(rng);

    if (bucket < 70)
    {
        std::uniform_int_distribution<std::size_t> dist(kSmallMin, kSmallMax);
        return dist(rng);
    }

    if (bucket < 90)
    {
        std::uniform_int_distribution<std::size_t> dist(kMediumMin, kMediumMax);
        return dist(rng);
    }

    std::uniform_int_distribution<std::size_t> dist(kLargeMin, kLargeMax);
    return dist(rng);
}

FragmentedHeap fragmentHeap(std::mt19937_64& rng)
{
    FragmentedHeap fragmented;
    fragmented.liveBlocks.reserve(kFragmentationAllocations / 2);

    std::bernoulli_distribution freeImmediately(0.5);

    for (std::size_t i = 0; i < kFragmentationAllocations; ++i)
    {
        const std::size_t size = randomAllocationSize(rng);
        auto* ptr = new std::byte[size];

        // On touche réellement la mémoire.
        ptr[0] = std::byte{ 0x1 };
        ptr[size - 1] = std::byte{ 0x2 };

        if (freeImmediately(rng))
            delete[] ptr;
        else
            fragmented.liveBlocks.push_back(ptr);
    }

    return fragmented;
}

struct Stats
{
    double minNsPerOp = 0.0;
    double medianNsPerOp = 0.0;
    double meanNsPerOp = 0.0;
    double maxNsPerOp = 0.0;
};

Stats computeStats(std::vector<double> values)
{
    std::sort(values.begin(), values.end());

    Stats stats;
    stats.minNsPerOp = values.front();
    stats.maxNsPerOp = values.back();
    stats.meanNsPerOp = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());

    const std::size_t mid = values.size() / 2;
    stats.medianNsPerOp = (values.size() % 2 == 0) ? (values[mid - 1] + values[mid]) / 2.0 : values[mid];

    return stats;
}

void printStats(const std::string& name, std::size_t size, const Stats& stats)
{
    std::cout
        << std::left
        << std::setw(18)
        << name
        << " N="
        << std::setw(9)
        << size
        << " min="
        << std::setw(10)
        << std::fixed
        << std::setprecision(2)
        << stats.minNsPerOp
        << " median="
        << std::setw(10)
        << stats.medianNsPerOp
        << " mean="
        << std::setw(10)
        << stats.meanNsPerOp
        << " max="
        << std::setw(10)
        << stats.maxNsPerOp
        << " ns/op\n";
}

template <typename T>
inline void doNotOptimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(&value) : "memory");
#else
    const volatile void* sink = static_cast<const void*>(&value);
    (void)sink;
#endif
}

std::vector<int> makePresentKeys(std::size_t n, std::mt19937_64& rng)
{
    std::vector<int> keys(n);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), rng);
    return keys;
}

std::vector<int> makeMixedKeys(std::size_t n, std::mt19937_64& rng)
{
    std::vector<int> keys;
    keys.reserve(n);

    const std::size_t hits = n / 2;
    const std::size_t misses = n - hits;

    for (std::size_t i = 0; i < hits; ++i)
        keys.push_back(static_cast<int>(i));

    for (std::size_t i = 0; i < misses; ++i)
        keys.push_back(static_cast<int>(n + i + 1));

    std::shuffle(keys.begin(), keys.end(), rng);
    return keys;
}

template <typename Container>
void fillContainer(Container& container, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
        container.emplace(static_cast<int>(i), static_cast<int>(i));
}

template <typename Container, typename Operation>
Stats runMeasured(std::size_t n, std::uint64_t seed, Operation&& operation)
{
    std::vector<double> samples;
    samples.reserve(kMeasuredRuns);

    for (std::size_t run = 0; run < kWarmupRuns + kMeasuredRuns; ++run)
    {
        std::mt19937_64 rng(seed + run);

        // Hors chrono ; les blocs survivants restent alloués pendant la mesure.
        auto fragmentedHeap = fragmentHeap(rng);

        const auto elapsed = operation(rng);

        if (run >= kWarmupRuns)
        {
            samples.push_back(static_cast<double>(elapsed.count()) / static_cast<double>(n));
        }
    }

    return computeStats(std::move(samples));
}

template <typename Container>
Stats benchmarkEmplace(std::size_t n, std::uint64_t seed)
{
    return runMeasured<Container>(n,
                                  seed,
                                  [n](std::mt19937_64& rng) -> Nanoseconds
                                  {
                                      auto keys = makePresentKeys(n, rng);
                                      Container container;

                                      const auto start = Clock::now();
                                      for (int key : keys)
                                          container.emplace(key, key);
                                      const auto stop = Clock::now();

                                      doNotOptimize(container);
                                      return std::chrono::duration_cast<Nanoseconds>(stop - start);
                                  });
}

template <typename Container>
Stats benchmarkFindHit(std::size_t n, std::uint64_t seed)
{
    return runMeasured<Container>(n,
                                  seed,
                                  [n](std::mt19937_64& rng) -> Nanoseconds
                                  {
                                      Container container;
                                      fillContainer(container, n);
                                      auto keys = makePresentKeys(n, rng);

                                      std::size_t hits = 0;

                                      const auto start = Clock::now();
                                      for (int key : keys)
                                      {
                                          const auto it = container.find(key);
                                          if (it != container.end())
                                              ++hits;
                                      }
                                      const auto stop = Clock::now();

                                      doNotOptimize(hits);
                                      return std::chrono::duration_cast<Nanoseconds>(stop - start);
                                  });
}

template <typename Container>
Stats benchmarkFindMixed(std::size_t n, std::uint64_t seed)
{
    return runMeasured<Container>(n,
                                  seed,
                                  [n](std::mt19937_64& rng) -> Nanoseconds
                                  {
                                      Container container;
                                      fillContainer(container, n);
                                      auto keys = makeMixedKeys(n, rng);

                                      std::size_t hits = 0;

                                      const auto start = Clock::now();
                                      for (int key : keys)
                                      {
                                          const auto it = container.find(key);
                                          if (it != container.end())
                                              ++hits;
                                      }
                                      const auto stop = Clock::now();

                                      doNotOptimize(hits);
                                      return std::chrono::duration_cast<Nanoseconds>(stop - start);
                                  });
}

template <typename Container>
Stats benchmarkEraseHit(std::size_t n, std::uint64_t seed)
{
    return runMeasured<Container>(n,
                                  seed,
                                  [n](std::mt19937_64& rng) -> Nanoseconds
                                  {
                                      Container container;
                                      fillContainer(container, n);
                                      auto keys = makePresentKeys(n, rng);

                                      std::size_t erased = 0;

                                      const auto start = Clock::now();
                                      for (int key : keys)
                                          erased += static_cast<std::size_t>(container.erase(key));
                                      const auto stop = Clock::now();

                                      doNotOptimize(erased);
                                      return std::chrono::duration_cast<Nanoseconds>(stop - start);
                                  });
}

template <typename Container>
Stats benchmarkEraseMixed(std::size_t n, std::uint64_t seed)
{
    return runMeasured<Container>(n,
                                  seed,
                                  [n](std::mt19937_64& rng) -> Nanoseconds
                                  {
                                      Container container;
                                      fillContainer(container, n);
                                      auto keys = makeMixedKeys(n, rng);

                                      std::size_t erased = 0;

                                      const auto start = Clock::now();
                                      for (int key : keys)
                                          erased += static_cast<std::size_t>(container.erase(key));
                                      const auto stop = Clock::now();

                                      doNotOptimize(erased);
                                      return std::chrono::duration_cast<Nanoseconds>(stop - start);
                                  });
}

template <typename Container>
void benchmarkContainer(const std::string& name)
{
    std::cout << "\n============================================================\n";
    std::cout << name << '\n';
    std::cout
        << "warmups="
        << kWarmupRuns
        << ", measured runs="
        << kMeasuredRuns
        << ", fragmentation allocations="
        << kFragmentationAllocations
        << '\n';
    std::cout << "============================================================\n";

    for (const std::size_t n : kSizes)
    {
        std::cout << "\n--- N = " << n << " ---\n";

        printStats("emplace", n, benchmarkEmplace<Container>(n, kSeed + 0x1000));

        printStats("find_hit", n, benchmarkFindHit<Container>(n, kSeed + 0x2000));

        printStats("find_mixed", n, benchmarkFindMixed<Container>(n, kSeed + 0x3000));

        printStats("erase_hit", n, benchmarkEraseHit<Container>(n, kSeed + 0x4000));

        printStats("erase_mixed", n, benchmarkEraseMixed<Container>(n, kSeed + 0x5000));
    }
}
} // namespace bench

int main()
{
    std::cout
        << "HAMT benchmark - std::chrono::steady_clock\n"
        << "Toutes les valeurs sont en nanosecondes par operation.\n"
        << "La fragmentation du heap est effectuee avant chaque run "
           "et n'est pas chronometree.\n";

    bench::benchmarkContainer<hamt::Hamt<int, int>>("hamt::Hamt<int, int>");

    return 0;
}
