#pragma once

#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace rs_test {

struct Failure {
    std::string message;
};

struct TestCase {
    const char* name;
    void (*function)();
};

class Registry {
public:
    static std::vector<TestCase>& cases() {
        static std::vector<TestCase> all;
        return all;
    }

    static std::vector<Failure>& failures() {
        static std::vector<Failure> all;
        return all;
    }

    static const char*& current_test() {
        static const char* name = "";
        return name;
    }
};

class Registrar {
public:
    Registrar(const char* name, void (*function)()) {
        Registry::cases().push_back(TestCase{name, function});
    }
};

inline void fail(const char* file, int line, const std::string& message) {
    std::ostringstream out;
    out << file << ':' << line << " [" << Registry::current_test() << "] " << message;
    Registry::failures().push_back(Failure{out.str()});
    std::cerr << out.str() << '\n';
}

inline void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        fail(file, line, std::string("check failed: ") + expression);
    }
}

template <typename Actual, typename Expected>
void check_eq(const Actual& actual, const Expected& expected, const char* expression,
              const char* file, int line) {
    if (!(actual == expected)) {
        std::ostringstream out;
        out << expression << '\n' << "  actual:   " << actual << '\n' << "  expected: " << expected;
        fail(file, line, out.str());
    }
}

template <typename Exception, typename Fn>
void check_throws(Fn&& function, const char* expression, const char* file, int line) {
    try {
        std::forward<Fn>(function)();
    } catch (const Exception&) {
        return;
    } catch (const std::exception& ex) {
        fail(file, line, std::string(expression) + " threw " + ex.what());
        return;
    } catch (...) {
        fail(file, line, std::string(expression) + " threw a non-standard exception");
        return;
    }
    fail(file, line, std::string(expression) + " did not throw");
}

[[nodiscard]] inline int run_all() {
    const auto& tests = Registry::cases();
    if (tests.empty()) {
        std::cerr << "no tests registered\n";
        return 1;
    }

    int failed = 0;
    for (const auto& test : tests) {
        Registry::current_test() = test.name;
        Registry::failures().clear();
        try {
            test.function();
        } catch (const std::exception& ex) {
            std::cerr << "FAIL " << test.name << " threw " << ex.what() << '\n';
            ++failed;
            continue;
        } catch (...) {
            std::cerr << "FAIL " << test.name << " threw a non-standard exception\n";
            ++failed;
            continue;
        }

        if (!Registry::failures().empty()) {
            std::cerr << "FAIL " << test.name << " (" << Registry::failures().size()
                      << " checks)\n";
            ++failed;
        } else {
            std::cout << "ok   " << test.name << '\n';
        }
    }

    const auto passed = static_cast<int>(tests.size()) - failed;
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

} // namespace rs_test

#define RS_TEST(name)                                                                              \
    static void rs_test_##name();                                                                  \
    [[maybe_unused]] static const ::rs_test::Registrar rs_registrar_##name{#name,                  \
                                                                           &rs_test_##name};       \
    static void rs_test_##name()

#define RS_CHECK(expression)                                                                       \
    ::rs_test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

#define RS_CHECK_EQ(actual, expected)                                                              \
    ::rs_test::check_eq((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)

#define RS_CHECK_THROWS(ExceptionType, expression)                                                 \
    ::rs_test::check_throws<ExceptionType>([&] { static_cast<void>(expression); }, #expression,    \
                                           __FILE__, __LINE__)
