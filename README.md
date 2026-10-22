<div align="center">
  <!-- Main Banner -->
  <img src="https://img.sanishtech.com/u/091216e18840e4fecc9a81d3fe67aa6c.png" width="100%" alt="MOT PLAY Banner">
</div>

<div align="center">
  <!-- MOT PLAY Logo / Icon -->
  <img src="https://img.sanishtech.com/u/9dacc7331aac7704eac10394d33c9c15.png" width="120" alt="MOT PLAY Logo" style="border-radius: 50%;">
  
  <h3>MOT PLAY</h3>
  <p><em>Interactive ESP32 Desktop Companion & Rhythm-Synced Audio Player</em></p>
</div>

---

## Overview
**MOT PLAY** is an interactive desktop companion powered by an ESP32 microcontroller. Designed with a clean cyber-minimalist aesthetic, it features dynamic OLED eye animations, capacitive touch controls, and local audio playback to bring both utility and character to your workspace.

---

## Hardware & Tech Stack
* **Microcontroller:** ESP32 DevKit / NodeMCU
* **Display:** 0.96" OLED Display (SSD1306 Driver)
* **Audio Module:** MP3 Decoder / MicroSD Player Module
* **Input Control:** Capacitive Touch Sensor (Pin D27)
* **Language & Framework:** C++ (Arduino IDE / PlatformIO)

---

## Features Roadmap
- [x] **Core UI & Dynamic Idle Eyes:** Blinking animations and idle expression states on the OLED screen.
- [ ] **Interactive Touch Slider:** Visual volume and brightness progress bar via capacitive touch.
- [ ] **Pomodoro Focus Timer:** 25/5-minute productivity countdown mode with focused eye expressions.
- [ ] **White Noise Generator:** Ambient sound playback to assist with deep focus and studying.
- [ ] **Tamagotchi Companion Mode:** Virtual pet behaviors responding to idle duration.
- [ ] **Vibing Flash Mode:** Rhythm-synced dark-to-white theme flash during active music playback.

---

## Project Structure
```text
mot-play/
├── src/
│   ├── main.cpp         # Main state-machine & logic loop
│   ├── animations.h     # OLED eye frames & expressions
│   └── audio_ctrl.h     # MP3 module serial communication
├── assets/              # Schematic & reference images
├── LICENSE              # MIT License
└── README.md
