// Minimal header-only host test harness (GCC 4.7 / -std=gnu++0x safe).
//
// Usage (one tests/test_<name>.cpp per binary; the Makefile auto-discovers them):
//
//   #include "test_support.hpp"
//
//   TEST_CASE(myFeatureWorks) {
//       CHECK(1 + 1 == 2);                       // records failure, continues
//       CHECK_EQ(std::string("a"), std::string("a"));
//       CHECK_BYTES_EQ(test_support::hexToBytes("0a0b"), someVector);
//       REQUIRE(ptr != NULL);                    // records failure, aborts this test case
//   }
//
//   int main() { return test_support::runAll("test_<name>"); }
//
// runAll() prints a per-case [PASS]/[FAIL] line, then "test_<name>: N/M passed",
// and returns 0 only if every registered case passed.

#ifndef CARPLAY_TESTS_TEST_SUPPORT_HPP
#define CARPLAY_TESTS_TEST_SUPPORT_HPP

#include <stddef.h>
#include <stdint.h>

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace test_support {

typedef void (*TestFn)();

struct TestCase {
    const char* name;
    TestFn fn;
};

struct State {
    std::vector<TestCase> cases;
    int current_failures;
    int total_check_failures;

    State() : current_failures(0), total_check_failures(0) {}
};

inline State& state() {
    static State s;
    return s;
}

struct Registrar {
    Registrar(const char* name, TestFn fn) {
        TestCase tc;
        tc.name = name;
        tc.fn = fn;
        state().cases.push_back(tc);
    }
};

inline void recordFailure(const char* file, int line, const std::string& what) {
    std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, what.c_str());
    ++state().current_failures;
    ++state().total_check_failures;
}

/// Parses a hex string ("0a0B ff" — whitespace, ':' and '-' are ignored) into bytes.
/// Returns false on an odd digit count or a non-hex character.
inline bool hexToBytes(const std::string& hex, std::vector<uint8_t>& out) {
    out.clear();
    int hi = -1;
    for (size_t i = 0; i < hex.size(); ++i) {
        const char c = hex[i];
        int v = -1;
        if (c >= '0' && c <= '9') {
            v = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            v = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            v = c - 'A' + 10;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ':' || c == '-') {
            continue;
        } else {
            out.clear();
            return false;
        }
        if (hi < 0) {
            hi = v;
        } else {
            out.push_back(static_cast<uint8_t>((hi << 4) | v));
            hi = -1;
        }
    }
    if (hi >= 0) {
        out.clear();
        return false;
    }
    return true;
}

/// Convenience overload; returns an empty vector on malformed input.
inline std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> out;
    hexToBytes(hex, out);
    return out;
}

/// Lowercase hex, no separators.
inline std::string bytesToHex(const uint8_t* data, size_t len) {
    static const char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2U);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(kDigits[(data[i] >> 4) & 0x0fU]);
        out.push_back(kDigits[data[i] & 0x0fU]);
    }
    return out;
}

inline std::string bytesToHex(const std::vector<uint8_t>& v) {
    return v.empty() ? std::string() : bytesToHex(&v[0], v.size());
}

template <typename T>
inline std::string toDisplay(const T& v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

// Print 8-bit integers as numbers rather than characters.
inline std::string toDisplay(const uint8_t& v) {
    std::ostringstream oss;
    oss << static_cast<unsigned int>(v);
    return oss.str();
}

inline std::string toDisplay(const int8_t& v) {
    std::ostringstream oss;
    oss << static_cast<int>(v);
    return oss.str();
}

inline std::string toDisplay(const std::vector<uint8_t>& v) {
    return "hex:" + bytesToHex(v);
}

/// Runs every registered TEST_CASE in registration order. Returns 0 if all passed.
inline int runAll(const char* suite_name) {
    State& st = state();
    int passed = 0;
    const int total = static_cast<int>(st.cases.size());
    for (size_t i = 0; i < st.cases.size(); ++i) {
        st.current_failures = 0;
        st.cases[i].fn();
        if (st.current_failures == 0) {
            ++passed;
            std::printf("  [PASS] %s\n", st.cases[i].name);
        } else {
            std::printf("  [FAIL] %s (%d check failure%s)\n",
                        st.cases[i].name, st.current_failures,
                        st.current_failures == 1 ? "" : "s");
        }
    }
    std::printf("%s: %d/%d passed\n", suite_name, passed, total);
    std::fflush(stdout);
    return (passed == total && total > 0) ? 0 : 1;
}

}  // namespace test_support

#define TS_CONCAT_INNER(a, b) a##b
#define TS_CONCAT(a, b) TS_CONCAT_INNER(a, b)

/// Defines and registers a test case `void name()`.
#define TEST_CASE(name)                                                              \
    static void name();                                                              \
    static ::test_support::Registrar TS_CONCAT(ts_registrar_, name)(#name, &name);   \
    static void name()

/// Non-fatal check: records file:line and continues.
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            ::test_support::recordFailure(__FILE__, __LINE__, "CHECK(" #cond ")");   \
        }                                                                            \
    } while (0)

/// Fatal check: records file:line and returns from the current test case.
#define REQUIRE(cond)                                                                \
    do {                                                                             \
        if (!(cond)) {                                                               \
            ::test_support::recordFailure(__FILE__, __LINE__, "REQUIRE(" #cond ")"); \
            return;                                                                  \
        }                                                                            \
    } while (0)

/// Non-fatal equality check; prints both values on mismatch.
#define CHECK_EQ(expected, actual)                                                   \
    do {                                                                             \
        if (!((expected) == (actual))) {                                             \
            ::test_support::recordFailure(__FILE__, __LINE__,                        \
                std::string("CHECK_EQ(" #expected ", " #actual "): expected ") +     \
                ::test_support::toDisplay(expected) + " got " +                      \
                ::test_support::toDisplay(actual));                                  \
        }                                                                            \
    } while (0)

/// Non-fatal byte-vector equality check; prints both sides as hex on mismatch.
#define CHECK_BYTES_EQ(expected, actual)                                             \
    do {                                                                             \
        const std::vector<uint8_t> ts_e_ = (expected);                               \
        const std::vector<uint8_t> ts_a_ = (actual);                                 \
        if (ts_e_ != ts_a_) {                                                        \
            ::test_support::recordFailure(__FILE__, __LINE__,                        \
                std::string("CHECK_BYTES_EQ(" #expected ", " #actual "): expected ") + \
                ::test_support::bytesToHex(ts_e_) + " got " +                        \
                ::test_support::bytesToHex(ts_a_));                                  \
        }                                                                            \
    } while (0)

#endif  // CARPLAY_TESTS_TEST_SUPPORT_HPP
