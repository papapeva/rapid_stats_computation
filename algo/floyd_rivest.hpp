#pragma once

#include <algorithm>
#include <cmath>
#include <climits>
#include <stdexcept>
#include <vector>

// Точный выбор элемента по нулевому индексу k.
// функция реализации алгоритма Floyd_rivest
template <typename T>
void select_floyd_rivest_impl(std::vector<T>& a, int left, int right, int k) {
    while (right > left) {
        const int n = right - left + 1;

        if (n > 600) {
            const int i = k - left + 1;
            const double z = std::log(static_cast<double>(n));
            const double s = 0.5 * std::exp(2.0 * z / 3.0);
            double sd = 0.5 * std::sqrt(z * s * (n - s) / n);

            if (i < n / 2) {
                sd = -sd;
            }

            const int sample_left = std::max(
                left, static_cast<int>(std::floor(k - i * s / n + sd)));
            const int sample_right = std::min(
                right, static_cast<int>(std::floor(k + (n - i) * s / n + sd)));

            select_floyd_rivest_impl(a, sample_left, sample_right, k);
        }

        const T pivot = a[k];
        int lt = left;
        int i = left;
        int gt = right;

        while (i <= gt) {
            if (a[i] < pivot) {
                std::swap(a[lt], a[i]);
                ++lt;
                ++i;
            } else if (pivot < a[i]) {
                std::swap(a[i], a[gt]);
                --gt;
            } else {
                ++i;
            }
        }

        if (k < lt) {
            right = lt - 1;
        } else if (k > gt) {
            left = gt + 1;
        } else {
            return;
        }
    }
}

template <typename T>
T floyd_rivest_select(std::vector<T>& a, std::size_t k) {
    if (a.empty()) {
        throw std::invalid_argument("Array must not be empty");
    }
    if (k >= a.size()) {
        throw std::out_of_range("k is out of range");
    }
    if (a.size() > static_cast<std::size_t>(INT_MAX)) {
        throw std::length_error("Floyd-Rivest implementation uses int indices");
    }

    select_floyd_rivest_impl(
        a, 0, static_cast<int>(a.size()) - 1, static_cast<int>(k));
    return a[k];
}
