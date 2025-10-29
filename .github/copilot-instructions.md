# Copilot Instructions for Rollfilm Rotator

## Project Overview

This is a **PlatformIO-based ESP microcontroller project** that controls a rotary film processor. The system alternates motor direction for film development using a DRV8871 H-bridge motor driver across three ESP platforms with different pin configurations and PWM capabilities.

## Architecture & Key Components

### Multi-Platform Design Pattern
The project uses **compile-time platform detection** to configure pins and PWM parameters:

```cpp
// main.cpp pattern - platform-specific configuration
#if defined(CONFIG_IDF_TARGET_ESP32C6)
  cfg.pins = {2, 3, 9}; // GPIO pins for motor PWM1, PWM2, button
  cfg.pwmHz = 1000; cfg.pwmBits = 11;
#elif defined(ESP32) 
  cfg.pins = {18, 19, 25};
  cfg.pwmHz = 20000; cfg.pwmBits = 11;
#elif defined(ESP8266)
  cfg.pins = {D1, D2, D5};
  cfg.pwmHz = 1000; cfg.pwmBits = 10;
#endif
```

### Core Architecture
- **`main.cpp`**: Platform detection, pin configuration, timing parameters
- **`processor.h/cpp`**: Motor control, state machine, PWM abstraction layer
- **Configuration-driven**: All timing, speed, and hardware parameters centralized in `ProcessorConfig`

### State Machine Pattern
Three-phase operation with timing-based transitions:
- `IDLE` → `RUN_FWD` (start button) → `RUN_REV` (timer) → `RUN_FWD` (cycling)
- Uses `millis()` timing with configurable durations (`forwardRunMs`, `reverseRunMs`)
- Blocking ramps during transitions (10ms steps for smooth acceleration/deceleration)

## Development Workflow

### PlatformIO Commands
```bash
# Build for specific platform
pio run -e esp32c6        # ESP32-C6 Super Mini
pio run -e esp32dev       # ESP32-WROOM-32  
pio run -e d1_mini        # ESP8266 D1 Mini

# Upload and monitor
pio run -e <env> -t upload
pio device monitor -e <env>
```

### Platform-Specific Considerations

**ESP32-C6**: Uses forked platform (`pioarduino/platform-espressif32`) because official PlatformIO doesn't support newer chips. Requires USB CDC boot flags for serial communication.

**PWM Implementation**: 
- ESP32/ESP32-C6: `ledcAttach()` + `ledcWrite()` (v3 LEDC API)
- ESP8266: `analogWrite()` + `analogWriteFreq()`

### Serial CLI for Testing
When `ENABLE_SERIAL = 1`, provides single-character commands:
- `f`/`r`: Manual forward/reverse
- `a`: Start automatic cycling  
- `c`/`b`: Coast/brake stop
- `1`/`2`/`0`: Test individual GPIO pins
- `u[percentage]`: Set cruise speed

## Code Patterns & Conventions

### PWM Abstraction Pattern
```cpp
// processor.cpp - platform-agnostic motor control
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32)
  ledcWrite(G.pins.in1, duty);
#elif defined(ESP8266)  
  analogWrite(G.pins.in1, duty);
#endif
```

### Configuration Structure
Centralized hardware and timing config passed to `InitializeProcessor()`:
```cpp
ProcessorConfig cfg;
cfg.pins = {pwm1, pwm2, button};
cfg.cruisePct = 72.3f;  // Motor speed percentage
cfg.t.forwardRunMs = 10000; // Phase durations
```

### Button Debouncing
Custom debounced button handling in `CheckButtonPress()` with configurable debounce time (default 30ms).

## External Dependencies

- **ElegantOTA Library**: `/lib/ElegantOTA/` - Over-the-air firmware updates (AGPL-3.0 licensed)
- **Arduino Framework**: Cross-platform GPIO/PWM abstractions
- **Platform-specific**: ESP32 LEDC vs ESP8266 analogWrite APIs

## Hardware Integration

### DRV8871 H-Bridge Control
- **IN1/IN2 Logic**: One pin PWM + other pin 0 = directional drive
- **Coast**: Both pins 0  
- **Brake**: Both pins max PWM
- **Timing**: Ramp up/down + coast between direction changes

### Pin Layout Strategy
Pins chosen to avoid conflicts with common peripherals (I2C, SPI) and leave room for future expansion (display, additional buttons).

## Testing & Debugging

- Use serial CLI commands (`1`, `2`, `0`) to test individual GPIO pins before motor connection
- Monitor phase transitions and PWM values via serial output
- Verify platform detection with PWM frequency/bit depth logs during initialization