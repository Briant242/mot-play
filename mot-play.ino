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

// Network & OTA config
const char* wifiSsid = "OmaTik";
const char* wifiPassword = "212430jc";
const String firmwareUrl = "https://github.com/Briant242/mot-play/releases/latest/download/firmware.bin";
const String currentVersion = "v1.0.0"; // Local firmware version tracking

// Weather API config (OpenWeatherMap)
const char* weatherApiKey = "e85787b881413edea43ef2971ff41aad";
String weatherTemp = "--";
String weatherCond = "DETECTING";
unsigned long lastWeatherFetch = 0;
const unsigned long weatherInterval = 600000; // Fetch every 10 mins

uint8_t currentMenu = 0;        
uint8_t settingSelection = 0;   
int touchThreshold = 90; // Auto-calibrated on boot
bool lastTouchState = false;

unsigned long touchStartTime = 0;
bool waitingForRelease = false;

bool soundEnabled = true;
int themeMode = 0; 

// Virtual pet & state vars
unsigned long lastAnimUpdate = 0;
int animStep = 0;
int targetEyeHeight = 18;
uint8_t petEnergy = 85; 

bool isMusicPlaying = false; 

void playTone(int freq, int duration) {
  if (!soundEnabled) return;
  tone(BUZZER_PIN, freq, duration);
  delay(duration + 15);
  noTone(BUZZER_PIN);
}

// Pull and flash firmware from GitHub release
void performGitHubOTA() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(16, 20);
  display.print("DOWNLOADING");
  display.setCursor(16, 34);
  display.print("UPDATE...");
  display.display();
  playTone(2093, 100);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(firmwareUrl);
  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    int len = http.getSize();
    if (Update.begin(len)) {
      WiFiClient *stream = http.getStreamPtr();
      size_t written = Update.writeStream(*stream);
      
      if (written == len && Update.end() && Update.isFinished()) {
        display.clearDisplay();
        display.setCursor(24, 28);
        display.print("UPDATE OK!");
        display.display();
        delay(2000);
        ESP.restart();
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

// Check GitHub release tag before triggering OTA download
void checkForUpdateAndOTA() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(16, 20);
  display.print("CHECKING");
  display.setCursor(16, 34);
  display.print("UPDATES...");
  display.display();

  WiFi.begin(wifiSsid, wifiPassword);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(22, 28);
    display.print("WIFI FAILED!");
    display.display();
    playTone(1000, 200);
    delay(2000);
    WiFi.disconnect(true);
    return;
  }

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("ESP32-MOTPLAY");
  http.begin("https://api.github.com/repos/Briant242/mot-play/releases/latest");
  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    int tagIdx = payload.indexOf("\"tag_name\":\"");
    if (tagIdx != -1) {
      int startQ = tagIdx + 12;
      int endQ = payload.indexOf("\"", startQ);
      if (endQ != -1) {
        String latestTag = payload.substring(startQ, endQ);
        
        if (latestTag == currentVersion) {
          display.clearDisplay();
          display.setCursor(22, 28);
          display.print("UP TO DATE!");
          display.display();
          playTone(1500, 100);
          delay(2000);
          http.end();
          WiFi.disconnect(true);
          return; // Return back to settings menu if no updates found
        }
      }
    }
  }
  
  http.end();
  performGitHubOTA(); // Proceed to download if new version exists
}

// Grab IP location & parse OpenWeather JSON manually
void fetchLocationAndWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(wifiSsid, wifiPassword);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
      delay(500);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Auto-detect city via IP
    http.begin("http://ip-api.com/json/?fields=status,city");
    int code = http.GET();
    String city = "Lumajang";

    if (code == HTTP_CODE_OK) {
      String payload = http.getString();
      int idx = payload.indexOf("\"city\":\"");
      if (idx != -1) {
        int startQ = idx + 8;
        int endQ = payload.indexOf("\"", startQ);
        if (endQ != -1) city = payload.substring(startQ, endQ);
      }
    }
    http.end();

    // Fetch weather data for detected city
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&units=metric&appid=" + String(weatherApiKey);
    http.begin(url);
    int wCode = http.GET();

    if (wCode == HTTP_CODE_OK) {
      String payload = http.getString();
      
      int tIdx = payload.indexOf("\"temp\":");
      if (tIdx != -1) {
        int cIdx = payload.indexOf(",", tIdx);
        if (cIdx != -1) {
          weatherTemp = payload.substring(tIdx + 7, cIdx);
          int dot = weatherTemp.indexOf('.');
          if (dot != -1 && dot + 2 < weatherTemp.length()) {
            weatherTemp = weatherTemp.substring(0, dot + 2);
          }
        }
      }

      int mIdx = payload.indexOf("\"main\":\"");
      if (mIdx != -1) {
        int sQ = mIdx + 8;
        int eQ = payload.indexOf("\"", sQ);
        if (eQ != -1) {
          weatherCond = payload.substring(sQ, eQ);
          weatherCond.toUpperCase();
        }
      }
    }
    
    http.end();
    WiFi.disconnect(true);
  }
}

// Startup jingle & boot logo animation
void playSaaSIntro() {
  display.clearDisplay();
  
  playTone(1318, 100); 
  playTone(1567, 100); 
  playTone(2093, 180); 

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 24);
  display.print("MOT PLAY");
  display.display();
  delay(1000);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(32, 28);
  display.print("Designed by");
  display.display();
  delay(900);

  for (int w = 4; w <= 120; w += 10) {
    display.clearDisplay();
    display.drawRect((128 - w) / 2, 22, w, 20, SSD1306_WHITE);
    display.display();
    delay(8);
  }

  display.fillRect(4, 22, 120, 20, SSD1306_WHITE);
  display.display();
  delay(25);

  display.clearDisplay();
  display.drawRect(4, 20, 120, 24, SSD1306_WHITE);
  display.display();
  delay(40);

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

  display.invertDisplay(themeMode == 1);
  playSaaSIntro();

  delay(400);

  long total = 0;
  for (uint8_t i = 0; i < 10; i++) {
    total += touchRead(TOUCH_PIN);
    delay(20);
  }
  touchThreshold = ((total / 10) * 3) / 4; 
}

void drawMinimalNav() {
  for (int i = 0; i < 5; i++) {
    if (i == currentMenu) {
      display.fillRect(48 + (i * 7), 2, 4, 2, SSD1306_WHITE);
    } else {
      display.drawPixel(50 + (i * 7), 3, SSD1306_WHITE);
    }
  }
}

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

  if (millis() - lastWeatherFetch > weatherInterval || lastWeatherFetch == 0) {
    fetchLocationAndWeather();
    lastWeatherFetch = millis();
  }

  if (weatherCond.indexOf("SUN") != -1 || weatherCond.indexOf("CLEAR") != -1) {
    display.fillCircle(64, 28, 9, SSD1306_WHITE); 
  } else if (weatherCond.indexOf("RAIN") != -1) {
    display.drawRect(54, 22, 20, 12, SSD1306_WHITE); 
    display.fillRect(58, 36, 2, 6, SSD1306_WHITE);
    display.fillRect(66, 36, 2, 6, SSD1306_WHITE);
  } else {
    display.drawCircle(58, 28, 8, SSD1306_WHITE); 
    display.fillCircle(68, 30, 6, SSD1306_WHITE);
  }

  display.setTextSize(1);
  display.setCursor(16, 52);
  display.print(weatherTemp + " C, " + weatherCond);
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
  
  // Item 0: Sound
  display.setCursor(12, 16);
  display.print(settingSelection == 0 ? "> SND: " : "  SND: ");
  display.print(soundEnabled ? "ON" : "OFF");
  
  // Item 1: Theme
  display.setCursor(12, 28);
  display.print(settingSelection == 1 ? "> THM: " : "  THM: ");
  display.print(themeMode == 0 ? "DRK" : "LGT");
  
  // Item 2: OTA Update Check
  display.setCursor(12, 40);
  display.print(settingSelection == 2 ? "> OTA: CHECK" : "  OTA: CHECK");

  // Item 3: Home / Back
  display.setCursor(12, 52);
  display.print(settingSelection == 3 ? "> [HOME]" : "  [HOME]");
  
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
        settingSelection = (settingSelection + 1) % 4; // Cycle through 4 settings options
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
          checkForUpdateAndOTA(); // Trigger interactive OTA check from settings menu
        } else if (settingSelection == 3) {
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
