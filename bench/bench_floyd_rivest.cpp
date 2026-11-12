#include "floyd_rivest.hpp"
#include "order_statistic_benchmark.hpp"

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

} // namespace

int main() {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: ничего.
     * Возвращает: 0, если ответы Floyd–Rivest совпали с nth_element.
     */
    rapid_stats::FloydRivestSelect<rapid_stats::BenchmarkSample> algorithm;
    NthElementSelect<rapid_stats::BenchmarkSample> baseline;
    const std::vector<double> quantiles = {0, 0.01, 0.25, 0.5, 0.75, 0.99, 1};
    bool ok = true;

    rapid_stats::OrderStatisticBenchmark benchmark(100000, rapid_stats::SamplePattern::Random, 1);
    const auto result = benchmark.exact(algorithm, quantiles, 5);
    const auto reference = benchmark.exact(baseline, quantiles, 5);
    ok = result.values == reference.values;
    std::cout << "случайный, n = 100000\n  floyd_rivest  " << result.median_ms
              << " мс\n  nth_element   " << reference.median_ms << " мс\n  "
              << (ok ? "значения совпали" : "значения разошлись") << "\n";

    rapid_stats::OrderStatisticBenchmark larger(1000000, rapid_stats::SamplePattern::Random, 1);
    const auto again = larger.exact(algorithm, quantiles, 5);
    const auto again_base = larger.exact(baseline, quantiles, 5);
    const bool same = again.values == again_base.values;
    ok = ok && same;
    std::cout << "случайный, n = 1000000\n  floyd_rivest  " << again.median_ms
              << " мс\n  nth_element   " << again_base.median_ms << " мс\n  "
              << (same ? "значения совпали" : "значения разошлись") << "\n";

    const auto one = benchmark.exact(algorithm, {0.75}, 5);
    const auto one_base = benchmark.exact(baseline, {0.75}, 5);
    const bool one_same = one.values == one_base.values;
    ok = ok && one_same;
    std::cout << "только 0.75, n = 100000\n  floyd_rivest  " << one.median_ms << " мс, значение "
              << one.values.front() << "\n  nth_element   " << one_base.median_ms
              << " мс, значение " << one_base.values.front() << "\n  "
              << (one_same ? "значения совпали" : "значения разошлись") << "\n";

    return ok ? 0 : 1;
}
