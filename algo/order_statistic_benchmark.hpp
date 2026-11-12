#pragma once

// Одна выборка на объект. Алгоритм получает копию, копия и проверка ранга вне замера.
// Элемент — int64.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rapid_stats {

using BenchmarkSample = std::vector<std::int64_t>;

enum class SamplePattern {
    Random,
    Ascending,
    Descending,
    AlmostSorted,
    ManyDuplicates,
    AllEqual,
    TwoValuesBalanced,
    TwoValuesSkewed,
    UpThenDown,
    FullInt64,
    SameHighBytes,
    LowByteOnly,
};

// median_ms — линия времени. p95_error и failure_rate — для приближённых.
// values — ответы последнего запуска.
struct BenchResult {
    double median_ms = 0.0;
    double p95_error = 0.0;
    double failure_rate = 0.0;
    std::vector<std::int64_t> values;
};

class OrderStatisticBenchmark {
public:
    OrderStatisticBenchmark(std::size_t size, SamplePattern pattern, std::uint64_t seed);

    // quantiles: одна доля — одна статистика, несколько — все за один sort.
    template <typename Algorithm>
    BenchResult exact(Algorithm& algorithm, const std::vector<double>& quantiles,
                      std::size_t repeats) const;

    // Зёрна алгоритма: seed, seed + 1, ... Массив один и тот же.
    template <typename Algorithm>
    BenchResult approximate(Algorithm& algorithm, const std::vector<double>& quantiles, double eps,
                            double delta, std::uint64_t seed, std::size_t repeats) const;

private:
    static BenchmarkSample make_sample(std::size_t size, SamplePattern pattern, std::uint64_t seed);
    static std::vector<std::size_t> ranks_for(const std::vector<double>& quantiles,
                                              std::size_t size);
    static double median_ms(std::vector<double> times);
    static std::size_t rank_miss(const BenchmarkSample& sorted, std::int64_t value,
                                 std::size_t rank);

    BenchmarkSample sample_;
    BenchmarkSample sorted_;
};

inline OrderStatisticBenchmark::OrderStatisticBenchmark(std::size_t size, SamplePattern pattern,
                                                        std::uint64_t seed)
    : sample_(make_sample(size, pattern, seed)), sorted_(sample_) {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: size — длина, pattern — вид выборки, seed — зерно генератора.
     * Возвращает: ничего. Сразу строит отсортированную копию для ошибки ранга.
     */
    std::sort(sorted_.begin(), sorted_.end());
}

template <typename Algorithm>
BenchResult OrderStatisticBenchmark::exact(Algorithm& algorithm,
                                           const std::vector<double>& quantiles,
                                           std::size_t repeats) const {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: algorithm — точный алгоритм, quantiles — доли от 0 до 1, repeats — число запусков.
     * Возвращает: медиану времени и ответы последнего запуска.
     */
    const std::vector<std::size_t> ranks = ranks_for(quantiles, sample_.size());
    std::vector<double> times;
    times.reserve(repeats);
    BenchResult result;

    for (std::size_t run = 0; run < repeats; ++run) {
        BenchmarkSample copy = sample_;
        const auto start = std::chrono::steady_clock::now();
        const auto found = algorithm.sort(copy, ranks);
        const auto stop = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(stop - start).count());

        result.values.clear();
        for (const auto it : found) {
            result.values.push_back(*it);
        }
    }

    result.median_ms = median_ms(std::move(times));
    return result;
}

template <typename Algorithm>
BenchResult OrderStatisticBenchmark::approximate(Algorithm& algorithm,
                                                 const std::vector<double>& quantiles, double eps,
                                                 double delta, std::uint64_t seed,
                                                 std::size_t repeats) const {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: algorithm — приближённый алгоритм, quantiles — доли, eps и delta — допуск, seed — первое зерно, repeats — число запусков.
     * Возвращает: медиану времени, 95-й процентиль относительной ошибки и долю неудачных запусков.
     */
    const std::vector<std::size_t> ranks = ranks_for(quantiles, sample_.size());
    const double n = static_cast<double>(sample_.size());
    std::vector<double> times;
    std::vector<double> errors;
    std::size_t failed = 0;
    BenchResult result;

    for (std::size_t run = 0; run < repeats; ++run) {
        BenchmarkSample copy = sample_;
        const auto start = std::chrono::steady_clock::now();
        const auto found = algorithm.sort(copy, ranks, eps, delta, seed + run);
        const auto stop = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(stop - start).count());

        bool bad = false;
        for (std::size_t i = 0; i < found.size() && i < ranks.size(); ++i) {
            const double relative =
                static_cast<double>(rank_miss(sorted_, *found[i], ranks[i])) / n;
            errors.push_back(relative);
            bad = bad || relative > eps;
        }
        failed += bad ? 1 : 0;
    }

    result.median_ms = median_ms(std::move(times));
    std::sort(errors.begin(), errors.end());
    if (!errors.empty()) {
        result.p95_error = errors[errors.size() * 95 / 100];
    }
    result.failure_rate = static_cast<double>(failed) / static_cast<double>(repeats);
    return result;
}

inline BenchmarkSample OrderStatisticBenchmark::make_sample(std::size_t size, SamplePattern pattern,
                                                            std::uint64_t seed) {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: size — длина, pattern — вид выборки, seed — зерно.
     * Возвращает: выборку int64.
     */
    if (size == 0) {
        throw std::invalid_argument("Sample size must be positive");
    }

    BenchmarkSample data(size);
    std::mt19937_64 rng(seed);

    switch (pattern) {
    case SamplePattern::Random: {
        std::uniform_int_distribution<std::int64_t> values(0, static_cast<std::int64_t>(size) * 10);
        for (std::int64_t& value : data) {
            value = values(rng);
        }
        break;
    }
    case SamplePattern::Ascending:
        for (std::size_t i = 0; i < size; ++i) {
            data[i] = static_cast<std::int64_t>(i);
        }
        break;
    case SamplePattern::Descending:
        for (std::size_t i = 0; i < size; ++i) {
            data[i] = static_cast<std::int64_t>(size - 1 - i);
        }
        break;
    case SamplePattern::AlmostSorted: {
        for (std::size_t i = 0; i < size; ++i) {
            data[i] = static_cast<std::int64_t>(i);
        }
        std::uniform_int_distribution<std::size_t> index(0, size - 1);
        for (std::size_t i = 0; i < std::max<std::size_t>(1, size / 100); ++i) {
            std::swap(data[index(rng)], data[index(rng)]);
        }
        break;
    }
    case SamplePattern::ManyDuplicates: {
        std::uniform_int_distribution<std::int64_t> values(0, 15);
        for (std::int64_t& value : data) {
            value = values(rng);
        }
        break;
    }
    case SamplePattern::AllEqual:
        break;
    case SamplePattern::TwoValuesBalanced:
        for (std::size_t i = 0; i < size; ++i) {
            data[i] = i < size / 2 ? 0 : 1;
        }
        std::shuffle(data.begin(), data.end(), rng);
        break;
    case SamplePattern::TwoValuesSkewed:
        for (std::size_t i = 0; i < size; ++i) {
            data[i] = i < std::max<std::size_t>(1, size / 100) ? 0 : 1;
        }
        std::shuffle(data.begin(), data.end(), rng);
        break;
    case SamplePattern::UpThenDown: {
        const std::size_t peak = (size - 1) / 2;
        for (std::size_t i = 0; i < size; ++i) {
            data[i] = static_cast<std::int64_t>(i <= peak ? i : size - 1 - i);
        }
        break;
    }
    case SamplePattern::FullInt64:
        for (std::int64_t& value : data) {
            value = static_cast<std::int64_t>(rng());
        }
        break;
    case SamplePattern::SameHighBytes:
        for (std::int64_t& value : data) {
            value = static_cast<std::int64_t>(0x0102030400000000ULL | (rng() & 0xFFFFFFFFULL));
        }
        break;
    case SamplePattern::LowByteOnly:
        for (std::int64_t& value : data) {
            value = static_cast<std::int64_t>(0x0102030405060700ULL | (rng() & 0xFFULL));
        }
        break;
    }
    return data;
}

inline std::vector<std::size_t>
OrderStatisticBenchmark::ranks_for(const std::vector<double>& quantiles, std::size_t size) {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: quantiles — доли от 0 до 1, size — длина выборки.
     * Возвращает: ранги с единицы, ранг = 1 + round(доля * (n - 1)).
     */
    std::vector<std::size_t> ranks;
    ranks.reserve(quantiles.size());
    for (const double quantile : quantiles) {
        if (quantile < 0.0 || quantile > 1.0) {
            throw std::invalid_argument("Quantile must be in [0, 1]");
        }
        const auto rank =
            static_cast<std::size_t>(std::llround(quantile * static_cast<double>(size - 1))) + 1;
        ranks.push_back(std::min(rank, size));
    }
    return ranks;
}

inline double OrderStatisticBenchmark::median_ms(std::vector<double> times) {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: times — времена запусков.
     * Возвращает: медиану. При чётном количестве берётся верхнее из двух центральных.
     */
    if (times.empty()) {
        throw std::invalid_argument("No runs");
    }
    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}

inline std::size_t OrderStatisticBenchmark::rank_miss(const BenchmarkSample& sorted,
                                                      std::int64_t value, std::size_t rank) {
    /*
     * Автор: Viktor Hlyupin
     * Принимает: sorted — упорядоченная выборка, value — найденный элемент, rank — запрошенный ранг.
     * Возвращает: на сколько позиций ранг промахнулся. Ноль, если значение подходит.
     */
    const auto lower = std::lower_bound(sorted.begin(), sorted.end(), value);
    const auto upper = std::upper_bound(lower, sorted.end(), value);
    const std::size_t low = static_cast<std::size_t>(lower - sorted.begin()) + 1;
    const std::size_t high = static_cast<std::size_t>(upper - sorted.begin());
    if (rank < low) {
        return low - rank;
    }
    if (rank > high) {
        return rank - high;
    }
    return 0;
}

} // namespace rapid_stats
