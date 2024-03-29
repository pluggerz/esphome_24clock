#ifndef ONEWIRE_H
#define ONEWIRE_H

// HHL = START
// HL = 1
// LH = 0
// LHL = END

#include "../clocks_shared/pinio.h"
#include "../clocks_shared/pins.h"
#include "../clocks_shared/ringbuffer.h"

#define ENABLE_TIMER_INTERUPT() interrupts();
#define DISABLE_TIMER_INTERUPT() noInterrupts();

#ifdef SLAVE
#define DOLED
#endif

#ifdef DOLED
#include "common/leds.h"
#endif

#ifdef ESP8266

#define MOVE2RAM IRAM_ATTR

// credits to: https://github.com/PaulStoffregen/Encoder/
#define IO_REG_TYPE uint32_t
#define PIN_TO_BASEREG(pin) ((volatile uint32_t *)(0x60000000 + (0x318)))
#define PIN_TO_BITMASK(pin) (digitalPinToBitMask(pin))
#define DIRECT_PIN_READ(base, mask) (((*(base)) & (mask)) ? 1 : 0)
// deducted from DIRECT_PIN_READ
#define DIRECT_PIN_SET(base, mask) *(base) |= mask
#define DIRECT_PIN_CLEAR(base, mask) *(base) -= *(base)&mask

#else
#define MOVE2RAM
#endif

namespace onewire {
const char *const TAG = "onewire";

constexpr int ONEWIRE_BUFFER_SIZE = 32;

constexpr int UNO_TIMER2_1024_OCR2A = 7 * 1;
constexpr int UNO_TIMER2_256_OCR2A = 24 * 1;
// not working ?
constexpr int UNO_TIMER2_128_OCR2A = 63;
constexpr int UNO_TIMER2_64_OCR2A = 124;

#define TIMER2_PRESCALER 1024
// #define TIMER1_FALLBACK

constexpr int determine_ocr2a(int value) {
  return value == 1024  ? UNO_TIMER2_1024_OCR2A
         : value == 256 ? UNO_TIMER2_256_OCR2A
         : value == 128 ? -1
         : value == 64  ? -1
                        : -1;
}

// timer2_comp: baud
// 4  :  1562,5
// 7  :   976,5625
// 250:    31,12549801

#ifdef TIMER1_FALLBACK
constexpr double USED_BAUD = 1953.5 / 2;
#else
constexpr double USED_BAUD = double(16000000) /
                             double(determine_ocr2a(TIMER2_PRESCALER) + 1) /
                             double(TIMER2_PRESCALER);
#endif

class RxOnewire;
class TxOnewire;

constexpr int8_t START_BITS = 2;

typedef uint32_t Value;
constexpr int8_t MAX_DATA_BITS = sizeof(Value) * 8;
constexpr Value DATA_MASK = Value((uint64_t(1) << uint64_t(MAX_DATA_BITS)) - 1);

#ifdef ESP8266
static volatile uint32_t *rx_basereg = PIN_TO_BASEREG(SYNC_IN_PIN);
static IO_REG_TYPE rx_bitmask = PIN_TO_BITMASK(SYNC_IN_PIN);

static volatile uint32_t *tx_basereg = PIN_TO_BASEREG(SYNC_OUT_PIN);
static IO_REG_TYPE tx_bitmask = PIN_TO_BITMASK(SYNC_OUT_PIN);
#endif

typedef void (*TimerLoop)(Micros now);

class OnewireInterrupt {
 public:
  static int timer_attach_state;
  static RxOnewire *rx;
  static TxOnewire *tx;
  static TimerLoop timer_loop;

  static float delay;

  static void attach();
  static void kill();

  static void dump_config();

  // to be refactored
  static void disableTimer();
  static void enableTimer();
};

const int8_t RX_BIT_INITIAL = -onewire::START_BITS - 1;

}  // namespace onewire

#endif
