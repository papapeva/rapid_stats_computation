// просто проверка работы алгосов
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

#include "order_statistic_algorithm.hpp"
#include "floyd_rivest.hpp"
#include "random_sampling.hpp"
#include "multiple_quickselect.hpp"

using namespace rapid_stats;

namespace {

using Data = std::vector<int>;

const std::vector<std::size_t> ranks = {
    1, 10, 50, 100, 250, 500, 750, 900, 999, 1000,
    500, 10, 250
};

Data make_data(std::size_t size, std::uint64_t seed) {
    Data data(size);
    std::iota(data.begin(), data.end(), 1);

    std::mt19937_64 rng(seed);
    std::shuffle(data.begin(), data.end(), rng);

    return data;
}

Data make_reference(const Data& data) {
    Data reference = data;
    std::sort(reference.begin(), reference.end());
    return reference;
}

// Проверяет точный алгоритм, возвращающий итераторы.
template <typename Algorithm>
void test_exact_iterator_algorithm(
    const char* name,
    Algorithm& algorithm,
    const Data& source,
    const Data& reference)
{
    Data data = source;
    const auto result = algorithm.sort(data, ranks);

    assert(result.size() == ranks.size());

    for (std::size_t i = 0; i < ranks.size(); ++i) {
        const int expected = reference[ranks[i] - 1];
        const int actual = *result[i];

        if (actual != expected) {
            throw std::runtime_error(
                std::string(name) + ": incorrect result at rank " +
                std::to_string(ranks[i]));
        }
    }

    std::cout << "[OK] " << name << '\n';
}

// Проверяет приближённый алгоритм.
// Для каждого результата проверяем, что ранг значения
// находится не дальше eps * n от запрошенного ранга.
template <typename Algorithm>
void test_approximate_algorithm(
    const char* name,
    Algorithm& algorithm,
    const Data& source,
    const Data& reference,
    double eps,
    double delta,
    std::uint64_t seed)
{
    Data data = source;
    const auto result = algorithm.sort(data, ranks, eps, delta, seed);

    assert(result.size() == ranks.size());

    const std::size_t n = source.size();
    const std::size_t max_error =
        static_cast<std::size_t>(std::ceil(eps * n));

    for (std::size_t i = 0; i < ranks.size(); ++i) {
        const int actual = *result[i];

        const auto it = std::lower_bound(
            reference.begin(), reference.end(), actual);

        if (it == reference.end() || *it != actual) {
            throw std::runtime_error(
                std::string(name) + ": returned value is not in input");
        }

        const std::size_t actual_rank =
            static_cast<std::size_t>(
                std::distance(reference.begin(), it)) + 1;

        const std::size_t expected_rank = ranks[i];

        const std::size_t error =
            actual_rank > expected_rank
                ? actual_rank - expected_rank
                : expected_rank - actual_rank;

        if (error > max_error) {
            throw std::runtime_error(
                std::string(name) + ": rank error exceeds eps * n");
        }
    }

    std::cout << "[OK] " << name << '\n';
}

// MultipleQuickselectAlgorithm возвращает значения,
// а не итераторы.
void test_multiple_quickselect(
    const Data& source,
    const Data& reference)
{
    Data data = source;

    MultipleQuickselectAlgorithm<Data> algorithm;
    const auto result = algorithm.sort(data, ranks, 42);

    assert(result.size() == ranks.size());

    for (std::size_t i = 0; i < ranks.size(); ++i) {
        const int expected = reference[ranks[i] - 1];

        if (result[i] != expected) {
            throw std::runtime_error(
                "MultipleQuickselect: incorrect result at rank " +
                std::to_string(ranks[i]));
        }
    }

    std::cout << "[OK] MultipleQuickselectAlgorithm\n";
}

} // namespace

int main() {
    constexpr std::size_t data_size = 1000;
    constexpr std::uint64_t data_seed = 12345;

    const Data source = make_data(data_size, data_seed);
    const Data reference = make_reference(source);

    try {
        // Floyd–Rivest: точный алгоритм.
        FloydRivestSelect<Data> floyd_rivest;
        test_exact_iterator_algorithm(
            "FloydRivestSelect",
            floyd_rivest,
            source,
            reference);

        // Random Sampling: приближённый алгоритм.
        RandomSamplingAlgorithm<Data> random_sampling;
        test_approximate_algorithm(
            "RandomSamplingAlgorithm",
            random_sampling,
            source,
            reference,
            0.1,   // eps
            0.05,  // delta
            42);   // seed

        // Multiple Quickselect: точный поиск нескольких статистик.
        test_multiple_quickselect(source, reference);

    } catch (const std::exception& e) {
        std::cerr << "[FAILED] " << e.what() << '\n';
        return 1;
    }

    std::cout << "\nAll tests passed.\n";
    return 0;
}
