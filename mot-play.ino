#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <PubSubClient.h>
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

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Network & Cloud config
String wifiSsid = "";
String wifiPassword = "";
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
String deviceId = ""; // Unique Device ID generated from Chip ID

const String firmwareUrl = "https://github.com/Briant242/mot-play/releases/latest/download/firmware.bin";
const String currentVersion = "v1.4.0"; 

// Weather API config
const char* weatherApiKey = "e85787b881413edea43ef2971ff41aad";
String weatherTemp = "--";
String weatherCond = "READY";
unsigned long lastWeatherFetch = 0;
const unsigned long weatherInterval = 600000; 

uint8_t currentMenu = 0;        
uint8_t settingSelection = 0;   
int touchThreshold = 90; 
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
uint8_t petMood = 0; // 0: Normal, 1: Happy, 2: Sleepy, 3: Angry

bool isMusicPlaying = false; 

void playTone(int freq, int duration) {
  if (!soundEnabled) return;
  tone(BUZZER_PIN, freq, duration);
  delay(duration + 15);
  noTone(BUZZER_PIN);
}

// MQTT Message Callback (Incoming commands from Cloud Web)
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  if (message == "toggle_sound") {
    soundEnabled = !soundEnabled;
    prefs.putBool("sound", soundEnabled);
  } else if (message == "toggle_theme") {
    themeMode = !themeMode;
    prefs.putInt("theme", themeMode);
    display.invertDisplay(themeMode == 1);
  } else if (message.startsWith("mood:")) {
    petMood = message.substring(5).toInt();
  }
  playTone(2200, 50);
}

void reconnectMqtt() {
  if (!mqttClient.connected()) {
    String clientId = "MotPlayClient-" + deviceId;
    if (mqttClient.connect(clientId.c_str())) {
      String subTopic = "motplay/device/" + deviceId + "/cmd";
      mqttClient.subscribe(subTopic.c_str());
    }
  }
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

  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
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
          return; 
        }
      }
    }
  }
  
  http.end();
  performGitHubOTA(); 
}

void fetchLocationAndWeather() {
  if (wifiSsid.length() == 0) return;
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
    delay(3000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
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
  }
}

void startWifiConfigPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("MOT-PLAY-SETUP");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(12, 16);
  display.print("AP: MOT-PLAY");
  display.setCursor(12, 30);
  display.print("IP: 192.168.4.1");
  display.setCursor(12, 46);
  display.print("CONFIG WIFI");
  display.display();
  playTone(1318, 100);

  // Minimal AP web server just for WiFi credentials
  WebServer apServer(80);
  apServer.on("/", HTTP_GET, [&]() {
    String html = "<html style='font-family:sans-serif;background:#0d0d12;color:#fff;padding:20px;'>";
    html += "<h2 style='color:#a29bfe;'>MOT PLAY - WiFi Setup</h2>";
    html += "<form action='/save' method='POST'>";
    html += "SSID:<br><input type='text' name='ssid' style='width:100%;padding:10px;margin:10px 0;background:#1b1b2f;color:#fff;border:1px solid #444;'><br>";
    html += "Password:<br><input type='password' name='pass' style='width:100%;padding:10px;margin:10px 0;background:#1b1b2f;color:#fff;border:1px solid #444;'><br>";
    html += "<input type='submit' value='Save & Connect' style='width:100%;padding:12px;background:#6c5ce7;color:#fff;border:none;font-weight:bold;'>";
    html += "</form></html>";
    apServer.send(200, "text/html", html);
  });
  apServer.on("/save", HTTP_POST, [&]() {
    wifiSsid = apServer.arg("ssid");
    wifiPassword = apServer.arg("pass");
    prefs.putString("wifi_ssid", wifiSsid);
    prefs.putString("wifi_pass", wifiPassword);
    apServer.send(200, "text/html", "<h2 style='color:#55efc4;text-align:center;'>Saved! Rebooting...</h2>");
    delay(2000);
    ESP.restart();
  });
  apServer.begin();
  while (true) {
    apServer.handleClient();
    delay(2);
  }
}

void playSaaSIntro() {
  display.clearDisplay();
  playTone(1318, 100); playTone(1567, 100); playTone(2093, 180); 
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

  display.clearDisplay();
  display.drawRect(4, 20, 120, 24, SSD1306_WHITE);
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

  // Generate Unique Device ID from ESP32 Chip MAC Address
  uint64_t chipid = ESP.getEfuseMac();
  char idBuf[16];
  sprintf(idBuf, "MOT-%04X", (uint16_t)(chipid >> 32));
  deviceId = String(idBuf);

  wifiSsid = prefs.getString("wifi_ssid", "");
  wifiPassword = prefs.getString("wifi_pass", "");

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

  if (wifiSsid.length() > 0) {
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
    unsigned long startW = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startW < 8000) {
      delay(300);
    }
    if (WiFi.status() == WL_CONNECTED) {
      mqttClient.setServer(mqttServer, mqttPort);
      mqttClient.setCallback(mqttCallback);
      fetchLocationAndWeather();
      lastWeatherFetch = millis();
    }
  }
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

    if (petMood == 1) { 
      display.fillCircle(lX + 12, centerY, 10, SSD1306_WHITE);
      display.fillCircle(rX + 12, centerY, 10, SSD1306_WHITE);
    } else if (petMood == 2) { 
      display.fillRect(lX, centerY + 4, eW, 4, SSD1306_WHITE);
      display.fillRect(rX, centerY + 4, eW, 4, SSD1306_WHITE);
    } else if (petMood == 3) { 
      display.drawLine(lX, centerY - 6, lX + eW, centerY + 6, SSD1306_WHITE);
      display.drawLine(lX, centerY + 6, lX + eW, centerY - 6, SSD1306_WHITE);
      display.drawLine(rX, centerY - 6, rX + eW, centerY + 6, SSD1306_WHITE);
      display.drawLine(rX, centerY + 6, rX + eW, centerY - 6, SSD1306_WHITE);
    } else { 
      if (animStep == 1) targetEyeHeight = 4; 
      else if (animStep == 2) { targetEyeHeight = 18; lX = 36; rX = 76; } 
      else if (animStep == 3) { targetEyeHeight = 18; lX = 28; rX = 68; } 
      else targetEyeHeight = 18; 

      display.fillRoundRect(lX, centerY - (targetEyeHeight / 2), eW, targetEyeHeight, 4, SSD1306_WHITE);
      display.fillRoundRect(rX, centerY - (targetEyeHeight / 2), eW, targetEyeHeight, 4, SSD1306_WHITE);
    }

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
  display.setTextSize(1);
  display.setCursor(6, 14);
  display.print("ID: " + deviceId);
  display.setCursor(6, 28);
  display.print("VER: " + currentVersion);
  display.setCursor(6, 42);
  display.print(WiFi.status() == WL_CONNECTED ? "CLOUD: CONNECTED" : "CLOUD: OFFLINE");
  display.display();
}

void drawSettingsMenu() {
  display.clearDisplay();
  drawMinimalNav();
  display.setTextSize(1);
  display.setCursor(12, 10);
  display.print(settingSelection == 0 ? "> SND: " : "  SND: ");
  display.print(soundEnabled ? "ON" : "OFF");
  display.setCursor(12, 22);
  display.print(settingSelection == 1 ? "> THM: " : "  THM: ");
  display.print(themeMode == 0 ? "DRK" : "LGT");
  display.setCursor(12, 34);
  display.print(settingSelection == 2 ? "> WIFI: SETUP" : "  WIFI: SETUP");
  display.setCursor(12, 46);
  display.print(settingSelection == 3 ? "> OTA: CHECK" : "  OTA: CHECK");
  display.setCursor(12, 58);
  display.print(settingSelection == 4 ? "> [HOME]" : "  [HOME]");
  display.display();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnectMqtt();
    }
    mqttClient.loop();
  }

  if (wifiSsid.length() > 0 && (millis() - lastWeatherFetch > weatherInterval)) {
    fetchLocationAndWeather();
    lastWeatherFetch = millis();
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
        settingSelection = (settingSelection + 1) % 5; 
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
          startWifiConfigPortal(); 
        } else if (settingSelection == 3) {
          checkForUpdateAndOTA(); 
        } else if (settingSelection == 4) {
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
