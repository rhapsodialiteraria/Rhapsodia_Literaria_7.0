//=======================LIBRARY IMPORT SECTION===================
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FastLED.h>
#include <TimeLib.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

// ====================== CONFIG ======================
String current_version = "1.0.0";
String new_version = "0.0";
String currentConfigVersion = "0.0.0";

// Config URLs (remote)
const char* CONFIG_VERSION_URL = "https://raw.githubusercontent.com/milma-can-lab/milma_ota_config/refs/heads/main/config_version.txt";
const char* CONFIG_FILE_URL = "https://raw.githubusercontent.com/milma-can-lab/milma_ota_config/refs/heads/main/config.json";

const char* VERSION_URL = "https://raw.githubusercontent.com/milma-can-lab/milma_ota_config/refs/heads/main/ota_version.txt";
const char* FW_BASE_URL = "https://raw.githubusercontent.com/milma-can-lab/milma_ota_config/main/firmware_";

// Temperature sensor pins
#define AMBIENT_PIN 23
#define NUM_TEMP_BUSES 3
const int TEMP_PINS[NUM_TEMP_BUSES] = { 26, 25, 27 };
#define EXPECTED_SENSOR_COUNT 30

// ADC pins
#define VBAT_PIN 34
#define POWER_SENSE_PIN 35
bool powerConnected = false;
unsigned long lastPowerCheck = 0;

// ================== CONFIG STRUCT ==================
struct Config {
  uint32_t broadcastInterval;
  uint32_t retryTime;
  uint32_t sensorCheckTime;
  uint32_t intervalAfterAlert;
  int maxBroadcastRetries;
  float tempThresholdHigh;
  float tempThresholdMedium;
  float tempHysteresis;
  int socThreshold;
  int resetHour;
  int resetMinute;
  bool alertBroadcastEnabled;
  bool autoUpdateEnabled;
};

Config cfg;

// ===================== Global Variables =====================
int lastModemError = 0;      // Last modem error (after a failed command)
int lastBroadcastError = 0;  // Previous broadcast/network error (before new send)

// Divider constants (ohms, used for calculations)
const float R_VBAT_TOP = 680000.0f;
const float R_VBAT_BOT = 470000.0f;
const float R_EXT_TOP = 100000.0f;
const float R_EXT_BOT = 33000.0f;

// ADC / reference
const float ADC_MAX = 4095.0f;
const float VREF = 3.3f;

// LEDs
#define LED_PIN 21
#define NUM_LEDS 3
CRGB leds[NUM_LEDS];

// Modem
#define EC200U_RX 16
#define EC200U_TX 17
HardwareSerial MODEM(1);

uint32_t t_lastSensorRead = 0;
uint32_t t_lastSuccessBroadcast = 0;
uint32_t t_lastFailBroadcast = 0;
uint32_t t_lastAlertAttempt = 0;
unsigned long timestamp = 0;

// ====================== GLOBAL STATE ======================
struct TempData {
  String mac;
  float value;
  int tflag;
};
#define MAX_SENSORS 30
TempData freezerSensors[MAX_SENSORS];
float ambientTemp = 0;
int ambientFlag = 0;
int Alertflag = 0;
bool enableFlag = false;
int tflag = 0;
bool ttFlag = false;
int pflag = 0;
int nflag = 0;
bool vFlag = true;
int broadcastRetries = 0;
bool broadcastInProgress = false;
bool lastBroadcastWasAlert = false;
int totalSensorsConnected = 0;
bool isNetworkBlinking = false;
bool inBroadcast = false;  

OneWire* oneWireBuses[NUM_TEMP_BUSES];
DallasTemperature* tempBuses[NUM_TEMP_BUSES];

// ====================== LED BLINK STATE ======================
struct BlinkState {
  bool active = false;
  CRGB c1 = CRGB::Black;
  CRGB c2 = CRGB::Black;
  uint32_t interval = 500;
  uint32_t lastToggle = 0;
  bool state = false;
  uint32_t until = 0;
};
BlinkState ledBlink[NUM_LEDS];

// ====================== HELPERS ======================
void showResponseDetailed(String command) {
  String resp = "";
  unsigned long start = millis();
  while (millis() - start < 2000) {
    while (MODEM.available()) resp += (char)MODEM.read();
    if (resp.indexOf("OK") != -1 || resp.indexOf("ERROR") != -1) break;
  }
  if (resp.length() > 0) {
    Serial.printf("[MODEM] %s Response:\n%s\n", command.c_str(), resp.c_str());
  }
}

// ====================== Reads the saved device ID ======================
String getDeviceId() {
  Preferences prefs;
  prefs.begin("device_data", true);
  String device_id = prefs.getString("device_id");
  prefs.end();
  return device_id;
}

String getAPIKeyFromNVS() {
  Preferences prefs;
  prefs.begin("device_data", true);  // open NVS in read-only mode
  String apiKey = prefs.getString("api_key");
  prefs.end();
  return apiKey;
}

// ====================== Ambient pin (23) =========================
float readAmbientTemperature(uint8_t pin) {
  OneWire oneWire(AMBIENT_PIN);
  DallasTemperature tempSensor(&oneWire);
  tempSensor.begin();

  float t = NAN;
  for (int retry = 0; retry < 4; retry++) {
    tempSensor.requestTemperatures();
    delay(200);
    t = tempSensor.getTempCByIndex(0);
    if (t != 85.0 && t != -127.0 && t != DEVICE_DISCONNECTED_C && !isnan(t))
      break;
  }

  if (isnan(t) || t == -127.0 || t == 85.0) {
    Serial.printf("[WARN] Ambient sensor (Pin %d) failed after retries.\n", pin);
    return NAN;
  }

  Serial.printf("[DEBUG] Ambient sensor (Pin %d): %.2f°C\n", pin, t);
  return t;
}

// ====================== Address to a readable HEX string =========================
String temphexstring(DeviceAddress addr) {
  char buf[17];
  buf[0] = 0;
  for (uint8_t i = 0; i < 8; i++) {
    char tmp[3];
    sprintf(tmp, "%02X", addr[i]);
    strcat(buf, tmp);
  }
  return String(buf);
}

// ====================== TEMPERATURE READING ======================
float tempread(DallasTemperature& sensors, DeviceAddress device) {
  sensors.requestTemperaturesByAddress(device);
  float temp = sensors.getTempC(device);
  const float DISCONNECTED = DEVICE_DISCONNECTED_C;

  if (temp == DISCONNECTED) {
    for (int i = 0; i < 4; i++) {
      delay(50);
      sensors.requestTemperaturesByAddress(device);
      temp = sensors.getTempC(device);
      if (temp != DISCONNECTED) break;
    }
    if (temp == DISCONNECTED) {
      Serial.println("[DEBUG] tempread: DEVICE_DISCONNECTED after retries.");
      return NAN;
    }
  }
  return temp;
}

// ====================== INDIVIDUAL SENSOR FLAG CHECK ======================
int checkThreshold(float temp, int oldflag) {
  int newflag = oldflag;

  // Hysteresis logic
  if (oldflag == 0 && temp > cfg.tempThresholdMedium) newflag = 1;
  else if (oldflag == 1) {
    if (temp >= cfg.tempThresholdHigh) newflag = 2;
    else if (temp < (cfg.tempThresholdMedium - cfg.tempHysteresis)) newflag = 0;
  } else if (oldflag == 2 && temp < (cfg.tempThresholdHigh - cfg.tempHysteresis)) newflag = 1;

  if (newflag != oldflag) {
    Serial.printf("[DEBUG] Threshold changed %d -> %d (%.1f°C)\n", oldflag, newflag, temp);
  }

  return newflag;
}

// ====================== MAIN TEMPERATURE READ FUNCTION ======================
void readAllTemperatures() {
  ambientTemp = readAmbientTemperature(AMBIENT_PIN);
  totalSensorsConnected = 0;
  int index = 0;
  int oldTFlag = tflag;
  // Reset all previous data
  for (int i = 0; i < MAX_SENSORS; i++) {
    freezerSensors[i].mac = "";
    freezerSensors[i].value = NAN;
  }

  Serial.println("\n[INFO] === Reading all freezer sensors ===");

  // --- Freezer buses (pins 26, 25, 27)
  for (int b = 0; b < NUM_TEMP_BUSES; b++) {
    tempBuses[b]->begin();
    delay(50);

    int devCount = tempBuses[b]->getDeviceCount();
    if (devCount == 0) {
      ttFlag = true;
      Serial.printf("[WARN] No sensors detected on bus %d (pin %d)\n", b + 1, TEMP_PINS[b]);
      continue;
    } else ttFlag = false;

    DeviceAddress dev;
    for (int i = 0; i < devCount; i++) {
      if (!tempBuses[b]->getAddress(dev, i)) continue;
      String mac = temphexstring(dev);

      float t = NAN;
      for (int retry = 0; retry < 4; retry++) {
        tempBuses[b]->requestTemperaturesByAddress(dev);
        delay(150);
        t = tempBuses[b]->getTempC(dev);
        if (t != 85.0 && t != 127.0 && t != DEVICE_DISCONNECTED_C && !isnan(t))
          break;
      }

      // Store valid reading
      freezerSensors[index].mac = mac;
      freezerSensors[index].value = t;
      freezerSensors[index].tflag = checkThreshold(t, freezerSensors[index].tflag);
      index++;
      totalSensorsConnected++;

      Serial.printf("[BUS %d | %s] Temp: %.2f°C\n", b + 1, mac.c_str(), t);
      Serial.printf("[BUS %d | %s] Temp: %.2f°C | Flag: %d\n",
                    b + 1, mac.c_str(), t, freezerSensors[index - 1].tflag);
    }
  }
  int maxFlag = 0;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (freezerSensors[i].mac == "") continue;
    if (freezerSensors[i].tflag > maxFlag)
      maxFlag = freezerSensors[i].tflag;
  }
  if (oldTFlag != maxFlag) {
    Alertflag++;
  }
  tflag = maxFlag;
  Serial.printf("[INFO] Ambient: %.2f°C | Freezers detected: %d\n",
                ambientTemp, totalSensorsConnected);
  Serial.println("------------------------------------------------------");
}

// ====================== ADC & BATTERY / POWER LOGIC ======================
float readPinVoltage(int pin) {
  uint32_t rawSum = 0;
  const int samples = 5;
  for (int i = 0; i < samples; ++i) {
    rawSum += analogRead(pin);
    delay(5);
  }
  float raw = (float)rawSum / samples;
  float vadc = (raw / ADC_MAX) * VREF;  // measured voltage at the ADC pin
  return vadc;
}
// Convert VBAT divider reading to actual battery voltage
float readVBATVoltage() {
  float vadc = readPinVoltage(VBAT_PIN);
  float factor = (R_VBAT_TOP + R_VBAT_BOT) / R_VBAT_BOT;  // ~2.4468
  float vbat = vadc * factor;
  return vbat;
}
float readExternalVoltage() {
  float vadc = readPinVoltage(POWER_SENSE_PIN);
  float factor = (R_EXT_TOP + R_EXT_BOT) / R_EXT_BOT;  // ~4.0303
  float vin = vadc * factor;
  return vin;
}
int voltageToPercent(float vbat, float minV = 3.0f, float maxV = 4.2f) {
  if (vbat <= minV) return 0;
  if (vbat >= maxV) return 100;
  float pct = (vbat - minV) / (maxV - minV) * 100.0f;
  return (int)round(pct);
}
int getBatteryPercentPreferModem() {
  MODEM.println("AT+QADC=0");
  unsigned long startTime = millis();
  String response = "";

  while (millis() - startTime < 2000) {
    while (MODEM.available()) {
      char c = MODEM.read();
      response += c;
    }
  }
  Serial.println("[DEBUG] Full MODEM response:");
  Serial.println(response);
  int index = response.indexOf("+QADC:");
  if (index == -1) {
    Serial.println("[ERROR] QADC response not found!");
    return -1;
  }
  int commaIndex = response.indexOf(',', index);
  if (commaIndex == -1) return -1;
  String valueStr = response.substring(commaIndex + 1);
  valueStr.trim();

  int adc_mV = valueStr.toInt();                  // in mV
  float v_adc = adc_mV / 1000.0;                  // convert to volts
  float dividerFactor = 2.4468085;                // (R1+R2)/R2 = 1160/480 ≈ 2.4167
  float v_batt = (v_adc * dividerFactor) + 0.04;  // actual battery voltage

  // Debug prints
  Serial.printf("[DEBUG] Parsed ADC0 Value: %d mV\n", adc_mV);
  Serial.printf("[DEBUG] ADC0 (input) Voltage: %.3f V\n", v_adc);
  Serial.printf("[DEBUG] Estimated Battery Voltage: %.3f V\n", v_batt);

  // ============ Voltage → SOC Mapping ===============
  int soc = 0;
  if (v_batt >= 4.00) soc = 100;      // 4.2–4.0 V → 100%
  else if (v_batt >= 3.85) soc = 80;  // 4.0–3.85 V → 80%
  else if (v_batt >= 3.75) soc = 60;  // 3.85–3.75 V → 60%
  else if (v_batt >= 3.65) soc = 40;  // 3.75–3.65 V → 40%
  else if (v_batt >= 3.50) soc = 20;  // 3.65–3.50 V → 20%
  else soc = 0;
  if (v_batt < 3.5) {
    vFlag = false;
  } else vFlag = true;

  Serial.printf("[DEBUG] Estimated Battery SOC: %d%%\n", soc);
  return soc;
}

// ====================== POWER STATUS ======================
void powerRead() {
  float extVin = readExternalVoltage();  // real voltage on external input (e.g., 12V)
  float vbat = readVBATVoltage();        // battery pack voltage measured via divider
  int batteryPercent = getBatteryPercentPreferModem();

  Serial.printf("[DEBUG] ADC extVin=%.3fV, VBAT=%.3fV, ModemBatt=%d%%\n", extVin, vbat, batteryPercent);

  int oldPflag = pflag;

  // Determine external power presence: threshold choose e.g., > 6.0V means 12V present
  if (extVin > 0.2f) {
    pflag = 0;  // external power present
    Serial.println("[DEBUG] External power DETECTED -> pflag=0 (GREEN)");
  } else {
    // Running on battery or no external supply
    if (batteryPercent >= 0 && batteryPercent < 20) {
      pflag = 2;  // battery critical
      Serial.println("[DEBUG] Battery CRITICAL (<20%) -> pflag=2 (RED BLINK)");
    } else {
      pflag = 1;  // battery OK or unknown
      Serial.println("[DEBUG] Battery OK or unknown -> pflag=1 (ORANGE)");
    }
  }

  if (pflag != oldPflag) {
    Alertflag++;
    Serial.printf("[DEBUG] Power status changed: %d -> %d\n", oldPflag, pflag);
  }
}

// ====================== NETWORK ======================
void networkCheck() {
  Serial.println("[MODEM] Sending: AT+CSQ");
  MODEM.println("AT+CSQ");
  delay(300);

  String resp = "";
  while (MODEM.available()) resp += (char)MODEM.read();

  Serial.println("[MODEM] AT+CSQ Response:");
  Serial.println(resp);

  int rssi = -1;
  int start = resp.indexOf("+CSQ:");
  if (start != -1) {
    String val = resp.substring(start + 6);
    val.trim();
    int comma = val.indexOf(",");
    if (comma != -1) {
      rssi = val.substring(0, comma).toInt();
    }
  }

  if (rssi == -1 || rssi == 99) nflag = 2;
  else if (rssi < 10) nflag = 2;
  else if (rssi < 15) nflag = 1;
  else nflag = 0;

  Serial.printf("[DEBUG] RSSI=%d → nflag=%d\n", rssi, nflag);
}

// ====================== TIME ======================
void updateTimestamp() {
  Serial.println("[MODEM] Sending: AT+CCLK?");
  MODEM.println("AT+CCLK?");
  delay(500);

  String resp = "";
  unsigned long start = millis();
  while (millis() - start < 1000) {
    while (MODEM.available()) resp += (char)MODEM.read();
  }

  Serial.println("[MODEM] AT+CCLK? Response:");
  Serial.println(resp);

  int idx1 = resp.indexOf("\"");
  int idx2 = resp.indexOf("\"", idx1 + 1);

  if (idx1 != -1 && idx2 != -1) {
    String dt = resp.substring(idx1 + 1, idx2);
    int yy = dt.substring(0, 2).toInt();
    int year = 2000 + yy;
    int month = dt.substring(3, 5).toInt();
    int day = dt.substring(6, 8).toInt();
    int hh = dt.substring(9, 11).toInt();
    int mm = dt.substring(12, 14).toInt();
    int ss = dt.substring(15, 17).toInt();

    tmElements_t tm;
    tm.Year = year - 1970;
    tm.Month = month;
    tm.Day = day;
    tm.Hour = hh;
    tm.Minute = mm;
    tm.Second = ss;

    timestamp = makeTime(tm);
    Serial.printf("[TIME] %04d-%02d-%02d %02d:%02d:%02d (Unix: %lu)\n",
                  year, month, day, hh, mm, ss, timestamp);
  } else {
    Serial.println("[TIME] Failed to parse time");
    timestamp = 0;
  }
}

// ====================== LED CONTROL ======================
void updateAllLEDs() {
  // --- LED 0: Temperature status ---
  if (tflag == 0)
    leds[0] = CRGB::Green;
  else if (tflag == 1)
    leds[0] = CRGB(255, 164, 0);  // Orange
  else if (tflag == 2)
    leds[0] = CRGB::Red;
  if (ttFlag) {
    leds[0] = CRGB::Black;
  }
  // --- LED 1: Network / Broadcast / Alert ---
  if (inBroadcast) {
    // During broadcast/alert → show purple
    leds[1] = CRGB(0, 0, 255);
  } else {
    // Normal network status
    if (nflag == 0)
      leds[1] = CRGB::Green;
    else if (nflag == 1)
      leds[1] = CRGB(255, 164, 0);  // Orange
    else
      leds[1] = CRGB::Red;
  }

  // --- LED 2: Power / Battery ---
  if (pflag == 0) {
    // External power present
    leds[2] = CRGB::Green;
  } else if (pflag == 1) {
    // Battery mode
    leds[2] = CRGB(255, 164, 0);  // Orange
  } else if (pflag == 2) {
    // Battery critical (<20%) → blink red
    static bool blinkState = false;
    static unsigned long lastBlink = 0;
    unsigned long now = millis();
    if (now - lastBlink >= 500) {
      blinkState = !blinkState;
      lastBlink = now;
    }
    leds[2] = blinkState ? CRGB::Red : CRGB::Black;
  }
  FastLED.setBrightness(100);
  FastLED.show();
}

// ================== HELPERS ==================
void showResponse(unsigned long timeout = 5000) {
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (MODEM.available()) {
      Serial.write(MODEM.read());
      start = millis();
    }
  }
  Serial.println();
}
// Clean raw AT/HTTP response to extract JSON
String cleanJson(const String& raw) {
  int startIdx = raw.indexOf('{');
  int endIdx = raw.lastIndexOf('}');
  if (startIdx >= 0 && endIdx > startIdx) {
    return raw.substring(startIdx, endIdx + 1);
  } else {
    Serial.println("[ERROR] Could not locate JSON in response!");
    return "";
  }
}
// HTTP GET via EC200U
String httpGET(const char* url) {
  delay(2000);

  MODEM.println("AT+QHTTPCFG=\"contextid\",1");
  delay(200);
  delay(2000);

  MODEM.print("AT+QHTTPURL=");
  MODEM.print(strlen(url));
  MODEM.println(",60");
  delay(200);
  showResponse();

  MODEM.println(url);
  delay(500);
  showResponse();

  MODEM.println("AT+QHTTPGET=80");
  delay(2000);
  showResponse();

  MODEM.println("AT+QHTTPREAD=120");
  delay(500);

  String response = "";
  unsigned long start = millis();
  while (millis() - start < 5000) {
    while (MODEM.available()) {
      char c = MODEM.read();
      response += c;
      start = millis();
    }
  }
  return response;
}

String parseVersionResponse(const String& response) {
  String clean = "";
  String line = "";
  for (int i = 0; i < response.length(); i++) {
    char c = response[i];
    if (c == '\r' || c == '\n') {
      if (line.indexOf('.') > 0 && isDigit(line[0])) {
        clean = line;
        break;
      }
      line = "";
    } else {
      line += c;
    }
  }
  return clean;
}

String httpOGET(const char *url) {
  MODEM.println("AT+QHTTPCFG=\"contextid\",1");
  delay(200);
 // showResponse();

  MODEM.print("AT+QHTTPURL=");
  MODEM.print(strlen(url));
  MODEM.println(",60");
  delay(200);
  showResponse();

  MODEM.println(url);
  delay(500);
  showResponse();

  MODEM.println("AT+QHTTPGET=80");  // 80s timeout
  delay(2000);
  showResponse();

  MODEM.println("AT+QHTTPREAD=120");
  delay(500);

  String response = "";
  unsigned long start = millis();
  while (millis() - start < 5000) {
    while (MODEM.available()) {
      char c = MODEM.read();
      response += c;
      start = millis();
    }
  }

  Serial.println("[HTTP Raw Response]");
  Serial.println(response);

  String parsed = parseVersionResponse(response);
  Serial.println("[Parsed Version]");
  Serial.println(parsed);

  return parsed;
}

// ================== LOAD CONFIG ==================
bool loadConfig() {
  Serial.println("\n========== CONFIG SYSTEM START ==========");

  if (!SPIFFS.begin(true)) {
    Serial.println("[ERROR] SPIFFS mount failed!");
    return false;
  }

  // Read local config version
  currentConfigVersion = "0.0";
  if (SPIFFS.exists("/config_version.txt")) {
    File vf = SPIFFS.open("/config_version.txt", "r");
    if (vf) {
      currentConfigVersion = vf.readString();
      vf.close();
      currentConfigVersion.trim();
    }
  }
  Serial.println("[CFG] Local config version: " + currentConfigVersion);

  // Fetch remote version via EC200U
  String response = httpGET(CONFIG_VERSION_URL);
  response.trim();
  String remoteVersion = response;
  Serial.println("[CFG] Remote config version: " + remoteVersion);

  // Compare versions and update if needed
  if (!remoteVersion.isEmpty() && remoteVersion != currentConfigVersion) {
    Serial.println("[CFG] New config version found. Updating...");

    // Fetch new config.json
    String configResponse = httpGET(CONFIG_FILE_URL);
    configResponse.trim();
    configResponse = cleanJson(configResponse);
    if (configResponse.isEmpty()) {
      Serial.println("[ERROR] Failed to extract valid JSON!");
      return false;
    }

    // Save new config.json
    File f = SPIFFS.open("/config.json", "w");
    if (f) {
      f.print(configResponse);
      f.close();
      Serial.println("[CFG] New config.json saved to SPIFFS.");
    }

    // Save new version file
    File vf = SPIFFS.open("/config_version.txt", "w");
    if (vf) {
      vf.print(remoteVersion);
      vf.close();
    }

    currentConfigVersion = remoteVersion;
  } else {
    Serial.println("[CFG] Config is already up-to-date.");
  }
  // Load current config.json into structure
  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    Serial.println("[ERROR] Failed to open /config.json");
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);

  cfg.broadcastInterval = doc["broadcast_interval"] | 600000;
  cfg.retryTime = doc["retry_time"] | 120000;
  cfg.sensorCheckTime = doc["sensor_check_time"] | 20000;
  cfg.maxBroadcastRetries = doc["max_broadcast_retries"] | 4;
  cfg.intervalAfterAlert = doc["interval_after_alert"] | 60000;
  cfg.tempThresholdHigh = doc["temp_threshold_high"] | 15.0;
  cfg.tempThresholdMedium = doc["temp_threshold_medium"] | 10.0;
  cfg.tempHysteresis = doc["temp_hysteresis"] | 2.0;
  cfg.socThreshold = doc["soc_threshold"] | 20;
  cfg.resetHour = doc["reset_hour"] | 0;
  cfg.resetMinute = doc["reset_minute"] | 0;
  cfg.alertBroadcastEnabled = doc["alert_broadcast_enabled"] | true;
  cfg.autoUpdateEnabled = doc["autoUpdateEnabled"] | false;
  if (error) {

    Serial.printf("[ERROR] JSON parse failed: %s\n", error.c_str());
    file.close();
    return false;
  }

  file.close();
  Serial.println("[CFG] Config structure loaded successfully.");

  Serial.println("========== CONFIG SYSTEM READY ==========\n");

  return true;
}

//===========================OTA Code================================
bool checkForUpdate() {
  Serial.println("Checking for update...");
  //String server_version = "http://" + String(SERVER_IP) + String(VERSION_PATH);
  String server_version = String(VERSION_URL);
  String response = httpOGET(server_version.c_str());

  if (response.length() == 0) {
    Serial.println("No valid version response");
    return false;
  }

  new_version = response;
  Serial.println("Current version: " + current_version);
  Serial.println("Available version: " + new_version);

  if (new_version != current_version) {
    Serial.println("New firmware available!");
    return true;
  }
  Serial.println("Already latest firmware.");
  return false;
}

// ================== DOWNLOAD TO SPIFFS ==================
bool downloadFirmwareToSPIFFS(bool clearBefore = true) { 
            if (clearBefore) {
              //Serial.println("[INFO] Clearing SPIFFS...");
              if (!SPIFFS.format()) {
                Serial.println("[ERROR] Failed to format SPIFFS!");
                return false;
              }
              Serial.println("[INFO] SPIFFS cleared successfully.");
             delay(2000); 
            }

  String fw_url_path = String(FW_BASE_URL) + new_version + ".bin";

  String fw_url = fw_url_path;
  
  Serial.println("Downloading firmware to SPIFFS: " + fw_url);

  String fw_filename = "/firmware_" + new_version + ".bin";
  File fwFile = SPIFFS.open(fw_filename, FILE_WRITE);
  if (!fwFile) {
    Serial.println("[ERROR] Could not open file on SPIFFS!");
    return false;
  }

  // Tell MODEM the URL
  MODEM.print("AT+QHTTPURL=");
  MODEM.print(fw_url.length());
  MODEM.println(",60");
  delay(200);
  showResponse();

  MODEM.println(fw_url);
  delay(500);
  showResponse();

  // Start GET
  MODEM.println("AT+QHTTPGET=120");
  delay(3000);

  // Capture GET response for size
  String getResp = "";
  unsigned long t0 = millis();
  while (millis() - t0 < 5000) {
    while (MODEM.available()) {
      char c = MODEM.read();
      getResp += c;
      t0 = millis();
    }
  }
  Serial.println(getResp);

  int fw_size = 0;
  int idx = getResp.indexOf("+QHTTPGET:");
  if (idx >= 0) {
    int c1 = getResp.indexOf(',', idx + 10);
    int c2 = getResp.indexOf(',', c1 + 1);
    if (c2 > 0) {
      fw_size = getResp.substring(c2 + 1).toInt();
    }
  }
  Serial.printf("Expected firmware size: %d bytes\n", fw_size);
  if (fw_size <= 0) {
    Serial.println("[ERROR] Could not parse firmware size!");
    fwFile.close();
    return false;
  }

  // Start streaming firmware
  uint8_t buf[256];
  size_t total_received = 0;
  unsigned long t_start = millis();
  bool data_mode = false;
MODEM
  .println("AT+QHTTPREAD=120");
  delay(10);

  // --- Wait for exact CONNECT sequence ---
  String headerBuffer = "";
  while (!data_mode && millis() - t_start < 10000) {
    while (MODEM.available()) {
      char c = MODEM.read();
      headerBuffer += c;

      if (headerBuffer.indexOf("CONNECT\r\n") >= 0) {
        Serial.println("[MODEM] CONNECT found, start binary download...");
        data_mode = true;
        break;
      }
    }
  }

  // --- Now read pure binary ---
  while (total_received < fw_size && millis() - t_start < 12000) {
    while (MODEM.available()) {
      int remaining = fw_size - total_received;
      int toRead = min((int)sizeof(buf), remaining);
      int len = MODEM.readBytes((char*)buf, toRead);

      if (len > 0) {
        fwFile.write(buf, len);
        total_received += len;

        int lastPercent=0;
        int percent = (total_received * 100) / fw_size;
        Serial.printf("Downloading... %d%% (%d/%d)\n", percent, total_received, fw_size); 
        if (percent % 5 == 0 && percent != lastPercent) {
          Serial.printf("Downloading... %d%%\n", percent);
          lastPercent = percent;
        } 
      }

      t_start = millis();

      if (total_received >= fw_size) {
        break;
      }
    }
  }

  fwFile.close();
  Serial.printf("Download complete: %d bytes written\n", total_received);

  if (total_received != fw_size) {
    Serial.printf("[ERROR] Download incomplete! Got %d of %d bytes\n", total_received, fw_size);
    return false;
  }

  Serial.println("Firmware saved to SPIFFS successfully!");
  Serial.printf("[READY] Firmware file prepared: /firmware_%s.bin\n", new_version.c_str());

  return true;
}

// ================== FLASH FROM SPIFFS ==================
bool flashFirmwareFromSPIFFS(const char* path) {
  File fwFile = SPIFFS.open(path);
  if (!fwFile) {
    Serial.println("[ERROR] Cannot open firmware file on SPIFFS!");
    return false;
  }

  size_t fwSize = fwFile.size();
  Serial.printf("Flashing firmware from SPIFFS: %s (%d bytes)\n", path, fwSize);

  if (!Update.begin(fwSize)) { 
    Serial.println("[ERROR] Not enough space for OTA!");
    fwFile.close();
    return false;
  }

  uint8_t buf[256];
  size_t written = 0;

  while (fwFile.available()) {
    int len = fwFile.read(buf, sizeof(buf));
    if (len > 0) {
      Update.write(buf, len);
      written += len;

      int percent = (written * 100) / fwSize;
      Serial.printf("Flashing... %d%% (%d/%d)\n", percent, written, fwSize);
    } else {
      delay(1);
    }
  }

  fwFile.close();

  if (Update.end() && Update.isFinished()) {
    Serial.println("OTA update complete! Restarting...");
    delay(1000);
    ESP.restart();
    return true;
  } else {
    Serial.printf("[ERROR] OTA failed! Error code: %d\n", Update.getError());
    return false;
  }
}


// ================== PERFORM OTA FUNCTION ==================
void performOTAUpdate() {
  Serial.println("=== OTA Update Process Started ===");

  // ---------- Step 1: Mount SPIFFS safely ----------
  if (!SPIFFS.begin(true)) {
    Serial.println("[ERROR] SPIFFS Mount Failed! Trying to format...");
    if (!SPIFFS.format()) {
      Serial.println("[FATAL] SPIFFS format failed. Aborting OTA.");
      return;
    }
    if (!SPIFFS.begin(true)) {
      Serial.println("[FATAL] SPIFFS still not mountable after format. Aborting.");
      return;
    }
  }
  Serial.println("[OK] SPIFFS mounted successfully.");

  // ---------- Step 2: Check for update ----------
  if (checkForUpdate()) {
    Serial.println("[INFO] Update available, starting download...");

    // ---------- Step 3: Download firmware ----------
    if (downloadFirmwareToSPIFFS()) {
      String fw_filename = "/firmware_" + new_version + ".bin";
      Serial.printf("[INFO] Firmware file prepared: %s\n", fw_filename.c_str());

      // ---------- Step 4: Flash firmware ----------
      if (flashFirmwareFromSPIFFS(fw_filename.c_str())) {
        Serial.println("[SUCCESS] OTA update complete.");
      } else {
        Serial.println("[ERROR] Flashing firmware failed!");
      }
    } else {
      Serial.println("[ERROR] Firmware download failed!");
    }

  } else {
    Serial.println("[INFO] No update required.");
  }

  Serial.println("=== OTA Update Process Completed ===");
}

// ====================== HTTPS FUNCTIONS) ======================
String getModemResponse(unsigned long timeout = 3000) {
  String resp = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (MODEM.available()) {
      resp += (char)MODEM.read();
    }
  }
  return resp;
}

// ===================== Helper: Send AT Command =====================
String sendATCommand(String cmd, unsigned long timeout = 2000) {
  Serial.println("AT> " + cmd);
  MODEM.println(cmd);
  String resp = getModemResponse(timeout);
  if (resp.length()) {
    Serial.println("AT< " + resp);
  } else {
    Serial.println("AT< (no response)");
  }
  return resp;
}

// ===================== Helper: Get Current Modem Error Code =====================
int getLastModemError() {
  MODEM.println("AT+QIGETERROR");
  delay(300);
  String resp = getModemResponse(1000);

  int idx = resp.indexOf(":");
  if (idx != -1) {
    String err = resp.substring(idx + 1);
    err.trim();
    err.replace("OK", "");
    err.trim();
    lastModemError = err.toInt();
    Serial.println("Last Modem Error Code: " + String(lastModemError));
    return lastModemError;
  }

  Serial.println("Could not parse modem error!");
  lastModemError = -1;
  return -1;
}

// ===================== Helper: Get Previous Broadcast Error =====================
int getPreviousBroadcastError() {
  MODEM.println("AT+QIGETERROR");
  delay(300);
  String resp = getModemResponse(1000);

  int idx = resp.indexOf(":");
  if (idx != -1) {
    String err = resp.substring(idx + 1);
    err.trim();
    err.replace("OK", "");
    err.trim();
    lastBroadcastError = err.toInt();
    Serial.println("Previous Broadcast Error: " + String(lastBroadcastError));
    return lastBroadcastError;
  }

  Serial.println("Could not get broadcast error!");
  lastBroadcastError = -1;
  return -1;
}

// ===================== HTTPS POST Function =====================
bool httpPostAT(String payload) {
  bool success = false;
  inBroadcast = true;
  updateAllLEDs();
  String resp;
  String API_KEY = getAPIKeyFromNVS();


  // 1. Configure SSL
  sendATCommand("AT+QSSLCFG=\"sslversion\",1,3");  // TLS 1.2
  sendATCommand("AT+QSSLCFG=\"ciphersuite\",1,0xFFFF");
  sendATCommand("AT+QSSLCFG=\"seclevel\",1,0");  // No certificate check

  // 2. Open SSL connection
  resp = sendATCommand("AT+QSSLOPEN=1,0,1,\"iotdashboard.cdipd.in\",443,0", 10000);
  delay(2000);
  resp += getModemResponse(3000);
  Serial.println("SSL Connection Response: " + resp);

  if (resp.indexOf("OK") == -1 && resp.indexOf("CONNECT") == -1) {
    Serial.println("SSL connection failed");
    getLastModemError();
    inBroadcast = false;
    updateAllLEDs();

    return false;
  }

  // 3. Build HTTP POST request
  String httpRequest = "POST /api/v1/telemetry HTTP/1.1\r\n";
  httpRequest += "Host: iotdashboard.cdipd.in\r\n";
  httpRequest += "Content-Type: application/json\r\n";
  httpRequest += "x-api-key: " + String(API_KEY) + "\r\n";
  httpRequest += "Content-Length: " + String(payload.length()) + "\r\n";
  httpRequest += "Connection: close\r\n\r\n";
  httpRequest += payload;

  Serial.println("\n===== HTTP REQUEST START =====");
  Serial.println(httpRequest);
  Serial.println("===== HTTP REQUEST END =====\n");

  // 4. Tell modem to prepare for sending
  String sendCmd = "AT+QSSLSEND=1," + String(httpRequest.length());
  resp = sendATCommand(sendCmd, 3000);
  if (resp.indexOf(">") == -1) {
    Serial.println("Failed to enter send mode, closing SSL...");
    getLastModemError();
    sendATCommand("AT+QSSLCLOSE=1");
    inBroadcast = false;
    updateAllLEDs();

    return false;
  }

  // 5. Transmit HTTP request
  MODEM.print(httpRequest);
  delay(2000);
  resp = getModemResponse(8000);
  Serial.println("Send response:\n" + resp);
    getLastModemError();
  

  // 6. Read server response
  Serial.println("\nReading server response...");
  resp = sendATCommand("AT+QSSLRECV=1,1500", 8000);
  Serial.println("\n===== SERVER RESPONSE =====");
  Serial.println(resp);
  Serial.println("===========================\n");

  // 7. Check success
  if (resp.indexOf("200 OK") != -1 || resp.indexOf("\"success\"") != -1) {
    success = true;
  }

  // 8. Close SSL connection
  sendATCommand("AT+QSSLCLOSE=1");

  if (success) {
    Serial.println("POST successful!");
  } else {
    Serial.println("POST failed or no valid response");
    getLastModemError();
  }
  inBroadcast = false;
  updateAllLEDs();

  return success;
}

// ================== PAYLOAD JSON ==================
String buildPayload(char S) {
  updateTimestamp();
  String device_Id = getDeviceId();
  int batteryPercent = getBatteryPercentPreferModem();
  int B = (batteryPercent >= 0) ? map(batteryPercent, 0, 100, 0, 5) : 0;
  if (lastBroadcastError == 0) {
    lastBroadcastError = 200;
  }
  if (enableFlag){
    lastBroadcastError = 201;
  }

  String j = "{";
  j += "\"K\":\"F\",";
  j += "\"ID\":\"" + device_Id + "\",";
  j += "\"V\":\"F5.12\",";
  j += "\"S\":\"" + String((S == 'A') ? "A" : "B") + "\",";
  j += "\"TS\":" + String(timestamp) + ",";
  if (isnan(ambientTemp)) j += "\"AT\":0.0,";
  else j += "\"AT\":" + String(ambientTemp, 1) + ",";
  j += "\"T\":[";
  bool first = true;
  for (int i = 0; i < MAX_SENSORS; i++) {
    String mac = freezerSensors[i].mac;
    float val = freezerSensors[i].value;
    if (mac == "") continue;
    if (!first) j += ",";
    first = false;
    if (isnan(val)) j += "{\"" + mac + "\":null}";
    else j += "{\"" + mac + "\":" + String(val, 1) + "}";
  }
  j += "],";
  // Add remaining fields expected by server
  j += "\"P\":" + String((pflag == 0) ? "true" : "false") + ",";
  j += "\"E\":" + String(lastBroadcastError) + ",";
  j += "\"B\":" + String(B);
  j += "}";
  Serial.println("[DEBUG] Payload: " + j);
  return j;
}

// ====================== BROADCAST ======================
bool broadcast(char S) {
  broadcastInProgress = true;
  getPreviousBroadcastError();  
  networkCheck();               

  String msg = buildPayload(S);
  Serial.printf("[DEBUG] %s broadcast starting\n", (S == 'A') ? "ALERT" : "NORMAL");

  bool ok = httpPostAT(msg);
  if (ok) {
    Serial.println("[DEBUG] Broadcast SUCCESS");
    broadcastInProgress = false;
    t_lastSuccessBroadcast = millis();
    broadcastRetries = 0;
    Alertflag = 0;

    if (S == 'A') {
      lastBroadcastWasAlert = true;
      Serial.println("[DEBUG] Next broadcast in 2 minute");
    } else {
      lastBroadcastWasAlert = false;
    }
    return true;
  } else {
    Serial.println("[DEBUG] Broadcast FAILED");
    broadcastRetries++;
    t_lastFailBroadcast = millis();
    return false;
  }
}

// ====================== PERIODIC CHECKS ======================
void checkPeriodicBroadcast() {
  uint32_t now = millis();

  // === ALERT HANDLING ===
  if (Alertflag > 0) {
    // 1. If alert flag is raised, attempt ALERT broadcast immediately
    Serial.printf("[ALERT] Alert detected! Attempting ALERT broadcast (Attempt %d/4)\n", broadcastRetries + 1);

    bool success = broadcast('A');
    if (success) {
      Serial.println("[ALERT] Alert broadcast successful!");
      lastBroadcastWasAlert = true;
      t_lastSuccessBroadcast = now;
      broadcastRetries = 0;
      Alertflag = 0;
      return;
    } else {
      Serial.printf("[ALERT] ALERT broadcast failed (Attempt %d/4)\n", broadcastRetries);
      if (broadcastRetries < 4) {
        // Retry every cfg.retryTime (e.g., 2 min)
        if (Alertflag == 0 && broadcastRetries > 0 && broadcastRetries < cfg.maxBroadcastRetries) {
  if (now - t_lastFailBroadcast >= cfg.retryTime) {
    Serial.println("[ALERT] Retrying ALERT broadcast...");
    broadcast('A');
  }
}
      } 
      else {
        Serial.println("[ALERT] Max retries reached for ALERT broadcast. Will reset retry counter.");
        broadcastRetries = 0;
        Alertflag = 0;  // optional: clear alert to resume normal ops
      }
      return;
    }
  }

  // === NORMAL PERIODIC BROADCAST HANDLING ===
  if ((now - t_lastSuccessBroadcast) >= cfg.broadcastInterval && broadcastRetries == 0) {
    Serial.println("[BROADCAST] 10-minute periodic broadcast triggered");
    bool success = broadcast('B');
    if (success) {
      Serial.println("[BROADCAST] Normal broadcast successful!");
      t_lastSuccessBroadcast = now;
      broadcastRetries = 0;
      lastBroadcastWasAlert = false;
    } else {
      Serial.println("[BROADCAST] Normal broadcast failed, starting retry timer...");
      t_lastFailBroadcast = now;
      broadcastRetries = 1;  // initialize retry count
    }

    // Optional OTA check after normal broadcast
    if (cfg.autoUpdateEnabled) {
      loadConfig();
    }
    return;
  }

  // === RETRY HANDLING ===
  if (broadcastRetries > 0 && broadcastRetries < 4) {
    if (now - t_lastFailBroadcast >= cfg.retryTime) {
      Serial.printf("[RETRY] Retrying broadcast (Attempt %d/4)\n", broadcastRetries + 1);
      char retryType = lastBroadcastWasAlert ? 'A' : 'B';
      bool success = broadcast(retryType);

      if (success) {
        Serial.println("[RETRY] Retry successful!");
        t_lastSuccessBroadcast = now;
        broadcastRetries = 0;
        Alertflag = 0;
      } else {
        broadcastRetries++;
        t_lastFailBroadcast = now;
        Serial.printf("[RETRY] Retry failed (Attempt %d/4)\n", broadcastRetries);
        if (broadcastRetries >= 4) {
          Serial.println("[RETRY] Max retries reached. Resetting retry counter.");
          broadcastRetries = 0;
          Alertflag = 0;
        }
      }
    }
  }
}

// ====================== INIT ======================
void initialization() {
  Serial.println("\n[DEBUG] ===== INITIALIZATION START =====");
  String deviceId = getDeviceId();
  Serial.printf("[DEBUG] Device ID: %s\n", deviceId.c_str());

  for (int i = 0; i < NUM_TEMP_BUSES; i++) {
    oneWireBuses[i] = new OneWire(TEMP_PINS[i]);
    tempBuses[i] = new DallasTemperature(oneWireBuses[i]);
    tempBuses[i]->begin();
  }

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(100);
  FastLED.clear();
  FastLED.show();

  analogSetPinAttenuation(VBAT_PIN, ADC_11db);
  analogSetPinAttenuation(POWER_SENSE_PIN, ADC_11db);

  Serial.println("[DEBUG] Initializing MODEM modem...");
  MODEM.begin(115200, SERIAL_8N1, EC200U_RX, EC200U_TX);
  delay(500);

  Serial.println("[MODEM] Sending: AT");
  MODEM.println("AT");
  delay(200);
  Serial.println("[DEBUG] ===== INITIALIZATION COMPLETE =====\n");
}

//============== CURRENT CONFIGURATION ==============
void printConfig() {
  Serial.println("\n========== CURRENT CONFIGURATION ==========");
  Serial.printf("Broadcast Interval     : %lu ms\n", cfg.broadcastInterval);
  Serial.printf("Retry Time              : %lu ms\n", cfg.retryTime);
  Serial.printf("Sensor Check Time       : %lu ms\n", cfg.sensorCheckTime);
  Serial.printf("interval_after_alert       : %lu ms\n", cfg.intervalAfterAlert);  //////newly added
  Serial.printf("Max Broadcast Retries   : %d\n", cfg.maxBroadcastRetries);
  Serial.printf("Temp Threshold (High)   : %.2f °C\n", cfg.tempThresholdHigh);
  Serial.printf("Temp Threshold (Medium) : %.2f °C\n", cfg.tempThresholdMedium);
  Serial.printf("Temp Hysteresis         : %.2f °C\n", cfg.tempHysteresis);
  Serial.printf("SOC Threshold           : %d %%\n", cfg.socThreshold);
  Serial.printf("Reset Hour              : %d\n", cfg.resetHour);
  Serial.printf("Reset Minute            : %d\n", cfg.resetMinute);
  Serial.printf("Alert Broadcast Enabled : %s\n", cfg.alertBroadcastEnabled ? "true" : "false");
  Serial.printf("Auto Update Enabled     : %s\n", cfg.autoUpdateEnabled ? "true" : "false");
  Serial.println("===========================================\n");
}

//============== MID NIGHT ==============
unsigned long secondsToReset = 0;
unsigned long resetStartMillis = 0;

unsigned long getSecondsToResetFromModem() {
  String resp = "";
  unsigned long start = millis();
  bool validTime = false;

  // Wait up to 60 seconds for valid CCLK
  while (millis() - start < 60000) {
    Serial.println("[MODEM] Sending: AT+CCLK?");
    MODEM.println("AT+CCLK?");
    delay(500);

    resp = "";
    unsigned long readStart = millis();
    while (millis() - readStart < 1000) {
      while (MODEM.available()) resp += (char)MODEM.read();
    }

    Serial.println("[MODEM] AT+CCLK? Response:");
    Serial.println(resp);

    // Check if response looks valid
    if (resp.indexOf("+CCLK:") != -1 && resp.indexOf("00/01/01") == -1 && resp.indexOf("00:00:00") == -1) {
      validTime = true;
      break;
    }

    Serial.println("[WAIT] Waiting for valid network time...");
    delay(2000);
  }

  if (!validTime) {
    Serial.println("[ERROR] Failed to get valid CCLK within 60s");
    return 0;
  }

  // Parse the valid CCLK response
  int idx1 = resp.indexOf("\"");
  int idx2 = resp.indexOf("\"", idx1 + 1);
  if (idx1 == -1 || idx2 == -1) {
    Serial.println("[TIME] Failed to parse modem time");
    return 0;
  }

  String dt = resp.substring(idx1 + 1, idx2);
  // Example: "25/10/30,03:30:07+00"
  int yy = dt.substring(0, 2).toInt();
  int year = 2000 + yy;
  int month = dt.substring(3, 5).toInt();
  int day = dt.substring(6, 8).toInt();
  int hh = dt.substring(9, 11).toInt();
  int mm = dt.substring(12, 14).toInt();
  int ss = dt.substring(15, 17).toInt();

  // ---- Apply Indian timezone offset (UTC+5:30) ----
  hh += 5;
  mm += 30;

  if (mm >= 60) {  // minute overflow
    mm -= 60;
    hh += 1;
  }
  if (hh >= 24) hh -= 24;  // wrap around next day

  // -------------------------------------------------
  tmElements_t tm;
  tm.Year = year - 1970;
  tm.Month = month;
  tm.Day = day;
  tm.Hour = hh;
  tm.Minute = mm;
  tm.Second = ss;

  time_t timestamp = makeTime(tm);

  // Fixed reset time (09:05 IST)
  const int resetHour = cfg.resetHour;
  const int resetMinute = cfg.resetMinute;

  unsigned long currentSeconds = (hh * 3600UL) + (mm * 60UL) + ss;
  unsigned long resetSeconds = (resetHour * 3600UL) + (resetMinute * 60UL);
  unsigned long secondsRemaining;

  if (currentSeconds < resetSeconds)
    secondsRemaining = resetSeconds - currentSeconds;
  else
    secondsRemaining = (24UL * 3600UL) - (currentSeconds - resetSeconds);

  Serial.printf("[TIME] Current (IST): %02d:%02d:%02d | Reset at: %02d:%02d | Remaining: %lu sec\n",
                hh, mm, ss, resetHour, resetMinute, secondsRemaining);

  return secondsRemaining;
}

// ====================== MAIN SETUP ======================
void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  pinMode(POWER_SENSE_PIN, INPUT);
  delay(1000);

  initialization();
  updateAllLEDs();
  readAllTemperatures();
  powerRead();
  networkCheck();
  secondsToReset = getSecondsToResetFromModem();

  resetStartMillis = millis();
  enableFlag = true;
}

// ====================== MAIN LOOP ======================
void loop() {
  digitalWrite(13, HIGH);

  // --- CONFIG & OTA: check and load ---
  if (enableFlag) {
   
    broadcast('A');
    loadConfig();
    performOTAUpdate();
    printConfig();
    enableFlag = false;
  }
  uint32_t now = millis();
  if (now - t_lastSensorRead >= cfg.sensorCheckTime) {
    t_lastSensorRead = now;

    readAllTemperatures();
    powerRead();
    networkCheck();
    if (vFlag) {
      checkPeriodicBroadcast();
    }
    //delay(200);

    Serial.println("\n========== STATUS ==========");
    Serial.printf("[STATUS] Sensors: %d/%d connected\n", totalSensorsConnected, EXPECTED_SENSOR_COUNT);
    Serial.printf("[STATUS] Ambient: %.1f°C, Flag=%d\n", ambientTemp, ambientFlag);
    Serial.printf("[STATUS] Temperature LED: %d\n", tflag);
    Serial.printf("[STATUS] Network: nflag=%d (0=Good, 1=Medium, 2=Poor)\n", nflag);
    Serial.printf("[STATUS] Power: pflag=%d (0=Ext, 1=Batt OK, 2=Batt Low)\n", pflag);
    Serial.printf("[STATUS] Alertflag=%d\n", Alertflag);
    Serial.println("============================\n");
  }
  unsigned long elapsedMillis = millis() - resetStartMillis;
  unsigned long elapsedSeconds = elapsedMillis / 1000UL;
  if (elapsedSeconds >= secondsToReset) {
   // performOTAUpdate();
   // loadConfig();
    //printConfig();
    ESP.restart();
  }
  updateAllLEDs();
}