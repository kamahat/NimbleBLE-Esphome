#include "connect_backoff.h"

namespace esphome::nimble_ble {

uint32_t compute_connect_backoff_delay_ms(uint16_t backoff_count, uint32_t jitter_random_0_99) {
  // Cap the shift itself (not just the final value) to avoid uint32_t
  // overflow for a pathologically large backoff_count -- the verified TLA+
  // model bounds it (MaxBackoff), but nothing stops a real uint16_t counting
  // higher in practice, and 5000 << 5 = 160000 already exceeds MAX_MS.
  constexpr uint8_t MAX_SHIFT = 5;
  uint8_t shift = backoff_count == 0 ? 0 : static_cast<uint8_t>(backoff_count - 1);
  if (shift > MAX_SHIFT)
    shift = MAX_SHIFT;

  uint32_t base = CONNECT_BACKOFF_BASE_MS << shift;
  uint32_t delay = base > CONNECT_BACKOFF_MAX_MS ? CONNECT_BACKOFF_MAX_MS : base;

  int32_t signed_percent = static_cast<int32_t>(jitter_random_0_99 % (2 * CONNECT_BACKOFF_JITTER_PERCENT + 1)) -
                            CONNECT_BACKOFF_JITTER_PERCENT;
  int32_t offset = static_cast<int32_t>(delay) * signed_percent / 100;
  return static_cast<uint32_t>(static_cast<int32_t>(delay) + offset);
}

}  // namespace esphome::nimble_ble
