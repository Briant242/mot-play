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

// State variables
uint8_t currentMenu = 0;        
uint8_t settingSelection = 0;   
int touchThreshold = 40;
bool lastTouchState = false;
unsigned long lastDebounceTime = 0;

bool soundEnabled = true;
int themeMode = 0; 

// Animation & Media states
unsigned long lastBlinkTime = 0;
int eyeHeight = 18;
bool isBlinking = false;

String pcSongTitle = "Idle / No Music";
bool isMusicPlaying = false; 

void playStartupMelody() {
  if (!soundEnabled) return;
  int notes[] = {1318, 1568, 1760, 2093}; 
  int dur[]   = {120, 120, 120, 300};

  for (uint8_t i = 0; i < 4; i++) {
    tone(BUZZER_PIN, notes[i], dur[i]);
    delay(dur[i] + 40);
  }
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
  
  display.setCursor(34, 10);
  display.print("MOT PLAY");
  display.setCursor(20, 24);
  display.print("Aesthetic Edition");

  display.drawRect(14, 40, 100, 10, SSD1306_WHITE);
  display.display();

  playStartupMelody();
  delay(150);

  // Boot progress bar
  for (int i = 0; i <= 100; i += 12) {
    display.fillRect(16, 42, i * 0.96, 6, SSD1306_WHITE);
    display.display();
    delay(50);
  }

  // Calibrate touch baseline
  long total = 0;
  for (uint8_t i = 0; i < 10; i++) {
    total += touchRead(TOUCH_PIN);
    delay(20);
  }
  touchThreshold = ((total / 10) * 3) / 4; 
}

void drawHomeMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print("MOT PLAY");
  display.setCursor(84, 2);
  display.print("[1/5]");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  if (isMusicPlaying) {
    uint8_t animTick = (millis() / 200) % 4; 
    
    display.setCursor(14, 18);
    display.print("~ VIBING MODE ~");
    display.setTextSize(2);
    display.setCursor(32, 32);

    if (animTick == 0)      display.print("^ ^");
    else if (animTick == 1) display.print("> <");
    else                    display.print("- -");

    display.setTextSize(1);
    display.setCursor(6, 52);
    display.print("Playing: ");
    display.print(pcSongTitle.substring(0, 12)); 

  } else {
    if (millis() - lastBlinkTime > random(2500, 5000)) {
      isBlinking = true;
      lastBlinkTime = millis();
    }
    
    if (isBlinking) {
      eyeHeight = 2; 
      if (millis() - lastBlinkTime > 150) {
        isBlinking = false;
        eyeHeight = 18; 
      }
    }

    int centerY = 34;
    display.fillRoundRect(30, centerY - (eyeHeight / 2), 24, eyeHeight, 4, SSD1306_WHITE);
    display.fillRoundRect(74, centerY - (eyeHeight / 2), 24, eyeHeight, 4, SSD1306_WHITE);

    display.setCursor(14, 53);
    display.print("Touch to switch >");
  }

  display.display();
}

void drawMusicMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print("SD LYRICS");
  display.setCursor(84, 2);
  display.print("[2/5]");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  display.setCursor(6, 20);
  display.print("Local SD Card:");
  display.setCursor(6, 34);
  display.print("-> Open Doors 2027!");

  display.setCursor(14, 53);
  display.print("Touch to switch >");
  display.display();
}

void drawPcSyncMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print("PC LIVE SYNC");
  display.setCursor(84, 2);
  display.print("[3/5]");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  display.setCursor(6, 18);
  display.print("YT Music / PC Status:");

  display.setTextSize(1);
  display.setCursor(6, 35);
  if (!isMusicPlaying) display.print("[Paused / Idle]");
  else                 display.print(pcSongTitle);

  display.setCursor(14, 53);
  display.print("Touch to switch >");
  display.display();
}

void drawSystemMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print("SYSTEM INFO");
  display.setCursor(84, 2);
  display.print("[4/5]");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  display.setCursor(6, 18);
  display.print("MCU: ESP32-D0WD-V3");
  display.setCursor(6, 30);
  display.print("RAM Free: ~295 KB");
  display.setCursor(6, 42);
  display.print("Profile: Jeryco");

  display.setCursor(14, 53);
  display.print("Touch to switch >");
  display.display();
}

void drawSettingsMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print("USER SETTINGS");
  display.setCursor(84, 2);
  display.print("[5/5]");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  display.setCursor(2, 16);
  display.print(settingSelection == 0 ? "> " : "  ");
  display.print("Sound FX: ");
  display.print(soundEnabled ? "ON" : "OFF");

  display.setCursor(2, 28);
  display.print(settingSelection == 1 ? "> " : "  ");
  display.print("Theme: ");
  display.print(themeMode == 0 ? "Dark" : "Light");

  display.setCursor(2, 40);
  display.print(settingSelection == 2 ? "> " : "  ");
  display.print("-> Exit to Home");

  display.setCursor(4, 54);
  display.print("Tap:Pilih | Hold:Action");
  display.display();
}

void loop() {
  if (Serial.available() > 0) {
    String incoming = Serial.readStringUntil('\n');
    incoming.trim();
    
    if (incoming == "PAUSE") {
      isMusicPlaying = false; 
      pcSongTitle = "Paused";
    } else if (incoming.length() > 0) {
      pcSongTitle = incoming;
      isMusicPlaying = true; 
    }
  }

  int touchVal = touchRead(TOUCH_PIN);
  bool isTouched = (touchVal < touchThreshold && touchVal > 0);

  if (isTouched && !lastTouchState) {
    delay(250); 
    
    if (touchRead(TOUCH_PIN) < touchThreshold) { 
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
        
        if (soundEnabled && currentMenu != 0) {
          tone(BUZZER_PIN, 2500, 100);
          delay(120);
          noTone(BUZZER_PIN);
        }
      }
    } else {
      if (millis() - lastDebounceTime > 350) {
        if (currentMenu == 4) {
          settingSelection = (settingSelection + 1) % 3;
        } else {
          currentMenu = (currentMenu + 1) % 5; 
        }
        
        if (soundEnabled) {
          tone(BUZZER_PIN, 1800, 40);
          delay(50);
          noTone(BUZZER_PIN);
        }
        lastDebounceTime = millis();
      }
    }
  }
  lastTouchState = isTouched;

  switch(currentMenu) {
    case 0: drawHomeMenu(); delay(50); break;
    case 1: drawMusicMenu(); delay(100); break;
    case 2: drawPcSyncMenu(); delay(100); break;
    case 3: drawSystemMenu(); delay(100); break;
    case 4: drawSettingsMenu(); delay(100); break;
  }
}
