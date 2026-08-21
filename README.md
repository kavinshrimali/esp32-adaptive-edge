# esp32-adaptive-edge

An energy-efficient ESP32 IoT edge vision node that dynamically adjusts its frame rate and sleep states based on PIR-interrupts and real-time pixel delta tracking.

## Overview

In edge IoT networks, excess power is consumed by components that aren't in active use. Standard motion-powered systems are typically binary (all-on or all-off), and lack numerous states to account for varying motion complexity. 

This project implements the following power-management architecture:
1. A PIR sensor acts as an external interrupt, keeping the ESP32 in low-power light sleep until motion is detected.
2. Once awake, the camera processes downsampled grayscale images to calculate real-time pixel deltas. The program dynamically adjusts the camera's frame rate based on the motion's complexity (ranging from 0 ms to 300 ms), and decays back to light sleep when there's a pause of 5000 ms between consecutive image captures. 

## State transitions

IDLE: Light sleep mode awaiting a PIR interrupt on `GPIO 14`.
IDLE -> ACTIVE: Triggered immediately when motion is detected.
ACTIVE: Frames are captured and processed as a 1D array. Frame rate dynamically adjusted based on calculated pixel deltas.
ACTIVE -> DECAY: Occurs after a 5000 ms gap with no detected motion.
DECAY: Intermittent frame capture (every 500 ms). If motion is detected, the state switches back to ACTIVE. If no motion occurs for more than 3000 ms, state changes to IDLE.
DECAY -> IDLE: System re-enters light sleep mode.

## Algorithm Breakdown:

1. **Interrupt and Wakeup:** The PIR Sensor drives `GPIO 14` HIGH, waking the ESP32 core via `esp_light_sleep_start()` and `gpio_wakeup_enable()`.
2. **Frame Capture:** Grayscale frames (640 x 480) are processed as 1D arrays.
3. **Subsampling:** To avoid looping over all 307,200 bytes, the algorithm samples every 16th byte (`STRIDE` = 16) for a total of 19,200 bytes to reduce processing times.
4. **Pixel Delta Calculation:**
   * The first frame populates the array `prevPixels[]` as an initial baseline.
   * From the second frame onwards, the algorithm calculates the difference between grayscale pixel values and their corresponding pixels in the `prevPixels[]` array. The difference is evaluated against a `NOISE` value; if the difference exceeds the `NOISE` value, `totalDelta` increments by 1.
5. **Adaptive Frame Rate Adjustment:**
   * If `totalDelta` >= `HIGH_MOTION_THRESHOLD`: `frameDelay` = 0 ms (Maximum frame rate)
   * If `totalDelta` >= `LOW_MOTION_THRESHOLD`: `frameDelay` = 150 ms
   * If `totalDelta` < `LOW_MOTION_THRESHOLD`: `frameDelay` = 300 ms (Minimum frame rate)

## Key design decisions
* **Fixed Frame Size (VGA):** Flipping between VGA (640 x 480) and QQVGA (160 x 120) framesizes creates register lockups and desynchronizes DMA (Direct Memory Access) descriptor chains.
* **Enabling PSRAM:** Using PSRAM (Pseudo-Static RAM) over DRAM (Data RAM) allows for continuous image processing. This is because enabling PSRAM allows for the `fb_count` to be set to 2 rather than 1 (as in the case of DRAM). As a result, while the algorithm runs the pixel delta tracking on the first image, it captures and stores the next frame, allowing for smoother processing.
* **Power Supply Buffering (100 µF Capacitor):** The PIR sensor is extremely sensitive to voltage changes in the power rail. The capacitor prevents false PIR interrupts by smoothing out voltage changes caused by changing power consumption. The capacitor also smooths out current changes that can cause the camera to brownout and reboot.

## Hardware Setup and Wiring

### _Bill of Materials_
* AI-Thinker ESP32-CAM
* AM312 Mini PIR Motion Sensor
* ESP32-CAM-MB (FTDI Programmer Shield)
* 100 μF Electrolytic Capacitor
* Half-Size Breadboard and Jumper Cables

### _Pinout Mapping_
* **PIR Sensor (AM312):**
  * `VCC` -> Positive (`+`) power rail
  * `GND` -> Negative (`-`) power rail
  * `VOUT` -> `GPIO14`

* **FTDI Programmer / ESP32-CAM-MB:**
  * `5V` -> Positive (+) power rail
  * `GND` -> Negative (-) power rail
  * `TX` -> `U0R` (`GPIO 3`) of ESP32-CAM
  * `RX` -> `U0T` (`GPIO 1`) of ESP-32 CAM

* **ESP32-CAM**
  * `5V` -> Positive (+) power rail
  * `GND` -> Negative (-) power rail
  * `U0R` (`GPIO 3`) -> `U0T` of ESP32-CAM-MB
  * `U0T` (`GPIO 1`) -> `U0R` of ESP32-CAM-MB
  * `GPIO 14` -> PIR Sensor `VOUT`
  * `IO0` -> Negative (-) power rail (Jumper in place during boot to flash; remove for normal run)

* **Decoupling Capacitor**
  * Long lead (+) -> Positive (+) power rail
  * Short lead (-) -> Negative (-) power rail

## Empirical Data and Impact
### _Current draw for all states_
  **IDLE:** ~30 mA | Light Sleep, awaiting PIR interrupt
  **ACTIVE:** ~ 100 mA | Frame Capture and Pixel Delta Tracking
  **DECAY:** ~ 30 mA | Awaiting motion before returning to light sleep

**Power Reduction:** Multi-state throttling yields a **70% reduction in current draw** during IDLE periods, as compared to ACTIVE periods.

## Build and Flash Configuration

* **Board:** AI Thinker ESP32-CAM
* **Flash Frequency:** 40 MHz
* **Flash Mode:** DIO
* **Partition Scheme:** HUGE APP (3MB No OTA/1MB SPIFFS)
* **PSRAM:** Enabled
* **Baud Rate:** 115200

## Flash Walkthrough
1. Ensure IO0 on the ESP32-CAM is connected to the Negative (`-`) power rail. This puts the board in flashing mode.
2. Connect the FTDI Programmer to your computer.
3. In the Arduino IDE, apply the settings above and select the correct Port.
4. Power cycle the ESP32-CAM by disconnecting and re-connecting its 5V jumper.
5. Upload the code (either `sensor_test_code.ino` or `camera_project_code.ino`). 
6. After the upload finishes, disconnect IO0 from the Negative (`-`) power rail
7. Power cycle the ESP32-CAM once more (disconnect and re-connect the 5V jumper) to boot into normal run mode.

## Repository Structure

* `sensor_test_code.ino`: Calibration sketch used to establish baseline sensor noise (`NOISE`), `LOW_MOTION_THRESHOLD`, and `HIGH_MOTION_THRESHOLD` on a static scene.
* `camera_project_code.ino`: Firmware containing three-state machine, PIR interrupt handler, and adaptive frame rate processing.
