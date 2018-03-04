#include <benchmark/benchmark.h>
#include <buffer_pool.hpp>
#include <gsl.hpp>

using span_t = gsl::span<uint8_t>;

uint8_t mem[4096] = {0};


static void BM_RequestRelease(benchmark::State& state)
{
    buffer_pool<span_t> pool(span_t(mem, sizeof(mem)));
    for (auto _ : state)
    {
        auto c = pool.request(state.range(0));
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_RequestRelease)->Range(1, 1024);


BENCHMARK_MAIN();
