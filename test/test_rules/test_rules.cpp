#include <gtest/gtest.h>
#include "ledconfig_rules.h"
#include "led_math.h"

using namespace ledconfig;

// ── isValidConfig ─────────────────────────────────────────────────────────────

TEST(IsValidConfig, RejectZeroLeds) {
  Config c{0, 2};
  EXPECT_FALSE(isValidConfig(c));
}

TEST(IsValidConfig, AcceptMinLeds) {
  Config c{1, 2};
  EXPECT_TRUE(isValidConfig(c));
}

TEST(IsValidConfig, AcceptMaxLeds) {
  Config c{1000, 2};
  EXPECT_TRUE(isValidConfig(c));
}

TEST(IsValidConfig, RejectOverMaxLeds) {
  Config c{1001, 2};
  EXPECT_FALSE(isValidConfig(c));
}

TEST(IsValidConfig, RejectUint32Max) {
  Config c{0xFFFFFFFFu, 2};
  EXPECT_FALSE(isValidConfig(c));
}

TEST(IsValidConfig, RejectUnsupportedPin) {
  Config c{100, 3};  // GPIO 3 not in kSupportedPins
  EXPECT_FALSE(isValidConfig(c));
}

TEST(IsValidConfig, AcceptSupportedPin2) {
  Config c{100, 2};
  EXPECT_TRUE(isValidConfig(c));
}

TEST(IsValidConfig, AcceptSupportedPin33) {
  Config c{100, 33};
  EXPECT_TRUE(isValidConfig(c));
}

// ── isSupportedPin ────────────────────────────────────────────────────────────

TEST(IsSupportedPin, SupportedPins) {
  const uint8_t pins[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
  for (uint8_t p : pins) {
    EXPECT_TRUE(isSupportedPin(p)) << "pin " << (int)p << " should be supported";
  }
}

TEST(IsSupportedPin, UnsupportedPins) {
  const uint8_t bad[] = {0, 1, 3, 6, 7, 8, 9, 10, 11, 20, 24, 28, 29, 30, 31, 34};
  for (uint8_t p : bad) {
    EXPECT_FALSE(isSupportedPin(p)) << "pin " << (int)p << " should NOT be supported";
  }
}

// ── intervalForSpeed ──────────────────────────────────────────────────────────

TEST(IntervalForSpeed, SlowEnd) {
  EXPECT_EQ(led::intervalForSpeed(0), 90);
}

TEST(IntervalForSpeed, FastEnd) {
  EXPECT_EQ(led::intervalForSpeed(255), 10);
}

TEST(IntervalForSpeed, AlwaysInRange) {
  for (int s = 0; s <= 255; ++s) {
    uint16_t v = led::intervalForSpeed(static_cast<uint8_t>(s));
    EXPECT_GE(v, 10u) << "speed " << s;
    EXPECT_LE(v, 90u) << "speed " << s;
  }
}

TEST(IntervalForSpeed, MonotonicallyDecreasing) {
  for (int s = 1; s <= 255; ++s) {
    EXPECT_LE(led::intervalForSpeed(static_cast<uint8_t>(s)),
              led::intervalForSpeed(static_cast<uint8_t>(s - 1)))
        << "not monotone at speed " << s;
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
