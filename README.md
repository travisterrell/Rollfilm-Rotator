# Rollfilm Rotator: Film Processor Controller

A microcontroller-based control system for a rotary film processing rig, supporting multiple ESP platforms (ESP32C6/ESP32/ESP8266).

## Overview

This project provides automated motor control for rotary film processors. When the momentary button is pressed, the system begins alternating between forward and reverse rotation according to configurable timing and speed parameters, making it suitable for various film processing workflows. A 2nd press of the button stops the rotation cycle.

Note that a timer is not implemented or planned because I use a DIY [lift-style processor](https://www.printables.com/model/1183451-film-processor-rotationsprozessor-fur-jobo-tank) and follow Jobo's recommendation of maintaining continuous rotation whilst adding/removing chemicals.

**Planned:** Either Rotary encoders for setting rotation speed & direction intervals or simple buttons for preset programs.

## Features

- **Multi-Platform Support**: Automatically configures for ESP32, ESP32-C6, and ESP8266 chips based on selected build target
- **Toggle Control**: Single button starts/stops the processor
- **Bidirectional Motor Control**: Alternates between forward and reverse rotation
- **Configurable Timing**: Adjustable ramp-up/down times, run durations, and coast/brake periods
- **PWM Motor Drive**: Smooth motor control with configurable cruise speed percentage, soft start/stop
- **Serial CLI**: USB serial interface for monitoring and control (optional)

## Supported Hardware

| Platform | Board | Pins Used | PWM Specs |
|----------|-------|-----------|-----------|
| **ESP32** | ESP32-WROOM-32 NodeMCU clone | GPIO 18,19 (motor)<br/>GPIO 25 (button) | 20kHz, 11-bit |
| **ESP32-C6** | Super Mini | GPIO 2,3 (motor)<br/>GPIO 9 (button) | 20kHz, 11-bit |
| **ESP8266** | D1 Mini Clone | D1,D2 (motor)<br/>D5 (button) | 1kHz, 10-bit |

## Hardware Requirements

### Core Components
- One of the supported microcontroller boards
- **DRV8871 Motor Driver** (or compatible H-bridge)
- **DC Motor (Brushed)** for film processor rotation
- **Push Button** for start/stop control
- **Power Supply** appropriate for your motor

### Wiring Diagram

```
Microcontroller  →  DRV8871 Motor Driver  →  DC Motor
     PWM1        →        IN1
     PWM2        →        IN2
     
Button (Active Low with Internal Pullup)
Toggle Button   →  GPIO Pin  →  GND when pressed
```

### Motor Driver Connection
The system uses a DRV8871-style H-bridge driver:
- **IN1**: Forward PWM signal
- **IN2**: Reverse PWM signal
- **VM**: Motor supply voltage
- **GND**: Common ground
- **OUT1/OUT2**: Motor connections

## Software Architecture

### Key Components

- **`main.cpp`**: Platform-specific pin configuration and main loop
- **`processor.h/cpp`**: Motor control, timing, and state machine
- **Compile-Time Configuration**: Uses preprocessor directives to select appropriate pins/PWM parameters for supported target platforms

### Operating States

1. **IDLE**: Motor stopped, waiting for start command
2. **RUN_FWD**: Forward rotation at cruise speed
3. **RUN_REV**: Reverse rotation at cruise speed

### Timing Parameters

```cpp
cfg.t.rampUpMs       = 10;     // Motor acceleration time
cfg.t.rampDownMs     = 10;     // Motor deceleration time  
cfg.t.coastBetweenMs = 50;     // Pause between direction changes
cfg.t.forwardRunMs   = 15000;  // Forward run duration (15s)
cfg.t.reverseRunMs   = 15000;  // Reverse run duration (15s)
cfg.cruisePct        = 72.0f;  // Motor speed percentage
```

## Building and Flashing

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code Extension or PlatformIO Core)

### Build Commands

```bash
# For ESP32
pio run -e esp32dev
pio run -e esp32dev -t upload

# For ESP32-C6 
pio run -e esp32c6
pio run -e esp32c6 -t upload

# For ESP8266
pio run -e d1_mini
pio run -e d1_mini -t upload
```

### Serial Monitor
```bash
pio device monitor -e <environment_name>
```

## Usage

1. **Power On**: System initializes and enters IDLE state
2. **Start Processing**: Press the toggle button to begin automatic cycling
3. **Stop Processing**: Press the toggle button again to stop (motor will ramp down safely)
4. **Serial Output**: Connect via USB for status messages and debugging

### Serial Interface

When connected via USB, the system provides:
- Initialization status
- Manual control of start/direction/brake/coast via simple CLI  
- Button press notifications
- Phase transitions
- PWM configuration details
- Set `ENABLE_SERIAL` to 1 in `main.cpp`.

## Configuration

### Motor Speed
Adjust the cruise percentage in `main.cpp`. If using a 100 RPM gear motor,  roughly correlates to RPMs.
```cpp
cfg.cruisePct = 72.0f;  // 72% of maximum PWM
```


### Timing Cycles
Modify timing parameters in `main.cpp`:
```cpp
cfg.t.forwardRunMs = 10000;   // 10 seconds forward
cfg.t.reverseRunMs = 10000;   // 10 seconds reverse
```
The processor alternates between forward and reverse phases continuously until you issue a stop command.



## License

Do whatever you want.
