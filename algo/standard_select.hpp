#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

// Обёртка над std::nth_element.
// k — нулевой индекс; массив может быть переставлен.
template <typename T>
T standard_select(std::vector<T>& a, std::size_t k) {
    if (a.empty()) {
        throw std::invalid_argument("Array must not be empty");
    }
    if (k >= a.size()) {
        throw std::out_of_range("k is out of range");
    }

    auto nth = a.begin() + static_cast<std::ptrdiff_t>(k);
    std::nth_element(a.begin(), nth, a.end());
    return *nth;
}
