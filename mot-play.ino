#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define CUSTOM_SDA    26
#define CUSTOM_SCL    25
#define TOUCH_PIN     27   
#define BUZZER_PIN    14  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Preferences prefs;

uint8_t currentMenu = 0;        
uint8_t settingSelection = 0;   
int touchThreshold = 90; // Will be auto-calibrated in setup
bool lastTouchState = false;

unsigned long touchStartTime = 0;
bool waitingForRelease = false;

bool soundEnabled = true;
int themeMode = 0; 

unsigned long lastAnimUpdate = 0;
int animStep = 0;
int targetEyeHeight = 18;

String pcSongTitle = "Idle";
bool isMusicPlaying = false; 

void playTone(int freq, int duration) {
  if (!soundEnabled) return;
  tone(BUZZER_PIN, freq, duration);
  delay(duration + 20);
  noTone(BUZZER_PIN);
}

void setup() {
  Serial.begin(9600); 
  pinMode(BUZZER_PIN, OUTPUT);
  
  prefs.begin("motplay_pref", false);
  soundEnabled = prefs.getBool("sound", true);
  themeMode = prefs.getInt("theme", 0);

  Wire.begin(CUSTOM_SDA, CUSTOM_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  display.invertDisplay(themeMode == 1);
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(38, 28);
  display.print("MOT PLAY");
  display.display();

  // Keep hands away during boot for accurate touch baseline calibration
  delay(1500);

  // Auto-calibrate touch baseline (reads un-touched state and sets threshold)
  long total = 0;
  for (uint8_t i = 0; i < 10; i++) {
    total += touchRead(TOUCH_PIN);
    delay(20);
  }
  // Set threshold to ~75% of the baseline average (e.g. if baseline is 135, threshold is ~101)
  touchThreshold = ((total / 10) * 3) / 4; 

  playTone(2093, 150);
}

// Helper for minimalist top navigation dots across all menus
void drawMinimalNav() {
  for (int i = 0; i < 5; i++) {
    if (i == currentMenu) {
      display.fillRect(48 + (i * 7), 2, 4, 2, SSD1306_WHITE);
    } else {
      display.drawPixel(50 + (i * 7), 3, SSD1306_WHITE);
    }
  }
}

// Menu 0: Home / Idle Playful Eyes & Vibing Equalizer
void drawHomeMenu() {
  display.clearDisplay();
  drawMinimalNav();

  if (isMusicPlaying) {
    unsigned long eqMillis = millis() / 80;
    for (int i = 0; i < 10; i++) {
      int h = (int)(abs(sin((eqMillis + i) * 0.7)) * 26) + 4;
      display.fillRect(14 + (i * 10), 50 - h, 6, h, SSD1306_WHITE);
    }
  } else {
    if (millis() - lastAnimUpdate > 900) {
      animStep = (animStep + 1) % 4;
      lastAnimUpdate = millis();
    }

    int centerY = 32;
    int lX = 32;
    int rX = 72;
    int eW = 24;

    if (animStep == 1) {
      targetEyeHeight = 4;
    } else if (animStep == 2) {
      targetEyeHeight = 18;
      lX = 36; rX = 76;
    } else if (animStep == 3) {
      targetEyeHeight = 18;
      lX = 28; rX = 68;
    } else {
      targetEyeHeight = 18;
    }

    display.fillRoundRect(lX, centerY - (targetEyeHeight / 2), eW, targetEyeHeight, 4, SSD1306_WHITE);
    display.fillRoundRect(rX, centerY - (targetEyeHeight / 2), eW, targetEyeHeight, 4, SSD1306_WHITE);
  }

  display.display();
}

void drawMusicMenu() {
  display.clearDisplay();
  drawMinimalNav();
  display.drawRect(50, 20, 28, 32, SSD1306_WHITE);
  display.fillRect(58, 16, 12, 4, SSD1306_WHITE);
  display.setCursor(44, 55);
  display.print("SD READY");
  display.display();
}

void drawPcSyncMenu() {
  display.clearDisplay();
  drawMinimalNav();
  display.drawCircle(64, 32, 16, SSD1306_WHITE);
  display.drawCircle(64, 32, 8, SSD1306_WHITE);
  display.fillCircle(64, 32, 2, SSD1306_WHITE);
  display.setCursor(isMusicPlaying ? 20 : 42, 54);
  display.print(isMusicPlaying ? pcSongTitle.substring(0, 14) : "SYNC IDLE");
  display.display();
}

void drawSystemMenu() {
  display.clearDisplay();
  drawMinimalNav();
  display.drawRect(34, 24, 60, 20, SSD1306_WHITE);
  display.fillRect(94, 29, 4, 10, SSD1306_WHITE);
  display.fillRect(38, 28, 40, 12, SSD1306_WHITE);
  display.setCursor(48, 54);
  display.print("ESP32");
  display.display();
}

void drawSettingsMenu() {
  display.clearDisplay();
  drawMinimalNav();
  display.setTextSize(1);
  display.setCursor(16, 20);
  display.print(settingSelection == 0 ? "> SND: " : "  SND: ");
  display.print(soundEnabled ? "ON" : "OFF");
  display.setCursor(16, 34);
  display.print(settingSelection == 1 ? "> THM: " : "  THM: ");
  display.print(themeMode == 0 ? "DRK" : "LGT");
  display.setCursor(16, 48);
  display.print(settingSelection == 2 ? "> [HOME]" : "  [HOME]");
  display.display();
}

void loop() {
  if (Serial.available() > 0) {
    String incoming = Serial.readStringUntil('\n');
    incoming.trim();
    if (incoming == "PAUSE") {
      isMusicPlaying = false; 
      pcSongTitle = "Idle";
    } else if (incoming.length() > 0) {
      pcSongTitle = incoming;
      isMusicPlaying = true; 
    }
  }

  int touchVal = touchRead(TOUCH_PIN);
  bool isTouched = (touchVal < touchThreshold && touchVal > 0);

  if (isTouched && !lastTouchState) {
    touchStartTime = millis();
    waitingForRelease = true;
  }

  if (waitingForRelease && (!isTouched || (millis() - touchStartTime > 700))) {
    unsigned long duration = millis() - touchStartTime;
    waitingForRelease = false;

    if (duration < 500) {
      if (currentMenu == 4) {
        settingSelection = (settingSelection + 1) % 3;
      } else {
        currentMenu = (currentMenu + 1) % 5; 
      }
      playTone(1800, 40);
    } else {
      if (currentMenu == 4) {
        if (settingSelection == 0) {
          soundEnabled = !soundEnabled;
          prefs.putBool("sound", soundEnabled);
        } else if (settingSelection == 1) {
          themeMode = !themeMode;
          prefs.putInt("theme", themeMode);
          display.invertDisplay(themeMode == 1);
        } else if (settingSelection == 2) {
          currentMenu = 0;
          settingSelection = 0; 
        }
        playTone(2500, 80);
      }
    }
  }
  lastTouchState = isTouched;

  switch(currentMenu) {
    case 0: drawHomeMenu(); delay(40); break;
    case 1: drawMusicMenu(); delay(100); break;
    case 2: drawPcSyncMenu(); delay(100); break;
    case 3: drawSystemMenu(); delay(100); break;
    case 4: drawSettingsMenu(); delay(100); break;
  }
}
