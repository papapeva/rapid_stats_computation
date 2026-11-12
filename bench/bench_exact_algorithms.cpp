#include "bfprt.hpp"
#include "order_statistic_benchmark.hpp"
#include "partial_quicksort.hpp"
#include "radix_select.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <vector>

namespace {

// Обёртка стандартного nth_element. В бенчмарк не входит: передаётся туда как обычный алгоритм.
template <typename Container>
class NthElementSelect : public rapid_stats::ExactOrderStatisticAlgorithm<Container> {
public:
    using iterator = typename rapid_stats::ExactOrderStatisticAlgorithm<Container>::iterator;

    std::vector<iterator> sort(Container& data, const std::vector<std::size_t>& ranks) override {
        /*
         * Автор: Viktor Hlyupin
         * Принимает: data — выборка, ranks — ранги от 1 до n.
         * Возвращает: итераторы статистик. Уже выбранные позиции дальше не двигаются.
         */
        std::vector<std::size_t> order = ranks;
        std::sort(order.begin(), order.end());
        order.erase(std::unique(order.begin(), order.end()), order.end());

        auto from = std::begin(data);
        for (const std::size_t rank : order) {
            auto nth = std::begin(data) + static_cast<std::ptrdiff_t>(rank - 1);
            std::nth_element(from, nth, std::end(data));
            from = std::next(nth);
        }

        std::vector<iterator> result;
        result.reserve(ranks.size());
        for (const std::size_t rank : ranks) {
            result.push_back(std::begin(data) + static_cast<std::ptrdiff_t>(rank - 1));
        }
        return result;
    }
};

struct SampleCase {
    const char* name;
    std::size_t size;
    rapid_stats::SamplePattern pattern;
    std::vector<double> quantiles;
    std::size_t repeats;
};

template <typename Algorithm>
bool report(const char* name, Algorithm& algorithm,
            const rapid_stats::OrderStatisticBenchmark& benchmark, const SampleCase& sample,
            const std::vector<std::int64_t>& expected) {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: name — подпись, algorithm — алгоритм, benchmark — выборка, sample — запуск, expected — эталон.
     * Возвращает: истину, если значения последнего запуска совпали с nth_element.
     */
    const auto result = benchmark.exact(algorithm, sample.quantiles, sample.repeats);
    const bool same = result.values == expected;
    std::cout << "  " << name << "  " << result.median_ms << " мс";
    if (!same) {
        std::cout << ", значения разошлись";
    }
    std::cout << "\n";
    return same;
}

bool run_case(const SampleCase& sample) {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: sample — вид выборки, квантили и число запусков.
     * Возвращает: истину, если BFPRT, Radix Select и Partial Quicksort совпали с nth_element.
     */
    rapid_stats::OrderStatisticBenchmark benchmark(sample.size, sample.pattern, 1);
    NthElementSelect<rapid_stats::BenchmarkSample> baseline;
    rapid_stats::BfprtAlgorithm<rapid_stats::BenchmarkSample> bfprt;
    rapid_stats::RadixSelectAlgorithm<rapid_stats::BenchmarkSample> radix;
    rapid_stats::PartialQuicksortAlgorithm<rapid_stats::BenchmarkSample> partial;

    const auto reference = benchmark.exact(baseline, sample.quantiles, sample.repeats);
    std::cout << sample.name << ", n = " << sample.size << "\n";
    const bool bfprt_ok = report("bfprt              ", bfprt, benchmark, sample, reference.values);
    const bool radix_ok = report("radix_select       ", radix, benchmark, sample, reference.values);
    const bool partial_ok =
        report("partial_quicksort  ", partial, benchmark, sample, reference.values);
    const bool ok = bfprt_ok && radix_ok && partial_ok;
    std::cout << "  nth_element        " << reference.median_ms << " мс\n  "
              << (ok ? "значения совпали" : "значения разошлись") << "\n";
    return ok;
}

} // namespace

int main() {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: ничего.
     * Возвращает: 0, если ответы трёх точных алгоритмов совпали с nth_element.
     */
    const std::vector<double> quantiles = {0, 0.01, 0.25, 0.5, 0.75, 0.99, 1};
    const SampleCase cases[] = {
        {"случайный", 100000, rapid_stats::SamplePattern::Random, quantiles, 3},
        {"по возрастанию", 20000, rapid_stats::SamplePattern::Ascending, quantiles, 1},
        {"по убыванию", 20000, rapid_stats::SamplePattern::Descending, quantiles, 1},
        {"почти отсортированный", 20000, rapid_stats::SamplePattern::AlmostSorted, quantiles, 1},
        {"много повторов", 20000, rapid_stats::SamplePattern::ManyDuplicates, quantiles, 1},
        {"все равны", 20000, rapid_stats::SamplePattern::AllEqual, quantiles, 1},
        {"два значения поровну", 20000, rapid_stats::SamplePattern::TwoValuesBalanced, quantiles,
         1},
        {"два значения, перекос", 20000, rapid_stats::SamplePattern::TwoValuesSkewed, quantiles, 1},
        {"вверх и вниз", 20000, rapid_stats::SamplePattern::UpThenDown, quantiles, 1},
        {"весь int64", 20000, rapid_stats::SamplePattern::FullInt64, quantiles, 1},
        {"общие старшие байты", 20000, rapid_stats::SamplePattern::SameHighBytes, quantiles, 1},
        {"младший байт", 20000, rapid_stats::SamplePattern::LowByteOnly, quantiles, 1},
        {"один квантиль 0.75", 20000, rapid_stats::SamplePattern::Random, {0.75}, 3},
        {"пустой запрос", 1000, rapid_stats::SamplePattern::Random, {}, 1},
    };

    bool ok = true;
    for (const SampleCase& sample : cases) {
        ok = run_case(sample) && ok;
    }
    return ok ? 0 : 1;
}
