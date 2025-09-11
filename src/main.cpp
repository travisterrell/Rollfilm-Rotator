#include <Arduino.h>

// ---------------- Pins ----------------
constexpr int PIN_IN1 = 18; // DRV8871 IN1 (PWM-capable)
constexpr int PIN_IN2 = 19; // DRV8871 IN2 (PWM-capable)

constexpr int PIN_BTN_START = 25; // Active-low button to start
constexpr int PIN_BTN_STOP = 26;  // Active-low button to stop (coast)
constexpr int PIN_BTN_RES1 = 27;  // reserved
constexpr int PIN_BTN_RES2 = 14;  // reserved

// (Optional) nFAULT if you wire it
// constexpr int PIN_NFAULT = 21;

// ---------------- PWM (LEDC) ----------------
constexpr int CH_IN1 = 0;
constexpr int CH_IN2 = 1;
constexpr int PWM_HZ = 20000; // 20 kHz
constexpr int PWM_BITS = 12;  // 0..4095
constexpr int PWM_MAX = (1 << PWM_BITS) - 1;

// ---------------- Agitation parameters ----------------
float cruisePct = 65.0f; // ~65% ≈ ~70 RPM drum w/ your 100 RPM motor + 32:30 external
uint16_t rampUpMs = 300;
uint16_t rampDownMs = 200;
uint16_t coastBetweenMs = 500;
uint32_t forwardRunMs = 10000; // 10 s forward
uint32_t reverseRunMs = 10000; // 10 s reverse

// ---------------- Helpers ----------------
inline uint16_t pctToDuty(float pct)
{
  if (pct < 0)
    pct = 0;
  if (pct > 100)
    pct = 100;
  return (uint16_t)lroundf(pct * PWM_MAX / 100.0f);
}

// Drive↔coast primitives
void runForwardDuty(uint16_t duty)
{
  ledcWrite(CH_IN2, 0);    // other leg = coast
  ledcWrite(CH_IN1, duty); // drive forward (IN1 PWM)
}
void runReverseDuty(uint16_t duty)
{
  ledcWrite(CH_IN1, 0);
  ledcWrite(CH_IN2, duty); // drive reverse (IN2 PWM)
}
void coastStop()
{
  ledcWrite(CH_IN1, 0);
  ledcWrite(CH_IN2, 0);
}
void brakeStop()
{
  // DRV8871 brake = (IN1,IN2) = (1,1)
  ledcWrite(CH_IN1, PWM_MAX);
  ledcWrite(CH_IN2, PWM_MAX);
}

// Blocking ramps (short; fine for 200–300 ms)
void rampForward(uint16_t targetDuty, uint16_t ms)
{
  uint16_t steps = max<uint16_t>(1, ms / 10);
  for (uint16_t i = 0; i <= steps; ++i)
  {
    uint16_t d = (uint32_t)targetDuty * i / steps;
    runForwardDuty(d);
    delay(10);
  }
}
void rampReverse(uint16_t targetDuty, uint16_t ms)
{
  uint16_t steps = max<uint16_t>(1, ms / 10);
  for (uint16_t i = 0; i <= steps; ++i)
  {
    uint16_t d = (uint32_t)targetDuty * i / steps;
    runReverseDuty(d);
    delay(10);
  }
}

// ---------------- Simple debounced buttons ----------------
struct Btn
{
  int pin;
  bool lastStable;
  bool lastRead;
  uint32_t lastChangeMs;

  Btn(int p) : pin(p), lastStable(true), lastRead(true), lastChangeMs(0) {}
};

Btn btnStart(PIN_BTN_START);
Btn btnStop(PIN_BTN_STOP);

bool checkPressed(Btn &b, uint16_t debounceMs = 30)
{
  bool r = digitalRead(b.pin); // HIGH = released, LOW = pressed
  uint32_t now = millis();
  if (r != b.lastRead)
  {
    b.lastRead = r;
    b.lastChangeMs = now;
  }
  if ((now - b.lastChangeMs) >= debounceMs && r != b.lastStable)
  {
    b.lastStable = r;
    if (b.lastStable == LOW)
      return true; // new press
  }
  return false;
}

// ---------------- State machine ----------------
enum class Phase
{
  IDLE,
  RUN_FWD,
  RUN_REV
};
Phase phase = Phase::IDLE;
bool running = false;
bool dirForward = true; // remember last drive direction for graceful stop
uint32_t phaseStartMs = 0;
uint32_t runEndMs = 0; // 0 = indefinite

void startTimedCycle(uint32_t durationMs)
{
  uint16_t cruise = pctToDuty(cruisePct);
  running = true;
  dirForward = true;
  runEndMs = (durationMs ? millis() + durationMs : 0);

  // spin up into first forward run
  rampForward(cruise, rampUpMs);
  phase = Phase::RUN_FWD;
  phaseStartMs = millis();
}

void startContinuousCycle()
{
  startTimedCycle(0);
}

void stopCycleCoast()
{
  // graceful ramp down based on current direction
  uint16_t zero = 0;
  if (phase == Phase::RUN_FWD)
  {
    rampForward(zero, rampDownMs);
  }
  else if (phase == Phase::RUN_REV)
  {
    rampReverse(zero, rampDownMs);
  }
  coastStop();
  running = false;
  phase = Phase::IDLE;
}

void setup()
{
  // PWM channels
  ledcSetup(CH_IN1, PWM_HZ, PWM_BITS);
  ledcSetup(CH_IN2, PWM_HZ, PWM_BITS);
  ledcAttachPin(PIN_IN1, CH_IN1);
  ledcAttachPin(PIN_IN2, CH_IN2);

  // Buttons
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_STOP, INPUT_PULLUP);
  pinMode(PIN_BTN_RES1, INPUT_PULLUP); // reserved
  pinMode(PIN_BTN_RES2, INPUT_PULLUP); // reserved

  // Optional nFAULT
  // pinMode(PIN_NFAULT, INPUT_PULLUP);

  // Idle
  coastStop();
}

void loop()
{
  // --- Button handling ---
  if (checkPressed(btnStart))
  {
    // Example: run for 30 minutes (1,800,000 ms). Change as you like, or call startCycleIndefinite().
    startTimedCycle(1800000UL);
  }
  if (checkPressed(btnStop))
  {
    stopCycleCoast();
  }

  // --- Timed auto-stop ---
  if (running && runEndMs && (int32_t)(millis() - runEndMs) >= 0)
  {
    stopCycleCoast();
  }

  // --- Phase machine (non-blocking except short ramps triggered on transitions) ---
  if (!running)
    return;

  uint32_t now = millis();
  uint16_t cruise = pctToDuty(cruisePct);

  switch (phase)
  {
  case Phase::RUN_FWD:
    dirForward = true;
    if (now - phaseStartMs >= forwardRunMs)
    {
      // ramp down, coast, then ramp up reverse
      rampForward(0, rampDownMs);
      coastStop();
      delay(coastBetweenMs);
      rampReverse(cruise, rampUpMs);
      phase = Phase::RUN_REV;
      phaseStartMs = millis();
    }
    break;

  case Phase::RUN_REV:
    dirForward = false;
    if (now - phaseStartMs >= reverseRunMs)
    {
      rampReverse(0, rampDownMs);
      coastStop();
      delay(coastBetweenMs);
      rampForward(cruise, rampUpMs);
      phase = Phase::RUN_FWD;
      phaseStartMs = millis();
    }
    break;

  case Phase::IDLE:
  default:
    break;
  }
}
