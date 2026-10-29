#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

// Приближённое вычисление k-й порядковой статистики.
//
// T — тип элементов массива.
// k — целевой ранг, начиная с 1.
// c — параметр надёжности, c > 0.
// epsilon — допустимая ошибка относительного ранга.
// seed — начальное значение генератора случайных чисел.
//
// Возвращает std::nullopt, если массив пуст или k некорректен.
// Требуется C++17.

template <typename T>
std::optional<T> approximate_order_statistic(
    const std::vector<T>& values,
    std::size_t k,
    double c,
    double epsilon = 0.05,
    std::uint64_t seed = 12345)
{
    const std::size_t n = values.size();

    // Проверка входных данных.
    if (n == 0 || k < 1 || k > n) {
        return std::nullopt;
    }

    if (!std::isfinite(c) || c <= 0.0) {
        throw std::invalid_argument("c must be positive");
    }

    if (!std::isfinite(epsilon) ||
        epsilon <= 0.0 || epsilon >= 1.0) {
        throw std::invalid_argument("epsilon must be in (0, 1)");
    }

    if (n == 1) {
        return values[0];
    }

    // Размер выборки согласно оценке DKW:
    // P(error > epsilon) <= 2 * exp(-2 * s * epsilon^2).
    const double required =
        (c * std::log(static_cast<double>(n)) + std::log(2.0))
        / (2.0 * epsilon * epsilon);

    const std::size_t sample_size =
        required >= static_cast<double>(n)
            ? n
            : std::max<std::size_t>(
                  1, static_cast<std::size_t>(std::ceil(required)));

    // Если выборка совпадает с исходным массивом,
    // можно получить точный ответ.
    if (sample_size == n) {
        std::vector<T> sorted(values);
        std::sort(sorted.begin(), sorted.end());
        return sorted[k - 1];
    }

    // Генератор случайных чисел с фиксированным seed.
    std::mt19937_64 generator(seed);

    // Случайная выборка без возвращения.
    std::vector<T> sample;
    sample.reserve(sample_size);

    std::sample(
        values.begin(),
        values.end(),
        std::back_inserter(sample),
        sample_size,
        generator
    );

    // Сортировка выборки.
    std::sort(sample.begin(), sample.end());

    // Вычисление относительного целевого ранга.
    const double fraction =
        static_cast<double>(k - 1) / static_cast<double>(n - 1);

    std::size_t index = static_cast<std::size_t>(
        std::floor(fraction * (sample_size - 1))
    );

    index = std::min(index, sample.size() - 1);

    return sample[index];
}


