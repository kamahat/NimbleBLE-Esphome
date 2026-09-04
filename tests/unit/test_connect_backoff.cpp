// Host-buildable unit test (no ESP-IDF): g++ -std=c++20 -I components/nimble_ble
// tests/unit/test_connect_backoff.cpp components/nimble_ble/connect_backoff.cpp
#include "connect_backoff.h"

#include <cstdio>

using esphome::nimble_ble::compute_connect_backoff_delay_ms;
using esphome::nimble_ble::CONNECT_BACKOFF_BASE_MS;
using esphome::nimble_ble::CONNECT_BACKOFF_MAX_MS;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                         \
    }                                                                     \
  } while (0)

// No jitter (jitter draw at the exact midpoint of its range -> signed_percent
// == 0): the curve should be a clean base * 2^(n-1), capped.
static void test_exponential_curve_no_jitter() {
  const uint32_t mid = 20;  // (20 % 41) - 20 == 0
  CHECK(compute_connect_backoff_delay_ms(1, mid) == CONNECT_BACKOFF_BASE_MS);        // 5000
  CHECK(compute_connect_backoff_delay_ms(2, mid) == CONNECT_BACKOFF_BASE_MS * 2);    // 10000
  CHECK(compute_connect_backoff_delay_ms(3, mid) == CONNECT_BACKOFF_BASE_MS * 4);    // 20000
  CHECK(compute_connect_backoff_delay_ms(4, mid) == CONNECT_BACKOFF_BASE_MS * 8);    // 40000
  CHECK(compute_connect_backoff_delay_ms(5, mid) == CONNECT_BACKOFF_MAX_MS);         // 80000 -> capped 60000
  CHECK(compute_connect_backoff_delay_ms(20, mid) == CONNECT_BACKOFF_MAX_MS);        // stays capped
}

// backoff_count == 0 must not be treated as "one below the first failure"
// (which would double it) -- it should behave exactly like 1.
static void test_zero_backoff_count_is_base_delay() {
  const uint32_t mid = 20;
  CHECK(compute_connect_backoff_delay_ms(0, mid) == CONNECT_BACKOFF_BASE_MS);
}

// Jitter must stay within +/-20% of the unjittered value, in both directions.
static void test_jitter_stays_within_bounds() {
  const uint32_t base_delay = CONNECT_BACKOFF_BASE_MS;  // backoff_count=1
  for (uint32_t draw = 0; draw < 100; draw++) {
    uint32_t d = compute_connect_backoff_delay_ms(1, draw);
    CHECK(d >= base_delay * 80 / 100);
    CHECK(d <= base_delay * 120 / 100);
  }
}

// The cap must never be exceeded even with maximal positive jitter.
static void test_capped_value_plus_jitter_never_exceeds_reasonable_bound() {
  for (uint32_t draw = 0; draw < 100; draw++) {
    uint32_t d = compute_connect_backoff_delay_ms(10, draw);
    CHECK(d <= CONNECT_BACKOFF_MAX_MS * 120 / 100);
  }
}

int main() {
  test_exponential_curve_no_jitter();
  test_zero_backoff_count_is_base_delay();
  test_jitter_stays_within_bounds();
  test_capped_value_plus_jitter_never_exceeds_reasonable_bound();
  if (failures == 0) {
    std::printf("All connect_backoff tests passed.\n");
    return 0;
  }
  std::fprintf(stderr, "%d failure(s).\n", failures);
  return 1;
}
