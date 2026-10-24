#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define CUSTOM_SDA    26
#define CUSTOM_SCL    25
#define TOUCH_PIN     27   
#define BUZZER_PIN    14  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Preferences prefs;

// Wi-Fi credentials for remote GitHub OTA updates (Change to your network)
const char* wifiSsid = "OmaTik";
const char* wifiPassword = "212430jc";

// Direct GitHub Release binary download URL for automated OTA updates
const String firmwareUrl = "https://github.com/Briant242/mot-play/releases/latest/download/firmware.bin";

uint8_t currentMenu = 0;        
uint8_t settingSelection = 0;   
int touchThreshold = 90; // Auto-calibrated in setup
bool lastTouchState = false;

unsigned long touchStartTime = 0;
bool waitingForRelease = false;

bool soundEnabled = true;
int themeMode = 0; 

// Virtual Pet & Mood state variables
unsigned long lastAnimUpdate = 0;
int animStep = 0;
int targetEyeHeight = 18;
uint8_t petEnergy = 85; 

// Media state
bool isMusicPlaying = false; 

void playTone(int freq, int duration) {
  if (!soundEnabled) return;
  tone(BUZZER_PIN, freq, duration);
  delay(duration + 15);
  noTone(BUZZER_PIN);
}

// Perform automated Over-The-Air firmware update from GitHub Releases with OLED notifications
void performGitHubOTA() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Notification: Connecting Wi-Fi
  display.setCursor(16, 20);
  display.print("CONNECTING");
  display.setCursor(16, 34);
  display.print("WIFI...");
  display.display();

  // Make sure to fill in your actual Wi-Fi SSID and Password here!
  WiFi.begin(wifiSsid, wifiPassword);
  
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
    delay(500);
    display.print(".");
    display.display();
  }

  // Notification: Connection Failed check
  if (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(22, 28);
    display.print("WIFI FAILED!");
    display.display();
    playTone(1000, 200); // Error beep
    delay(2000);
    WiFi.disconnect(true);
    return;
  }

  // Notification: Connected & Downloading
  display.clearDisplay();
  display.setCursor(24, 20);
  display.print("CONNECTED!");
  display.setCursor(16, 36);
  display.print("DOWNLOADING...");
  display.display();
  playTone(2093, 100); // Success beep

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(firmwareUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (Update.begin(contentLength)) {
      WiFiClient *clientStream = http.getStreamPtr();
      size_t written = Update.writeStream(*clientStream);
      
      if (written == contentLength) {
        if (Update.end()) {
          if (Update.isFinished()) {
            display.clearDisplay();
            display.setCursor(24, 28);
            display.print("UPDATE OK!");
            display.display();
            delay(2000);
            ESP.restart();
          }
        }
      }
    }
  } else {
    display.clearDisplay();
    display.setCursor(20, 28);
    display.print("HTTP ERROR!");
    display.display();
    delay(2000);
  }
  
  http.end();
  WiFi.disconnect(true);
}

// Playful startup jingle & SaaS Mograph Intro Sequence
void playSaaSIntro() {
  display.clearDisplay();
  
  // Playful startup chime melody
  playTone(1318, 100); 
  playTone(1567, 100); 
  playTone(2093, 180); 

  // Step 1: Display "MOT PLAY" title clearly
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 24);
  display.print("MOT PLAY");
  display.display();
  delay(1000);

  // Step 2: Display "designed by" subtitle
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(32, 28);
  display.print("Designed by");
  display.display();
  delay(900);

  // Step 3: SaaS-style mograph expanding bounding box
  for (int w = 4; w <= 120; w += 10) {
    display.clearDisplay();
    display.drawRect((128 - w) / 2, 22, w, 20, SSD1306_WHITE);
    display.display();
    delay(8);
  }

  // Flash effect for modern tech aesthetic
  display.fillRect(4, 22, 120, 20, SSD1306_WHITE);
  display.display();
  delay(25);

  display.clearDisplay();
  display.drawRect(4, 20, 120, 24, SSD1306_WHITE);
  display.display();
  delay(40);

  // Step 4: Render crisp GitHub typography "< briant242 />"
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(24, 28);
  display.print("< Briant242 />");
  display.display();

  playTone(2637, 140);
  delay(1800); 
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

  // Apply saved theme preference (Dark/Light)
  display.invertDisplay(themeMode == 1);

  // Run startup jingle & SaaS mograph intro
  playSaaSIntro();

  // Keep hands away briefly for accurate touch baseline calibration
  delay(400);

  long total = 0;
  for (uint8_t i = 0; i < 10; i++) {
    total += touchRead(TOUCH_PIN);
    delay(20);
  }
  touchThreshold = ((total / 10) * 3) / 4; 
}

// Minimalist top navigation dots across all menus
void drawMinimalNav() {
  for (int i = 0; i < 5; i++) {
    if (i == currentMenu) {
      display.fillRect(48 + (i * 7), 2, 4, 2, SSD1306_WHITE);
    } else {
      display.drawPixel(50 + (i * 7), 3, SSD1306_WHITE);
    }
  }
}

// Menu 0: Home / Virtual Pet Mood Station & Music Dancing Mode
void drawHomeMenu() {
  display.clearDisplay();
  drawMinimalNav();

  if (isMusicPlaying) {
    unsigned long eqMillis = millis() / 70;
    for (int i = 0; i < 10; i++) {
      int h = (int)(abs(sin((eqMillis + i) * 0.8)) * 28) + 4;
      display.fillRect(14 + (i * 10), 52 - h, 6, h, SSD1306_WHITE);
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

    display.drawRect(34, 56, 60, 4, SSD1306_WHITE);
    display.fillRect(36, 58, (petEnergy * 56) / 100, 2, SSD1306_WHITE);
  }

  display.display();
}

void drawMusicMenu() {
  display.clearDisplay();
  drawMinimalNav();
  display.drawRect(50, 20, 28, 32, SSD1306_WHITE);
  display.fillRect(58, 16, 12, 4, SSD1306_WHITE);
  display.setCursor(44, 55);
  display.print("SD AUDIO");
  display.display();
}

void drawWeatherMenu() {
  display.clearDisplay();
  drawMinimalNav();
  display.drawCircle(54, 30, 10, SSD1306_WHITE);
  display.fillCircle(66, 34, 8, SSD1306_WHITE);
  display.fillRect(56, 36, 18, 6, SSD1306_WHITE);
  display.setCursor(46, 54);
  display.print("28 C, SUN");
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
    if (incoming == "PLAY") {
      isMusicPlaying = true; 
    } else if (incoming == "STOP") {
      isMusicPlaying = false; 
    } else if (incoming == "OTA") {
      performGitHubOTA(); // Type "OTA" in Serial Monitor to trigger remote update test
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
    case 2: drawWeatherMenu(); delay(100); break;
    case 3: drawSystemMenu(); delay(100); break;
    case 4: drawSettingsMenu(); delay(100); break;
  }
}
