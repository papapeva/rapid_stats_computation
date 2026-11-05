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
     * Автор: Codex (OpenAI).
     * Принимает: проверяемое условие и сообщение о его нарушении.
     * Возвращает: ничего; при ошибке бросает исключение, в том числе в Release.
     */
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T> std::vector<std::uint64_t> representations(const std::vector<T>& data) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: вектор 64-битных чисел, включая оба знака нуля.
     * Возвращает: отсортированные битовые представления для проверки сохранности значений.
     */
    static_assert(sizeof(T) == sizeof(std::uint64_t), "64-bit test data required");
    std::vector<std::uint64_t> result;
    for (const auto value : data) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        result.push_back(bits);
    }
    std::sort(result.begin(), result.end());
    return result;
}

template <template <typename> class Algorithm, typename T>
void check(const std::vector<T>& original, const std::vector<std::size_t>& ranks) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: исходный массив и запрос рангов для проверяемого класса алгоритма.
     * Возвращает: ничего; проверяет полиморфный вызов, порядок результатов, повторы и повторный вызов.
     */
    auto data = original;
    auto sorted = original;
    std::sort(sorted.begin(), sorted.end());
    Algorithm<std::vector<T>> implementation(data);
    rapid_stats::OrderStatisticAlgorithm<std::vector<T>>& base = implementation;
    for (int repeat = 0; repeat < 2; ++repeat) {
        const auto results = base.sort(ranks);
        require(results.size() == ranks.size(), "wrong result count");
        for (std::size_t i = 0; i < ranks.size(); ++i) {
            require(results[i] != data.end(), "end iterator returned");
            require(*results[i] == sorted[ranks[i]], "incorrect order statistic");
        }
        require(representations(data) == representations(original), "input values lost or changed");
        ++checks;
    }
}

template <typename T>
void check_all_algorithms(const std::vector<T>& data, const std::vector<std::size_t>& ranks) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: одни и те же данные и нулевые ранги для четырёх вариантов.
     * Возвращает: ничего; сравнивает каждый результат с независимо отсортированным эталоном.
     */
    check<rapid_stats::BfprtAlgorithm>(data, ranks);
    check<rapid_stats::RadixSelectAlgorithm>(data, ranks);
    check<rapid_stats::RadixSortAlgorithm>(data, ranks);
    check<rapid_stats::PartialQuicksortAlgorithm>(data, ranks);
}

template <typename T> void check_prefix(const std::vector<T>& original, std::size_t count) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: исходные данные и число минимальных элементов для частичной сортировки.
     * Возвращает: ничего; проверяет весь префикс, хвост и неизменность данных при count=0.
     */
    auto data = original;
    auto sorted = original;
    std::sort(sorted.begin(), sorted.end());
    rapid_stats::PartialQuicksortAlgorithm<std::vector<T>> algorithm(data);
    algorithm.sort_prefix(count);
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
     * Автор: Codex (OpenAI).
     * Принимает: класс алгоритма через шаблонный параметр.
     * Возвращает: ничего; проверяет ошибки рангов и неконечных значений до модификации массива.
     */
    std::vector<double> data{3, 1, 2};
    Algorithm<std::vector<double>> algorithm(data);
    bool caught = false;
    try {
        algorithm.sort({0, data.size()});
    } catch (const std::out_of_range&) {
        caught = true;
    }
    require(caught && data == std::vector<double>({3, 1, 2}), "invalid ranks mutated input");
    for (double bad :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity()}) {
        data.back() = bad;
        const auto before = representations(data);
        caught = false;
        try {
            algorithm.sort({0});
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        require(caught && representations(data) == before, "nonfinite input accepted or changed");
        require(algorithm.sort({}).empty(), "empty query must be a no-op");
    }
    std::vector<double> empty;
    Algorithm<std::vector<double>> empty_algorithm(empty);
    require(empty_algorithm.sort({}).empty(), "empty query on empty data");
    caught = false;
    try {
        empty_algorithm.sort({0});
    } catch (const std::out_of_range&) {
        caught = true;
    }
    require(caught, "rank on empty array must throw");
}

template <typename T> void suite() {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: числовой тип через шаблонный параметр.
     * Возвращает: ничего; проверяет различные размеры, распределения и ранги, включая нулевой.
     */
    check_all_algorithms<T>({}, {});
    check_all_algorithms<T>({T{5}}, {0, 0});
    std::mt19937_64 rng(20261105);
    for (std::size_t n : {2U, 5U, 25U, 126U, 1000U, 10000U}) {
        std::vector<T> data(n);
        for (auto& value : data) {
            if constexpr (std::is_unsigned_v<T>) {
                /*
     * Автор: Codex (OpenAI).
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
            check_all_algorithms(data, {n - 1, 0, n / 2, 0, n - 1});
            check_all_algorithms(data, {});
            if (n <= 126) {
                std::vector<std::size_t> ranks(n);
                std::iota(ranks.begin(), ranks.end(), 0);
                std::reverse(ranks.begin(), ranks.end());
                check_all_algorithms(data, ranks);
            }
            for (auto count : {std::size_t{0}, std::size_t{1}, n / 2, n}) {
                check_prefix(data, count);
            }
        }
    }
}
} // namespace

int main() {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: ничего, тесты полностью воспроизводимы и не требуют пользовательского ввода.
     * Возвращает: 0 при успехе, 1 при нарушении контракта любого алгоритма.
     */
    try {
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
                    ranks[i] = n - 1 - i;
                }
                check_all_algorithms(data, ranks);
                for (std::size_t count = 0; count <= n; ++count) {
                    check_prefix(data, count);
                }
            }
        }
        check_all_algorithms<std::int64_t>({INT64_MIN, INT64_MAX, -1, 0, 1}, {4, 0, 2, 1, 3});
        check_all_algorithms<std::uint64_t>({UINT64_MAX, 0, std::uint64_t{1} << 63}, {2, 0, 1});
        check_all_algorithms<double>({-0.0, +0.0, -std::numeric_limits<double>::max(),
                                      std::numeric_limits<double>::max(),
                                      std::numeric_limits<double>::denorm_min()},
                                     {4, 0, 1, 2, 3});
        check_errors<rapid_stats::BfprtAlgorithm>();
        check_errors<rapid_stats::RadixSelectAlgorithm>();
        check_errors<rapid_stats::RadixSortAlgorithm>();
        check_errors<rapid_stats::PartialQuicksortAlgorithm>();
        std::cout << "PASS: " << checks << " contract checks and error suites\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
