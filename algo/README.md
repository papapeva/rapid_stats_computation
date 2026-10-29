# Selection Benchmark (C++17)

## Files
- `floyd_rivest.hpp` — exact Floyd–Rivest selection.
- `random_sampling.hpp` — approximate Random Sampling selection.
- `standard_select.hpp` — wrapper around `std::nth_element`.
- `benchmark.cpp` — shared benchmark and correctness checks.
- `CMakeLists.txt` — optional CMake build file.

## Build with g++
```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic benchmark.cpp -o benchmark
```

## Run
Default sizes:
```bash
./benchmark
```

Custom sizes:
```bash
./benchmark 10000 100000 1000000
```

On Windows with MinGW:
```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic benchmark.cpp -o benchmark.exe
.\benchmark.exe 10000 100000 1000000
```

## Settings
In `benchmark.cpp`:
- `h` — allowed rank tolerance for Random Sampling.
- `delta` — allowed miss probability.
- `data_seed` — reproducible input data.
- `sample_seed` — reproducible sampling.

The random-sampling result is validated by rank tolerance, not exact equality,
because it is an approximate algorithm. The other two algorithms must return
the exact k-th element (with `k` being zero-based).

Array copies and sorting used for validation are outside each timed interval.
The reported time includes each algorithm's own work, including Random Sampling's
sample construction and its exact selection on the sample.
