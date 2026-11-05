#pragma once
// Minimal dependency-free test framework, only for BitMotor's own tests:
//     TEST(RandomIsDeterministic) { Random a(1), b(1); CHECK_EQ(a.Next(), b.Next()); }
#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace BitMotor::Testing {

struct Case {
    const char* Name;
    std::function<void()> Body;
};

inline std::vector<Case>& Registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& CurrentFailures() {
    static int failures = 0;
    return failures;
}

struct Register {
    Register(const char* name, std::function<void()> body) { Registry().push_back({name, std::move(body)}); }
};

inline void Fail(const char* file, int line, const std::string& message) {
    std::printf("    FAIL %s:%d: %s\n", file, line, message.c_str());
    ++CurrentFailures();
}

template <class A, class B> std::string Describe(const A& a, const B& b) {
    std::ostringstream s;
    s << a << " != " << b;
    return s.str();
}

// Runs every test (or those whose name contains `filter`). Returns 0 if all pass.
inline int Run(const char* filter = nullptr) {
    int failed = 0, total = 0;
    for (auto& test : Registry()) {
        if (filter && std::string(test.Name).find(filter) == std::string::npos) continue;
        ++total;
        CurrentFailures() = 0;
        try {
            test.Body();
        } catch (const std::exception& e) {
            Fail(test.Name, 0, std::string("exception: ") + e.what());
        }
        bool ok = CurrentFailures() == 0;
        std::printf("%s %s\n", ok ? "[ok]  " : "[FAIL]", test.Name);
        if (!ok) ++failed;
    }
    std::printf("%d/%d tests passed\n", total - failed, total);
    if (total == 0) std::printf("no test matches '%s'\n", filter ? filter : "");
    return failed == 0 && total > 0 ? 0 : 1;
}

} // namespace BitMotor::Testing

#define BITMOTOR_CONCAT2(a, b) a##b
#define BITMOTOR_CONCAT(a, b) BITMOTOR_CONCAT2(a, b)

#define TEST(name)                                                                                                     \
    static void name();                                                                                                \
    static ::BitMotor::Testing::Register BITMOTOR_CONCAT(Register_, name)(#name, name);                                \
    static void name()

#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) ::BitMotor::Testing::Fail(__FILE__, __LINE__, #expr);                                             \
    } while (0)

#define CHECK_EQ(a, b)                                                                                                 \
    do {                                                                                                               \
        auto&& checkA_ = (a);                                                                                          \
        auto&& checkB_ = (b);                                                                                          \
        if (!(checkA_ == checkB_))                                                                                     \
            ::BitMotor::Testing::Fail(                                                                                 \
                __FILE__, __LINE__, std::string(#a " == " #b ": ") + ::BitMotor::Testing::Describe(checkA_, checkB_)); \
    } while (0)

#define CHECK_NEAR(a, b, epsilon)                                                                                      \
    do {                                                                                                               \
        double checkA_ = double(a), checkB_ = double(b);                                                               \
        if (std::abs(checkA_ - checkB_) > (epsilon))                                                                   \
            ::BitMotor::Testing::Fail(                                                                                 \
                __FILE__, __LINE__, std::string(#a " ~ " #b ": ") + ::BitMotor::Testing::Describe(checkA_, checkB_));  \
    } while (0)
