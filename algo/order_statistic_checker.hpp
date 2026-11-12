#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace rapid_stats {

// Выборка и проверка алгоритма на ней. Ранг с единицы.
class OrderStatisticChecker {
public:
    OrderStatisticChecker(std::size_t size, std::uint64_t seed);

    std::vector<int>& data();
    [[nodiscard]] const std::vector<int>& data() const;

    // Истина, если каждый возвращённый элемент равен статистике своего ранга.
    template <typename Algorithm>
    bool check_exact(Algorithm& algorithm, const std::vector<std::size_t>& ranks);

    // Истина, если каждый ответ лежит не дальше eps * n от своего ранга.
    template <typename Algorithm>
    bool check_approximate(Algorithm& algorithm, const std::vector<std::size_t>& ranks, double eps,
                           double delta, std::uint64_t seed);

private:
    [[nodiscard]] bool points_into(std::vector<int>::const_iterator it) const;
    [[nodiscard]] std::size_t rank_error(int value, std::size_t rank) const;

    std::vector<int> data_;
};

inline OrderStatisticChecker::OrderStatisticChecker(std::size_t size, std::uint64_t seed) {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: size — длина выборки, seed — зерно генератора.
     * Возвращает: ничего.
     */
    data_.resize(size);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> values(0, 999);
    for (int& value : data_) {
        value = values(rng);
    }
}

inline std::vector<int>& OrderStatisticChecker::data() {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: ничего.
     * Возвращает: выборку, которую можно отдать алгоритму.
     */
    return data_;
}

inline const std::vector<int>& OrderStatisticChecker::data() const {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: ничего.
     * Возвращает: выборку без права менять элементы.
     */
    return data_;
}

template <typename Algorithm>
bool OrderStatisticChecker::check_exact(Algorithm& algorithm,
                                        const std::vector<std::size_t>& ranks) {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: algorithm — точный алгоритм, ranks — ранги от 1 до n.
     * Возвращает: истину, если ответы совпали с полной сортировкой.
     */
    std::vector<int> sorted = data_;
    std::sort(sorted.begin(), sorted.end());

    std::vector<std::vector<int>::iterator> result;
    try {
        result = algorithm.sort(data_, ranks);
    } catch (const std::exception&) {
        return false;
    }

    if (result.size() != ranks.size()) {
        return false;
    }

    for (std::size_t i = 0; i < ranks.size(); ++i) {
        const std::size_t rank = ranks[i];
        if (rank < 1 || rank > sorted.size() || !points_into(result[i])) {
            return false;
        }
        if (*result[i] != sorted[rank - 1]) {
            return false;
        }
    }
    return true;
}

template <typename Algorithm>
bool OrderStatisticChecker::check_approximate(Algorithm& algorithm,
                                              const std::vector<std::size_t>& ranks, double eps,
                                              double delta, std::uint64_t seed) {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: algorithm — приближённый алгоритм, ranks — ранги от 1 до n, eps — доля n, delta — вероятность ошибки, seed — зерно.
     * Возвращает: истину, если каждый ответ попал в допуск eps * n.
     */
    std::vector<std::vector<int>::iterator> result;
    try {
        result = algorithm.sort(data_, ranks, eps, delta, seed);
    } catch (const std::exception&) {
        return false;
    }

    if (result.size() != ranks.size()) {
        return false;
    }

    const auto allowed =
        static_cast<std::size_t>(std::ceil(eps * static_cast<double>(data_.size())));

    for (std::size_t i = 0; i < ranks.size(); ++i) {
        if (!points_into(result[i]) || rank_error(*result[i], ranks[i]) > allowed) {
            return false;
        }
    }
    return true;
}

inline bool OrderStatisticChecker::points_into(std::vector<int>::const_iterator it) const {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: it — итератор из ответа алгоритма.
     * Возвращает: истину, если он указывает внутрь выборки.
     */
    return it >= data_.cbegin() && it < data_.cend();
}

inline std::size_t OrderStatisticChecker::rank_error(int value, std::size_t rank) const {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: value — найденный элемент, rank — запрошенный ранг от 1 до n.
     * Возвращает: на сколько позиций ранг элемента промахнулся мимо запрошенного.
     */
    std::size_t less = 0;
    std::size_t equal = 0;
    for (const int item : data_) {
        if (item < value) {
            ++less;
        } else if (item == value) {
            ++equal;
        }
    }

    const std::size_t low = less + 1;
    const std::size_t high = less + equal;
    if (equal == 0 || rank < low) {
        return rank < low ? low - rank : rank;
    }
    if (rank > high) {
        return rank - high;
    }
    return 0;
}

} // namespace rapid_stats
