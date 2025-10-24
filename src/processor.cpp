#include "processor.h"

// ---------- Internal state ----------
namespace
{
  ProcessorConfig G; // copy of user config
  constexpr uint32_t PWM_MAX_MASKS[13] = {
      0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095};

  inline uint32_t PwmMax()
  {
    int b = G.pwmBits;
    if (b < 1)
      b = 1;
    if (b > 12)
      return (1u << G.pwmBits) - 1u; // fallback for exotic sizes
    return PWM_MAX_MASKS[b];
  }

  inline uint16_t PercentageToDutyCycle(float pct)
  {
    if (pct < 0)
      pct = 0;
    if (pct > 100)
      pct = 100;
    return (uint16_t)lroundf(pct * PwmMax() / 100.0f);
  }

  // Debounced buttons
  struct Btn
  {
    int pin;
    bool lastStable{true}; // pull-up => idle high
    bool lastRead{true};
    uint32_t lastChangeMs{0};
  };
  Btn Bstart;

  bool CheckButtonPress(Btn &b, uint16_t debounceMs = 30)
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

  // Blocking ramps (short; fine for 200–300 ms)
  void RampForward(uint16_t targetDuty, uint16_t ms)
  {
    LOGFLN("RampForward: target=%d, ms=%d", targetDuty, ms);
    uint16_t steps = ms / 10;
    if (steps == 0)
      steps = 1;
    for (uint16_t i = 0; i <= steps; ++i)
    {
      uint16_t d = (uint32_t)targetDuty * i / steps;
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
      ledcWrite(G.pins.in2, 0); // coast other leg
      ledcWrite(G.pins.in1, d); // forward drive on IN1
      if (i == steps) LOGFLN("RampForward final: IN1(pin %d)=%d, IN2(pin %d)=0", G.pins.in1, d, G.pins.in2);
#elif defined(ESP8266)
      analogWrite(G.pins.in2, 0); // coast other leg
      analogWrite(G.pins.in1, d); // forward drive on IN1
#endif
      delay(10);
    }
  }
  void RampReverse(uint16_t targetDuty, uint16_t ms)
  {
    LOGFLN("RampReverse: target=%d, ms=%d", targetDuty, ms);
    uint16_t steps = ms / 10;
    if (steps == 0)
      steps = 1;
    for (uint16_t i = 0; i <= steps; ++i)
    {
      uint16_t d = (uint32_t)targetDuty * i / steps;
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
      ledcWrite(G.pins.in1, 0);
      ledcWrite(G.pins.in2, d); // reverse drive on IN2
      if (i == steps) LOGFLN("RampReverse final: IN1(pin %d)=0, IN2(pin %d)=%d", G.pins.in1, G.pins.in2, d);
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
void InitializeProcessor(const ProcessorConfig &cfg)
{
  G = cfg; // copy-by-value

  // PWM setup - auto-detect platform
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  // ESP32 and ESP32-C6 both use v3 LEDC API
  ledcAttach(G.pins.in1, G.pwmHz, G.pwmBits);
  ledcAttach(G.pins.in2, G.pwmHz, G.pwmBits);
  LOGFLN("PWM setup: IN1=GPIO%d, IN2=GPIO%d, freq=%dHz, bits=%d", G.pins.in1, G.pins.in2, G.pwmHz, G.pwmBits);
#elif defined(ESP8266)
  // ESP8266 uses analogWrite with analogWriteFreq
  analogWriteFreq(G.pwmHz);
  analogWriteRange(PwmMax()); // Set PWM range to match pwmBits
  pinMode(G.pins.in1, OUTPUT);
  pinMode(G.pins.in2, OUTPUT);
#endif

  // Buttons
  pinMode(G.pins.btnStart, INPUT_PULLUP);
  pinMode(G.pins.btnStop, INPUT_PULLUP);
  // Init button state objects
  Bstart.pin = G.pins.btnStart;

  // Idle (coast)
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  // ESP32 and ESP32-C6 both use pin-based ledcWrite with v3 API
  ledcWrite(G.pins.in1, 0);
  ledcWrite(G.pins.in2, 0);
#elif defined(ESP8266)
  analogWrite(G.pins.in1, 0);
  analogWrite(G.pins.in2, 0);
#endif

  LOGFLN("Processor init: PWM=%dkHz bits=%d, cruise=%.1f%%",
         G.pwmHz / 1000, G.pwmBits, G.cruisePct);
}

//--------------------------------
// Motor control basics
//--------------------------------
void RunForwardDuty(uint16_t duty)
{
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  ledcWrite(G.pins.in2, 0); // coast leg
  ledcWrite(G.pins.in1, duty);
  LOGFLN("RunForwardDuty: IN1(pin %d)=%d, IN2(pin %d)=0", G.pins.in1, duty, G.pins.in2);
#elif defined(ESP8266)
  analogWrite(G.pins.in2, 0); // coast leg
  analogWrite(G.pins.in1, duty);
#endif
}

void RunReverseDuty(uint16_t duty)
{
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  ledcWrite(G.pins.in1, 0);
  ledcWrite(G.pins.in2, duty);
  LOGFLN("RunReverseDuty: IN1(pin %d)=0, IN2(pin %d)=%d", G.pins.in1, G.pins.in2, duty);
#elif defined(ESP8266)
  analogWrite(G.pins.in1, 0);
  analogWrite(G.pins.in2, duty);
#endif
}

void CoastStop()
{
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  ledcWrite(G.pins.in1, 0);
  ledcWrite(G.pins.in2, 0);
#elif defined(ESP8266)
  analogWrite(G.pins.in1, 0);
  analogWrite(G.pins.in2, 0);
#endif
}

void BrakeStop()
{
  uint32_t maxd = PwmMax();
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  ledcWrite(G.pins.in1, maxd);
  ledcWrite(G.pins.in2, maxd);
#elif defined(ESP8266)
  analogWrite(G.pins.in1, maxd);
  analogWrite(G.pins.in2, maxd);
#endif
}

//--------------------------------
// High-level patterns
//--------------------------------
void StartContinuousCycle()
{
  uint16_t cruise = PercentageToDutyCycle(G.cruisePct);
  running = true;
  dirForward = true;

  RampForward(cruise, G.t.rampUpMs);
  phase = Phase::RUN_FWD;
  phaseStartMs = millis();
}

void StopCycleCoast()
{
  uint16_t zero = 0;
  if (phase == Phase::RUN_FWD)
    RampForward(zero, G.t.rampDownMs);
  else if (phase == Phase::RUN_REV)
    RampReverse(zero, G.t.rampDownMs);
  CoastStop();
  running = false;
  phase = Phase::IDLE;
}

void StopCycleBrake()
{
  BrakeStop();
  running = false;
  phase = Phase::IDLE;
}

void ServiceProcessor()
{
  // Handle button (toggle state)
  if (CheckButtonPress(Bstart))
  {
    LOGFLN("Button pressed (toggle)");
    if (!running)
    {
    StartContinuousCycle();
    }
    else
    {
      StopCycleCoast();
    }
  }


  //-----------------------------------------------------------------
  // Phase Machine (non-blocking except short ramps at transitions)  
  //-----------------------------------------------------------------
  if (!running)
    return;

  uint32_t now = millis();
  uint16_t cruise = PercentageToDutyCycle(G.cruisePct);

  switch (phase)
  {
  case Phase::RUN_FWD:
    dirForward = true;
    if (now - phaseStartMs >= G.t.forwardRunMs)
    {
      RampForward(0, G.t.rampDownMs);
      CoastStop();
      delay(G.t.coastBetweenMs);
      RampReverse(cruise, G.t.rampUpMs);
      phase = Phase::RUN_REV;
      phaseStartMs = millis();
    }
    break;

  case Phase::RUN_REV:
    dirForward = false;
    if (now - phaseStartMs >= G.t.reverseRunMs)
    {
      RampReverse(0, G.t.rampDownMs);
      CoastStop();
      delay(G.t.coastBetweenMs);
      RampForward(cruise, G.t.rampUpMs);
      phase = Phase::RUN_FWD;
      phaseStartMs = millis();
    }
    break;

  case Phase::IDLE:
  default:
    break;
  }
}


void HandleSerialCLI()
{
  if (!Serial.available())
    return;
  const char cmd = Serial.read();

  if (cmd == 'f')
  {
    uint16_t d = PercentageToDutyCycle(G.cruisePct);
    LOGFLN("Manual FWD %.1f%%", G.cruisePct);
    RampForward(d, G.t.rampUpMs);
  }
  else if (cmd == 'r')
  {
    uint16_t d = PercentageToDutyCycle(G.cruisePct);
    LOGFLN("Manual REV %.1f%%", G.cruisePct);
    RampReverse(d, G.t.rampUpMs);
  }
  else if (cmd == 'c')
  {
    LOGFLN("Coast stop");
    StopCycleCoast();
  }
  else if (cmd == 'b')
  {
    LOGFLN("Brake stop");
    StopCycleBrake();
  }
  else if (cmd == 'a')
  {
    LOGFLN("Auto pattern start (indef)");
    StartContinuousCycle();
  }
  else if (cmd == 'u')
  {
    while (!Serial.available())
    { /* wait for serial */
    }

    float pct = Serial.parseFloat();
    G.cruisePct = pct;
    LOGFLN("Cruise set to %.1f%%", G.cruisePct);
  }
  else if (cmd == 'p')
  {
    LOGFLN("State: running=%d phase=%s duty=%.1f%%", (int)running, phaseName(phase), G.cruisePct);
  }
  else if (cmd == '1')
  {
    // Test GPIO2 (IN1) only
    LOGFLN("Test GPIO2 only at 50%%");
    uint16_t halfDuty = PercentageToDutyCycle(50.0f);
    ledcWrite(G.pins.in1, halfDuty);
    ledcWrite(G.pins.in2, 0);
  }
  else if (cmd == '2')
  {
    // Test GPIO3 (IN2) only
    LOGFLN("Test GPIO3 only at 50%%");
    uint16_t halfDuty = PercentageToDutyCycle(50.0f);
    ledcWrite(G.pins.in1, 0);
    ledcWrite(G.pins.in2, halfDuty);
  }
  else if (cmd == '0')
  {
    // Turn off both
    LOGFLN("Turn off both pins");
    ledcWrite(G.pins.in1, 0);
    ledcWrite(G.pins.in2, 0);
  }
  else
  {
    LOGFLN("Commands: f=FWD, r=REV, c=COAST, b=BRAKE, a=AUTO, u[%%], p=print, 1=test GPIO2, 2=test GPIO3, 0=off");
  }
}
