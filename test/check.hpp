#ifndef ZOAL_BRIEFING_TEST_CHECK_HPP
#define ZOAL_BRIEFING_TEST_CHECK_HPP

// assert() vanishes in Release (NDEBUG), which is how `make test` builds, so
// tests use CHECK: always evaluated, and main returns test_result().

#include <cstdio>

inline int &test_failures() {
  static int failures = 0;
  return failures;
}

#define CHECK(cond)                                                                    \
  do {                                                                                 \
    if (!(cond)) {                                                                     \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);    \
      ++test_failures();                                                               \
    }                                                                                  \
  } while (0)

inline int test_result(const char *suite) {
  if (test_failures() == 0) {
    std::printf("%s: all checks passed\n", suite);
    return 0;
  }
  std::printf("%s: %d check(s) failed\n", suite, test_failures());
  return 1;
}

#endif
