#include "processor.h"

// ---------- Internal state ----------
namespace
{
  ProcessorConfig G; // copy of user config
  constexpr uint32_t PWM_MAX_MASKS[13] = {
      0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095};

  inline uint32_t pwmMax()
  {
    int b = G.pwmBits;
    if (b < 1)
      b = 1;
    if (b > 12)
      return (1u << G.pwmBits) - 1u; // fallback for exotic sizes
    return PWM_MAX_MASKS[b];
  }

  inline uint16_t pctToDuty(float pct)
  {
    if (pct < 0)
      pct = 0;
    if (pct > 100)
      pct = 100;
    return (uint16_t)lroundf(pct * pwmMax() / 100.0f);
  }

  // Debounced buttons
  struct Btn
  {
    int pin;
    bool lastStable{true}; // pull-up => idle high
    bool lastRead{true};
    uint32_t lastChangeMs{0};
  };
  Btn Bstart, Bstop;

  bool checkPressed(Btn &b, uint16_t debounceMs = 30)
  {
    bool r = digitalRead(b.pin); // HIGH released, LOW pressed
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
        return true;
    }
    return false;
  }

  // Phase machine
  enum class Phase
  {
    IDLE,
    RUN_FWD,
    RUN_REV
  };
  Phase phase = Phase::IDLE;
  bool running = false;
  bool dirForward = true;
  uint32_t phaseStartMs = 0;
  uint32_t runEndMs = 0; // 0 => indefinite

  // Blocking ramps (short; fine for 200–300 ms)
  void rampForward(uint16_t targetDuty, uint16_t ms)
  {
    uint16_t steps = ms / 10;
    if (steps == 0)
      steps = 1;
    for (uint16_t i = 0; i <= steps; ++i)
    {
      uint16_t d = (uint32_t)targetDuty * i / steps;
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
      ledcWrite(G.pins.in2, 0); // coast other leg
      ledcWrite(G.pins.in1, d); // forward drive on IN1
#elif defined(ESP8266)
      analogWrite(G.pins.in2, 0); // coast other leg
      analogWrite(G.pins.in1, d); // forward drive on IN1
#endif
      delay(10);
    }
  }
  void rampReverse(uint16_t targetDuty, uint16_t ms)
  {
    uint16_t steps = ms / 10;
    if (steps == 0)
      steps = 1;
    for (uint16_t i = 0; i <= steps; ++i)
    {
      uint16_t d = (uint32_t)targetDuty * i / steps;
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
      ledcWrite(G.pins.in1, 0);
      ledcWrite(G.pins.in2, d); // reverse drive on IN2
#elif defined(ESP8266)
      analogWrite(G.pins.in1, 0);
      analogWrite(G.pins.in2, d); // reverse drive on IN2
#endif
      delay(10);
    }
  }
} // namespace

// TODO: Put this somewhere better
static const char *phaseName(Phase p)
{
  switch (p)
  {
  case Phase::IDLE:
    return "IDLE";
  case Phase::RUN_FWD:
    return "RUN_FWD";
  case Phase::RUN_REV:
    return "RUN_REV";
  default:
    return "?";
  }
}

// ---------- Public API ----------
void initializeProcessor(const ProcessorConfig &cfg)
{
  G = cfg; // copy-by-value

  // PWM setup - auto-detect platform
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  // ESP32 and ESP32-C6 both use newer LEDC API
  ledcAttach(G.pins.in1, G.pwmHz, G.pwmBits);
  ledcAttach(G.pins.in2, G.pwmHz, G.pwmBits);
#elif defined(ESP8266)
  // ESP8266 uses analogWrite with analogWriteFreq
  analogWriteFreq(G.pwmHz);
  analogWriteRange(pwmMax()); // Set PWM range to match pwmBits
  pinMode(G.pins.in1, OUTPUT);
  pinMode(G.pins.in2, OUTPUT);
#endif

  // Buttons
  pinMode(G.pins.btnStart, INPUT_PULLUP);
  pinMode(G.pins.btnStop, INPUT_PULLUP);
  if (G.pins.btnRes1 >= 0)
    pinMode(G.pins.btnRes1, INPUT_PULLUP);
  if (G.pins.btnRes2 >= 0)
    pinMode(G.pins.btnRes2, INPUT_PULLUP);

  // Init button state objects
  Bstart.pin = G.pins.btnStart;
  Bstop.pin = G.pins.btnStop;

  // Idle (coast)
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  // ESP32 and ESP32-C6 both use pin-based ledcWrite
  ledcWrite(G.pins.in1, 0);
  ledcWrite(G.pins.in2, 0);
#elif defined(ESP8266)
  analogWrite(G.pins.in1, 0);
  analogWrite(G.pins.in2, 0);
#endif

  LOGFLN("Agitator init: PWM=%dkHz bits=%d, cruise=%.1f%%",
         G.pwmHz / 1000, G.pwmBits, G.cruisePct);
}

void setDefaultRunDurationMs(uint32_t ms) { G.defaultRunDurationMs = ms; }

void runForwardDuty(uint16_t duty)
{
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  ledcWrite(G.pins.in2, 0); // coast leg
  ledcWrite(G.pins.in1, duty);
#elif defined(ESP8266)
  analogWrite(G.pins.in2, 0); // coast leg
  analogWrite(G.pins.in1, duty);
#endif
}
void runReverseDuty(uint16_t duty)
{
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  ledcWrite(G.pins.in1, 0);
  ledcWrite(G.pins.in2, duty);
#elif defined(ESP8266)
  analogWrite(G.pins.in1, 0);
  analogWrite(G.pins.in2, duty);
#endif
}
void coastStop()
{
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  ledcWrite(G.pins.in1, 0);
  ledcWrite(G.pins.in2, 0);
#elif defined(ESP8266)
  analogWrite(G.pins.in1, 0);
  analogWrite(G.pins.in2, 0);
#endif
}
void brakeStop()
{
  uint32_t maxd = pwmMax();
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  ledcWrite(G.pins.in1, maxd);
  ledcWrite(G.pins.in2, maxd);
#elif defined(ESP8266)
  analogWrite(G.pins.in1, maxd);
  analogWrite(G.pins.in2, maxd);
#endif
}

void startTimedCycle(uint32_t durationMs)
{
  uint16_t cruise = pctToDuty(G.cruisePct);
  running = true;
  dirForward = true;
  runEndMs = (durationMs ? millis() + durationMs : 0);

  rampForward(cruise, G.t.rampUpMs);
  phase = Phase::RUN_FWD;
  phaseStartMs = millis();
}

void startContinuousCycle() { startTimedCycle(0); }

void stopCycleCoast()
{
  uint16_t zero = 0;
  if (phase == Phase::RUN_FWD)
    rampForward(zero, G.t.rampDownMs);
  else if (phase == Phase::RUN_REV)
    rampReverse(zero, G.t.rampDownMs);
  coastStop();
  running = false;
  phase = Phase::IDLE;
}

void stopCycleBrake()
{
  // For a gentle ramp first:
  /*
  uint16_t zero = 0;
  if (phase == Phase::RUN_FWD)      rampForward(zero, G.t.rampDownMs);
  else if (phase == Phase::RUN_REV) rampReverse(zero, G.t.rampDownMs);
  */
  brakeStop();
  running = false;
  phase = Phase::IDLE;
}

void serviceProcessor()
{
  // Buttons
  if (checkPressed(Bstart))
  {
    LOGFLN("BTN start (toggle)");
    if (!running) {
      if (G.defaultRunDurationMs)
        startTimedCycle(G.defaultRunDurationMs);
      else
        startContinuousCycle();
    } else {
      stopCycleCoast();
    }
  }
  // The stop button is reserved for future use, but not active in logic.
  // if (checkPressed(Bstop)) { /* reserved for later */ }

  // Timed stop
  if (running && runEndMs && (int32_t)(millis() - runEndMs) >= 0)
  {
    stopCycleCoast();
  }

  // Phase machine (non-blocking except short ramps at transitions)
  if (!running)
    return;

  uint32_t now = millis();
  uint16_t cruise = pctToDuty(G.cruisePct);

  switch (phase)
  {
  case Phase::RUN_FWD:
    dirForward = true;
    if (now - phaseStartMs >= G.t.forwardRunMs)
    {
      rampForward(0, G.t.rampDownMs);
      coastStop();
      delay(G.t.coastBetweenMs);
      rampReverse(cruise, G.t.rampUpMs);
      phase = Phase::RUN_REV;
      phaseStartMs = millis();
    }
    break;

  case Phase::RUN_REV:
    dirForward = false;
    if (now - phaseStartMs >= G.t.reverseRunMs)
    {
      rampReverse(0, G.t.rampDownMs);
      coastStop();
      delay(G.t.coastBetweenMs);
      rampForward(cruise, G.t.rampUpMs);
      phase = Phase::RUN_FWD;
      phaseStartMs = millis();
    }
    break;

  case Phase::IDLE:
  default:
    break;
  }
}

void handleSerialCLI()
{
  if (!Serial.available())
    return;
  const char cmd = Serial.read();

  if (cmd == 'f')
  {
    uint16_t d = pctToDuty(G.cruisePct);
    LOGFLN("Manual FWD %.1f%%", G.cruisePct);
    rampForward(d, G.t.rampUpMs);
  }
  else if (cmd == 'r')
  {
    uint16_t d = pctToDuty(G.cruisePct);
    LOGFLN("Manual REV %.1f%%", G.cruisePct);
    rampReverse(d, G.t.rampUpMs);
  }
  else if (cmd == 'c')
  {
    LOGFLN("Coast stop");
    stopCycleCoast();
  }
  else if (cmd == 'b')
  {
    LOGFLN("Brake stop");
    stopCycleBrake();
  }
  else if (cmd == 'a')
  {
    LOGFLN("Auto pattern start (indef)");
    startContinuousCycle();
  }
  else if (cmd == 't')
  {
    while (!Serial.available())
    { /* wait */
    }
    int minutes = Serial.parseInt();
    if (minutes <= 0)
      minutes = 1;
    LOGFLN("Timed run %d min", minutes);
    startTimedCycle((uint32_t)minutes * 60UL * 1000UL);
  }
  else if (cmd == 'u')
  {
    while (!Serial.available())
    { /* wait */
    }
    float pct = Serial.parseFloat();
    G.cruisePct = pct;
    LOGFLN("Cruise set to %.1f%%", G.cruisePct);
  }
  else if (cmd == 'p')
  {
    LOGFLN("State: running=%d phase=%s duty=%.1f%%", (int)running, phaseName(phase), G.cruisePct);
  }
  else
  {
    LOGFLN("Commands: f=FWD, r=REV, c=COAST, b=BRAKE, a=AUTO, t[min], u[%%], p=print");
  }
}
