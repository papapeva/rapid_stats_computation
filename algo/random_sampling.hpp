#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

// Приближённый выбор по алгоритму Random Sampling из задания.
// k — нулевой индекс исходного массива.
// select_exact должен принимать (vector<T>&, size_t zero_based_k)
// и возвращать точный k-й элемент.
template <typename T, typename SelectExact>
T random_sampling_select(
    std::vector<T>& a,
    std::size_t k,
    std::size_t h,
    double delta,
    std::uint64_t seed,
    SelectExact select_exact
) {
    const std::size_t n = a.size();

    if (n == 0) {
        throw std::invalid_argument("Array must not be empty");
    }
    if (k >= n) {
        throw std::out_of_range("k is out of range");
    }
    if (!std::isfinite(delta) || delta <= 0.0 || delta >= 1.0) {
        throw std::invalid_argument("delta must be in (0, 1)");
    }

    if (h == 0) {
        return select_exact(a, k);
    }

    // epsilon = h/n; формулу считаем в long double.
    const long double epsilon =
        static_cast<long double>(h) / static_cast<long double>(n);
    const long double log_term =
        std::log(2.0L) - std::log(static_cast<long double>(delta));
    const long double raw_m =
        log_term / (2.0L * epsilon * epsilon);

    // Если m >= n, выборка невыгодна: запускаем точный алгоритм.
    if (!std::isfinite(raw_m) ||
        raw_m >= static_cast<long double>(n)) {
        return select_exact(a, k);
    }

    const long double rounded_m = std::ceil(raw_m);
    if (rounded_m < 1.0L) {
        throw std::runtime_error("Calculated sample size is invalid");
    }
    const std::size_t m = static_cast<std::size_t>(rounded_m);

    // В спецификации ранги нумеруются с 1.
    const std::size_t requested_rank = k + 1;
    const long double raw_j =
        std::ceil(static_cast<long double>(m) *
                  static_cast<long double>(requested_rank) /
                  static_cast<long double>(n));

    const std::size_t j = static_cast<std::size_t>(raw_j); // 1-based rank

    std::mt19937_64 rng(seed); // инициализация один раз
    std::uniform_int_distribution<std::size_t> index_dist(0, n - 1);

    std::vector<T> sample;
    sample.reserve(m);

    for (std::size_t i = 0; i < m; ++i) {
        sample.push_back(a[index_dist(rng)]); // выборка с возвращением
    }

    return select_exact(sample, j - 1); // преобразуем ранг в 0-based индекс
}
