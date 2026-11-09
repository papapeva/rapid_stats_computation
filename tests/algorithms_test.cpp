#include "algo/bfprt.hpp"
#include "algo/partial_quicksort.hpp"
#include "algo/radix_select.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <numeric>

namespace {
std::size_t checks = 0;

void require(bool condition, const char* message) {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: проверяемое условие и сообщение о его нарушении.
     * Возвращает: ничего; при ошибке бросает исключение, в том числе в Release.
     */
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T> std::vector<std::uint64_t> representations(const std::vector<T>& data) {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: вектор чисел до 64 бит, включая оба знака нуля.
     * Возвращает: отсортированные битовые представления для проверки сохранности значений.
     */
    static_assert(sizeof(T) <= sizeof(std::uint64_t), "test data up to 64 bits required");
    std::vector<std::uint64_t> result;
    for (const auto value : data) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(value));
        result.push_back(bits);
    }
    std::sort(result.begin(), result.end());
    return result;
}

template <template <typename> class Algorithm, typename T>
void check(const std::vector<T>& original, const std::vector<std::size_t>& ranks) {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: исходный массив и запрос рангов для проверяемого класса алгоритма.
     * Возвращает: ничего; проверяет полиморфный вызов, порядок результатов, повторы и повторный вызов.
     */
    auto data = original;
    auto sorted = original;
    std::sort(sorted.begin(), sorted.end());
    Algorithm<std::vector<T>> implementation;
    rapid_stats::ExactOrderStatisticAlgorithm<std::vector<T>>& base = implementation;
    for (int repeat = 0; repeat < 2; ++repeat) {
        const auto results = base.sort(data, ranks);
        require(results.size() == ranks.size(), "wrong result count");
        for (std::size_t i = 0; i < ranks.size(); ++i) {
            require(results[i] == data.begin() + static_cast<std::ptrdiff_t>(ranks[i] - 1),
                    "iterator does not point to requested rank position");
            require(results[i] != data.end(), "end iterator returned");
            for (auto it = data.begin(); it != results[i]; ++it) {
                require(*it <= *results[i], "larger value before selected position");
            }
            for (auto it = results[i] + 1; it != data.end(); ++it) {
                require(*it >= *results[i], "smaller value after selected position");
            }
            require(*results[i] == sorted[ranks[i] - 1], "incorrect order statistic");
        }
        if (ranks.empty()) {
            require(data == original, "empty query reordered input");
        }
        require(representations(data) == representations(original), "input values lost or changed");
        ++checks;
    }
}

template <typename T>
void check_all_algorithms(const std::vector<T>& data, const std::vector<std::size_t>& ranks) {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: одни и те же данные и ранги с единицы для трёх алгоритмов.
     * Возвращает: ничего; сравнивает каждый результат с независимо отсортированным эталоном.
     */
    check<rapid_stats::BfprtAlgorithm>(data, ranks);
    check<rapid_stats::RadixSelectAlgorithm>(data, ranks);
    check<rapid_stats::PartialQuicksortAlgorithm>(data, ranks);
}

template <typename T> void check_prefix(const std::vector<T>& original, std::size_t count) {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: исходные данные и число минимальных элементов для частичной сортировки.
     * Возвращает: ничего; проверяет весь префикс, хвост и неизменность данных при count=0.
     */
    auto data = original;
    auto sorted = original;
    std::sort(sorted.begin(), sorted.end());
    rapid_stats::PartialQuicksortAlgorithm<std::vector<T>> algorithm;
    algorithm.sort_prefix(data, count);
    for (std::size_t i = 0; i < count; ++i) {
        require(data[i] == sorted[i], "partial prefix is incorrect");
    }
    if (count != 0) {
        for (std::size_t i = count; i < data.size(); ++i) {
            require(data[i] >= data[count - 1], "partial tail is incorrect");
        }
    } else {
        require(data == original, "empty prefix modified input");
    }
    require(representations(data) == representations(original), "partial sort changed values");
    ++checks;
}

template <template <typename> class Algorithm> void check_errors() {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: класс алгоритма через шаблонный параметр.
     * Возвращает: ничего; проверяет ошибки рангов и неконечных значений до модификации массива.
     */
    std::vector<double> data{3, 1, 2};
    Algorithm<std::vector<double>> algorithm;
    bool caught = false;
    try {
        algorithm.sort(data, {1, data.size() + 1});
    } catch (const std::out_of_range&) {
        caught = true;
    }
    require(caught && data == std::vector<double>({3, 1, 2}), "invalid ranks mutated input");
    caught = false;
    try {
        algorithm.sort(data, {1, 0});
    } catch (const std::out_of_range&) {
        caught = true;
    }
    require(caught && data == std::vector<double>({3, 1, 2}),
            "zero rank accepted or mutated input");
    std::vector<double> other{9, -2};
    const auto other_results = algorithm.sort(other, {2, 1});
    require(*other_results[0] == 9 && *other_results[1] == -2, "cannot reuse on another array");
    for (double bad :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity()}) {
        data.back() = bad;
        const auto before = representations(data);
        caught = false;
        try {
            algorithm.sort(data, {1});
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        require(caught && representations(data) == before, "nonfinite input accepted or changed");
        require(algorithm.sort(data, {}).empty(), "empty query must be a no-op");
    }
    std::vector<double> empty;
    Algorithm<std::vector<double>> empty_algorithm;
    require(empty_algorithm.sort(empty, {}).empty(), "empty query on empty data");
    caught = false;
    try {
        empty_algorithm.sort(empty, {1});
    } catch (const std::out_of_range&) {
        caught = true;
    }
    require(caught, "rank on empty array must throw");
}

template <typename T> void suite() {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: числовой тип через шаблонный параметр.
     * Возвращает: ничего; проверяет различные размеры, распределения и ранги, включая первый и последний.
     */
    check_all_algorithms<T>({}, {});
    check_all_algorithms<T>({T{5}}, {1, 1});
    std::mt19937_64 rng(20261105);
    for (std::size_t n : {2U, 5U, 25U, 126U, 1000U, 10000U}) {
        std::vector<T> data(n);
        for (auto& value : data) {
            if constexpr (std::is_unsigned_v<T>) {
                /*
     * Автор: Polikarpov Kirill
     * Принимает: ветвь для выбранного числового типа.
     * Возвращает: ничего; это блок if constexpr.
     */
                value = rng();
            } else {
                value = static_cast<T>(static_cast<std::int64_t>(rng() % 20001) - 10000);
            }
        }
        for (int pattern = 0; pattern < 5; ++pattern) {
            if (pattern == 1) {
                std::sort(data.begin(), data.end());
            } else if (pattern == 2) {
                std::reverse(data.begin(), data.end());
            } else if (pattern == 3) {
                for (auto& value : data) {
                    value = static_cast<T>(rng() % 3);
                }
            } else if (pattern == 4) {
                std::fill(data.begin(), data.end(), T{7});
            }
            check_all_algorithms(data, {n, 1, n / 2 + 1, 1, n});
            check_all_algorithms(data, {});
            if (n <= 126) {
                std::vector<std::size_t> ranks(n);
                std::iota(ranks.begin(), ranks.end(), 1);
                std::reverse(ranks.begin(), ranks.end());
                check_all_algorithms(data, ranks);
            }
            for (auto count : {std::size_t{0}, std::size_t{1}, n / 2, n}) {
                check_prefix(data, count);
            }
        }
    }
}

void extended_cases() {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: ничего; seed фиксирован для воспроизводимости.
     * Возвращает: ничего; проверяет дробные числа, границы типов и отдельные ранги.
     */
    std::mt19937_64 rng(20261110);
    for (int trial = 0; trial < 120; ++trial) {
        const std::size_t n = 1 + rng() % 300;
        std::vector<double> data;
        while (data.size() < n) {
            const auto bits = rng();
            double value = 0;
            std::memcpy(&value, &bits, sizeof(value));
            if (std::isfinite(value)) {
                data.push_back(value);
            }
        }
        const std::size_t rank = 1 + rng() % n;
        check_all_algorithms(data, {rank});
        check_all_algorithms(data, {n, rank, 1, rank});
        auto nth = data;
        std::nth_element(nth.begin(), nth.begin() + static_cast<std::ptrdiff_t>(rank - 1),
                         nth.end());
        auto sorted = data;
        std::sort(sorted.begin(), sorted.end());
        require(nth[rank - 1] == sorted[rank - 1], "reference algorithms disagree");
        check_prefix(data, rank);
        ++checks;
    }
    check_all_algorithms<double>({-0.0, 1.0, +0.0, -1.0, -0.0, +0.0}, {6, 2, 3, 4, 5, 2});
    check<rapid_stats::BfprtAlgorithm, int>({INT_MIN, 4, -7, INT_MAX, 4}, {5, 1, 3});
    check<rapid_stats::PartialQuicksortAlgorithm, int>({INT_MIN, 4, -7, INT_MAX, 4}, {5, 1, 3});
    check<rapid_stats::BfprtAlgorithm, float>({-0.5F, 2.25F, -9.75F, 2.25F}, {4, 1, 2});
    check<rapid_stats::PartialQuicksortAlgorithm, float>({-0.5F, 2.25F, -9.75F, 2.25F}, {4, 1, 2});
}

void prefix_error_cases() {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: ничего.
     * Возвращает: ничего; проверяет неверный размер префикса и NaN без изменения массива.
     */
    rapid_stats::PartialQuicksortAlgorithm<std::vector<double>> algorithm;
    std::vector<double> data{3, 1, 2};
    bool caught = false;
    try {
        algorithm.sort_prefix(data, data.size() + 1);
    } catch (const std::out_of_range&) {
        caught = true;
    }
    require(caught && data == std::vector<double>({3, 1, 2}), "invalid prefix mutated input");
    data.back() = std::numeric_limits<double>::quiet_NaN();
    const auto before = representations(data);
    algorithm.sort_prefix(data, 0);
    require(representations(data) == before, "zero prefix changed nonfinite input");
    caught = false;
    try {
        algorithm.sort_prefix(data, 1);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    require(caught && representations(data) == before, "nonfinite prefix input accepted");
}

void radix_multi_cases() {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: ничего; проверяет стоимость общей обработки рангов без замеров времени.
     * Возвращает: ничего; проверяет границы работы, повторные ранги и пропуск ненужных корзин.
     */
    std::vector<std::uint64_t> data(4096);
    std::mt19937_64 rng(20261110);
    for (auto& value : data) {
        value = rng();
    }
    auto sorted = data;
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::size_t> positions(data.size());
    std::iota(positions.begin(), positions.end(), 0);
    rapid_stats::detail::radix::WorkStats stats;
    rapid_stats::detail::radix::select_positions(data, positions, &stats);
    require(data == sorted, "radix all-ranks selection is incorrect");
    require(stats.nodes[7] == 1, "radix rescanned root for separate ranks");
    for (std::size_t t = 0; t < 8; ++t) {
        require(stats.counted[t] <= data.size(), "radix counted data repeatedly at one level");
        require(stats.distributed[t] <= data.size(),
                "radix redistributed data repeatedly at one level");
    }
    // Все элементы в одной корзине старшего байта; запрос максимума требует
    // пройти общий путь один раз, несмотря на 100 повторяющихся запросов.
    data = {3, 1, 2, 0, 7, 5, 6, 4};
    rapid_stats::RadixSelectAlgorithm<std::vector<std::uint64_t>> algorithm;
    const std::vector<std::size_t> repeated(100, 8);
    const auto result = algorithm.sort(data, repeated);
    require(result.size() == repeated.size(), "radix dropped duplicate queries");
    for (auto it : result) {
        require(it == data.begin() + 7 && *it == 7, "wrong duplicate query iterator");
    }
    // У выбранной корзины только один элемент. Остальные имеют общий старший
    // байт и различаются младшими: их внутренняя несортированность допустима.
    data = {3, 1, 2, std::uint64_t{1} << 63};
    const auto max_result = algorithm.sort(data, {4});
    require(max_result[0] == data.begin() + 3, "wrong maximum position");
    require(!std::is_sorted(data.begin(), data.end()),
            "radix unnecessarily sorted unrequested bucket");
    ++checks;
}
} // namespace

int main() {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: ничего, тесты полностью воспроизводимы и не требуют пользовательского ввода.
     * Возвращает: 0 при успехе, 1 при нарушении контракта любого алгоритма.
     */
    try {
        radix_multi_cases();
        extended_cases();
        prefix_error_cases();
        suite<std::int64_t>();
        suite<std::uint64_t>();
        suite<double>();
        // Исчерпывающий перебор малых массивов с повторами и отрицательными числами.
        for (std::size_t n = 1; n <= 7; ++n) {
            std::size_t total = 1;
            for (std::size_t i = 0; i < n; ++i) {
                total *= 3;
            }
            for (std::size_t code = 0; code < total; ++code) {
                auto number = code;
                std::vector<std::int64_t> data(n);
                std::vector<std::size_t> ranks(n);
                for (std::size_t i = 0; i < n; ++i) {
                    data[i] = static_cast<std::int64_t>(number % 3) - 1;
                    number /= 3;
                    ranks[i] = n - i;
                }
                check_all_algorithms(data, ranks);
                for (std::size_t count = 0; count <= n; ++count) {
                    check_prefix(data, count);
                }
            }
        }
        check_all_algorithms<std::int64_t>({INT64_MIN, INT64_MAX, -1, 0, 1}, {5, 1, 3, 2, 4});
        check_all_algorithms<std::uint64_t>({UINT64_MAX, 0, std::uint64_t{1} << 63}, {3, 1, 2});
        check_all_algorithms<double>({-0.0, +0.0, -std::numeric_limits<double>::max(),
                                      std::numeric_limits<double>::max(),
                                      std::numeric_limits<double>::denorm_min()},
                                     {5, 1, 2, 3, 4});
        check_errors<rapid_stats::BfprtAlgorithm>();
        check_errors<rapid_stats::RadixSelectAlgorithm>();
        check_errors<rapid_stats::PartialQuicksortAlgorithm>();
        std::cout << "PASS: " << checks << " contract checks and error suites\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
