# STM32 Snake Game 🐍

A fully-featured, hardware-driven Snake game built for the **STM32 Nucleo-F103RB** using the STM32Cube HAL framework and PlatformIO. The game runs on a standard 16x2 I2C LCD and is controlled via an analog joystick. 

It features a non-blocking architecture using hardware timers, ADC polling, and interrupt-driven UART for real-time PC terminal control and telemetry.

## ✨ Features
* **Custom LCD Graphics:** Uses CGRAM custom characters for the snake's head, body, and food.
* **Non-Blocking Architecture:** Game ticks are driven by a `TIM2` hardware timer interrupt (no `HAL_Delay()` freezing).
* **Auto-Detecting I2C Driver:** Automatically scans both `PB8/PB9` (D15/D14) and `PB6/PB7` pins, and probes addresses `0x27` and `0x3F` to find your LCD backpack automatically.
* **Analog Joystick Control:** Dual-channel ADC reads with software deadzone filtering.
* **Live UART Telemetry:** Streams game stats to your PC serial terminal at 9600 baud.
* **Remote Terminal Commands:** Control the game remotely via serial commands (Change speed, pause, reset, check status).
* **State Machine Logic:** Clean transitions between `IDLE`, `PLAYING`, `PAUSED`, and `GAME_OVER`.

## 🛠️ Hardware Requirements
* **Microcontroller:** STM32 Nucleo-F103RB
* **Display:** 16x2 Character LCD with PCF8574 I2C Backpack
* **Controller:** Standard Analog Joystick Module (KY-023 or similar)
* **Components:** Breadboard, Jumper Wires, 2x Pull-up Resistors (e.g., 4.7kΩ or 10kΩ if your LCD backpack lacks them).

## 🔌 Wiring Guide

| Component Pin | Nucleo-F103RB Pin | Notes |
| :--- | :--- | :--- |
| **LCD VCC** | `5V` | The LCD requires 5V to drive the screen contrast properly. |
| **LCD GND** | `GND` | |
| **LCD SCL** | `D15` (PB8) | Add external pull-up resistor to 3.3V if not detected. |
| **LCD SDA** | `D14` (PB9) | Add external pull-up resistor to 3.3V if not detected. |
| **Joystick +5V** | `3V3` | Powered via 3.3V rail so ADC stays within safe 0-3.3V limits. |
| **Joystick GND** | `GND` | |
| **Joystick VRX** | `A0` (PA0) | ADC1 Channel 0 |
| **Joystick VRY** | `A1` (PA1) | ADC1 Channel 1 |
| **User Button** | `PC13` | Using the onboard blue USER button for Start/Pause. |

## 🚀 Installation & Flashing
1. Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO IDE Extension](https://platformio.org/).
2. Clone this repository and open the folder in VSCode.
3. Connect your STM32 Nucleo board via USB.
4. Click the **Upload** button (right arrow icon) in the bottom PlatformIO toolbar.
5. Open the **Serial Monitor** (plug icon) at `9600` baud.

## 🎮 How to Play
* **Start:** Press the blue **USER button** on the Nucleo board to transition from the splash screen to the game.
* **Move:** Push the analog joystick Up, Down, Left, or Right.
* **Pause/Resume:** Press the blue USER button during gameplay.
* **Win Condition:** Fill the entire 16x2 board!

## 💻 UART Serial Console
You can control the game and view real-time stats via the Serial Monitor (`9600` baud, `LF` or `CRLF` endings). 

Send the following commands to the STM32:
* `PAUSE` - Pauses the game.
* `RESUME` - Resumes the game.
* `RESET` - Forces a game over and restarts the board.
* `SPEED <1-5>` - Adjusts the game tick speed (1 is slowest, 5 is fastest). Example: `SPEED 4`
* `STATUS` - Prints the current game state, score, snake length, and speed.

## 🔧 Troubleshooting
* **Screen backlight is on, but shows solid rectangular boxes:** The screen is working, but the contrast is too high. Take a small screwdriver and turn the blue potentiometer screw on the back of the I2C backpack counter-clockwise until text appears.
* **Screen shows solid boxes on the top row only:** The LCD logic is not initializing. Ensure your `VCC` is plugged into `5V`, not `3V3`.
* **Serial Monitor says "Using Addr: 0x27" but the screen is blank:** The I2C connection is verified! Just adjust the contrast potentiometer.
* **Screen doesn't turn on at all:** Check the physical jumper cap on the side of the I2C backpack (often labeled "LED"). If it is missing, the backlight will not illuminate. Bridge those two pins to turn the backlight on.

## 📝 License
This project is open-source and available under the MIT License.