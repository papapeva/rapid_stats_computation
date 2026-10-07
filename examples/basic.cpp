#include "rapid_stats/order_statistic.hpp"

#include <iostream>
#include <vector>

int main() {
    const std::vector<int> sample{8, 3, 5, 1, 9, 2, 7};
    const auto n = static_cast<std::ptrdiff_t>(sample.size());
    const auto median_rank = (n - 1) / 2;

    std::cout << "sample size " << n << ", lower median rank " << median_rank << '\n';
    for (const auto algorithm : rapid_stats::kAlgorithms) {
        const auto median =
            rapid_stats::order_statistic(sample.begin(), sample.end(), median_rank, algorithm);
        std::cout << rapid_stats::algorithm_name(algorithm) << " -> " << median << '\n';
    }
    return 0;
}
