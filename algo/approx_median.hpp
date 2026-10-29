#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <stdexcept>
#include <vector>


// Файл реализации алгоритма Approx median
template <typename T>
T approximate_order_statistic(
    std::vector<T>& a,
    std::size_t k,
    std::size_t h,
    double delta,
    std::uint64_t seed)
{
    const std::size_t n = a.size();

    if (n == 0) {
        throw std::invalid_argument("Array must not be empty");
    }

    if (k >= n) {
        throw std::out_of_range("k is out of range");
    }

    if (!std::isfinite(delta) ||
        delta <= 0.0 || delta >= 1.0) {
        throw std::invalid_argument("delta must be in (0, 1)");
    }

    // Если требуется точный результат, сортируем весь массив.
    if (h == 0 || n == 1) {
        std::vector<T> sorted = a;
        std::sort(sorted.begin(), sorted.end());
        return sorted[k];
    }

    // Допустимая ошибка ранга в долях размера массива.
    const long double epsilon =
        static_cast<long double>(h) /
        static_cast<long double>(n);

    // Размер выборки по оценке DKW:
    // m >= ln(2 / delta) / (2 * epsilon^2).
    const long double log_term =
        std::log(2.0L) -
        std::log(static_cast<long double>(delta));

    const long double raw_m =
        log_term / (2.0L * epsilon * epsilon);

    // Если выборка получается слишком большой,
    // используем точный алгоритм.
    if (!std::isfinite(raw_m) ||
        raw_m >= static_cast<long double>(n)) {
        std::vector<T> sorted = a;
        std::sort(sorted.begin(), sorted.end());
        return sorted[k];
    }

    const std::size_t m = std::max<std::size_t>(
        1,
        static_cast<std::size_t>(std::ceil(raw_m)));

    // Случайная выборка без возвращения.
    std::mt19937_64 rng(seed);

    std::vector<T> sample;
    sample.reserve(m);

    std::sample(
        a.begin(),
        a.end(),
        std::back_inserter(sample),
        m,
        rng);

    std::sort(sample.begin(), sample.end());

    // Переводим целевой ранг в соответствующий ранг выборки.
    // В исходном массиве и выборке используем ранги с 1.
    const long double raw_j =
        std::ceil(
            static_cast<long double>(m) *
            static_cast<long double>(k + 1) /
            static_cast<long double>(n));

    const std::size_t j = std::clamp(
        static_cast<std::size_t>(raw_j),
        std::size_t{1},
        m);

    return sample[j - 1];
}
