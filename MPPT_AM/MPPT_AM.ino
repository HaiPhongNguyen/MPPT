
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_ADS1X15.h>
#include <ESP_Google_Sheet_Client.h>
#include <math.h>

// ============================================================
// 1. GPIO - GIU NGUYEN SO DO MACH
// ============================================================

#define backflow_MOSFET 32
#define buck_IN         15
#define buck_EN          2
#define FAN             13
#define TempSensor      33
#define Load            12

#define buttonLeft      17
#define buttonRight     16
#define buttonBack      18
#define buttonSelect     5

#define I2C_SDA         21
#define I2C_SCL         22

// ============================================================
// 2. LCD1602
// ============================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ============================================================
// 3. WIFI CONFIGURATION
// ============================================================

#define WIFI_SSID     "Nothing"
#define WIFI_PASSWORD "123456789a"

// ============================================================
// 4. GOOGLE SHEETS CONFIGURATION
// ============================================================

#define PROJECT_ID  "mpptdataloggingv2"

#define CLIENT_EMAIL "mpptdatalogging@mpptdataloggingv2.iam.gserviceaccount.com"

const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nMIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDImL1QlLgoUL+G\nNwg33xPZPZRTMiFWHclOqfjaaq+Dz/1EpKFzqiCpw0oqmTx0xL5TXnWxRXkHCMn7\nlEEdg+dZlHYxkQCcDLM5FpDOr1c9kgR04ljV5N69y6tuJLyRXZ2aKK2Rmh46KYAv\n1TnljIxMPbMVGQZ9fMCjnhRfkwJGWIKwyv7T0QLUlV/eJTdtk4eWgnk2CpL9ihEg\nh2+6qN0S+qTrLgYnNSc6S8iGoxnurpzr/MiTIe9MVUGbpyTUqRhqqvJxSkJeq9na\nfyk2LxET4xy06EDycs1TILDChYpkzRIIDY0qt1A4k4wTzT8OXN762CMUsmLhgXQm\nVGEUGnEDAgMBAAECggEARDm+2fNbF0sveOSqxOX4iECrfAhm0jVbTIf+/bqfa4OI\nLpqX1xEb/uFPDmID7+simJReDycD3OKeYBSyeOlt/EfEU2NwA6NGKbqFEecNEwHg\nxihd8pHfOEpXSiTwhbiDa/U943G5qiqHUL2C7ZH5/cxWZmyWioOtuXiAkWkcsvO5\niQqCPVvlGhD6KgDklogsBfOqc5JL3FMqdOqEgKyXuPMu3KAnREFj34/DMPMwjqOs\nK9BPt7NQCcrMDqLloRxAhEHY8oY0moQjV9F1mcaxSePvGqrGZPuby4xZjRVnhWDn\n9qXoDZLdjX+aFMuNR2b991QJ2Dt7dqk3R8JWGvYo8QKBgQDjazejH/2gJWaEFQrD\n71BaMlLg0AqsmPemojz/gSms4hhDcodN5bcw2cz5/vX/TMTm5UeMI2Wp/1C3Ahoh\nc99nD6kKGZ0uEY3H6p3r5srPaO+YFQow6WhlcpJdmt8D+kVwGk1H71mmpfLJ1qkn\nNkunnuPgPmTxXTgsRV5yUj2C9wKBgQDhzpFeOwxYfHEIRF7w0uXd9bofVSdwEUtK\nxVk82aF0cJ2QoXrq1LjJdbNmNx3BGPyJfk6Q2dJe7DCZd8Yoy8iArSTO3VMvWf69\nnWiVxrGy7QWPnsQpz3R41rM/dAO66pr1MyrvTqxMbOvIjQ7A0qqMQJ6vQIvVUcpf\nXaVbC69zVQKBgQCAsQWU+99sJn3ytG2ZHu2bfOrFaTUBIrbRxvE5rP/GY1NTp5XF\nkP6vWDDUL2FJjUnHV2u7c7RuFJq6VSgwUf/TtFr16jS2Axz2Sl//RGT0Je+fYhiY\nQRzge4s0CRW2jIOKwneFA375/OJ9MUobdHAvdIyDMwuwQg4tjzu4+azxVQKBgFhe\nryVhloNKsDG8ECvPXYZwFFcOzFBfdEfe0/2sbjNTn1Zq29/rQCYXmg32o/5d+PcW\nJ0ko9duQM7Tb2wE8P0ucdzs+AsAVSEyMvdFf4UtU10hNqhdyyPJBlATRMmSETACJ\n9dORxnxiZCQokA2GZJhe8btEV3U0fS61R3FKMW/BAoGBAMvlBlxzLSevwwAEQf5x\nnm1Wmy2KDZDoE8YM5u/7qDeUeOnPGFG2v19G+/lOI5nx9KJ36lHDsyQxluMvjU8t\nzCpxfVHe6DnTpkuEvJGYk2BG3jLIoN9w8ZlJ1sAgRZqAHXVzRZTc7JiSy/QsdrFX\nrpuvqUs2vN+rkT8EN8ffNeOa\n-----END PRIVATE KEY-----\n";

const char spreadsheetId[] = "1i4INt1LFrG9XI0cVhgIxyeBvhrvxF4NhfEMIpFMv0rU";

// Google Sheets
bool googleSheetStarted = false;
bool googleSheetReady = false;

unsigned long googleSheetTimer = 0;
unsigned long wifiReconnectTimer = 0;

constexpr uint32_t GOOGLE_SHEET_INTERVAL = 2000;
constexpr uint32_t WIFI_RECONNECT_INTERVAL = 10000;

// ============================================================
// 5. PWM - ESP32 CORE 2.x
// ============================================================

constexpr uint8_t PWM_CHANNEL = 0;

constexpr uint32_t PWM_FREQ = 39000;
constexpr uint8_t PWM_RES = 11;

constexpr uint16_t PWM_MAX = (1U << PWM_RES) - 1;

constexpr uint16_t DUTY_MIN = 500;
constexpr uint16_t DUTY_MAX = 980;
constexpr uint16_t DUTY_INITIAL = 500;
constexpr uint16_t DUTY_STEP = 3;

constexpr uint32_t MPPT_INTERVAL_MS = 1000;

// ============================================================
// 6. GIOI HAN MPPT / SAC
// ============================================================

constexpr float VOL_IN_LIMIT_MIN = 15.00f;
constexpr float VOL_IN_LIMIT_MAX = 25.00f;

constexpr float VOL_BATT_FULL = 13.80f;
constexpr float VOL_BATT_MAX  = 14.20f;

constexpr float EFFICIENCY_RATE = 0.95f;

// ============================================================
// 7. ADS1115
// ============================================================

Adafruit_ADS1115 ads;

constexpr uint8_t ADC_VOUT_CHANNEL    = 1;
constexpr uint8_t ADC_VIN_CHANNEL     = 2;
constexpr uint8_t ADC_CURRENT_CHANNEL = 3;

constexpr uint8_t ADC_SAMPLE_COUNT = 8;

constexpr float ADC_MAX_INPUT_VOLTAGE = 4.096f;

bool adsReady = false;

// ============================================================
// 8. HE SO CAU PHAN AP
// ============================================================

constexpr float VOLTAGE_R_TOP    = 100000.0f;
constexpr float VOLTAGE_R_BOTTOM = 5100.0f;

constexpr float VOLTAGE_DIVIDER_RATIO = 20.90f;

// ============================================================
// 9. ACS712 - 20A
// ============================================================

constexpr float CURRENT_R_TOP    = 3300.0f;
constexpr float CURRENT_R_BOTTOM = 10000.0f;

constexpr float CURRENT_DIVIDER_RATIO = 1.333f;

// ACS712 20A = 100mV/A
constexpr float ACS712_SENSITIVITY = 0.100f;

float currentSensorZeroVoltage = 2.500f;

constexpr uint16_t CURRENT_CALIBRATION_SAMPLES = 200;

constexpr uint8_t CURRENT_CALIBRATION_DELAY_MS = 1;

bool currentCalibrated = false;

// ============================================================
// 10. BIEN CAM BIEN
// ============================================================

float voltageInput = NAN;
float voltageOutput = NAN;

// Dòng điện đầu vào đo bằng ACS712
float currentInput = NAN;

// Dòng điện đầu ra ước tính
float currentOutput = NAN;

float powerInput = NAN;
float powerOutput = NAN;

float adcVoltageVIN = NAN;
float adcVoltageVOUT = NAN;
float adcVoltageCurrent = NAN;

bool sensorDataValid = false;

// ============================================================
// 11. BIEN MPPT P&O
// ============================================================

uint16_t duty = DUTY_INITIAL;

float previousPower = NAN;
float previousVoltage = NAN;
float previousCurrent = NAN;

bool increaseDuty = true;

uint32_t mpptTimer = 0;

bool mpptEnabled = false;
bool mpptInitialized = false;

float voltageDropout = 0.5f;

uint8_t bypassEnable = 0;

enum MpptState {
  MPPT_WAITING,
  MPPT_TRACKING,
  MPPT_BATTERY_LIMIT,
  MPPT_INPUT_ERROR,
  MPPT_SENSOR_ERROR
};

MpptState mpptState = MPPT_WAITING;

// ============================================================
// 12. CAC HAM KHAI BAO
// ============================================================

void writePWM(uint16_t newDuty);
void resetMPPT();

void Read_Sensors();
void runMPPT();
void mppt_PO(float V_new, float I_new);

void backflowControl();

float readADSChannelVoltage(uint8_t channel);
float readVoltageInput();
float readVoltageOutput();
float readCurrent();

bool calibrateCurrentSensor();

void updatePowerCalculation();

void printSensorData();
void updateLCD();

void connectWiFi();
void processGoogleSheets();
void sendGoogleSheets();

void tokenStatusCallback(TokenInfo info);

// ============================================================
// 13. DOC DIEN AP ADS1115
// ============================================================

float readADSChannelVoltage(uint8_t channel) {

  if (!adsReady || channel > 3) {
    return NAN;
  }

  float voltageSum = 0.0f;

  for (uint8_t i = 0; i < ADC_SAMPLE_COUNT; i++) {

    int16_t raw = ads.readADC_SingleEnded(channel);

    if (raw < 0) {
      return NAN;
    }

    float volts = ads.computeVolts(raw);

    if (!isfinite(volts) ||
        volts >= ADC_MAX_INPUT_VOLTAGE * 0.995f) {

      return NAN;
    }

    voltageSum += volts;
  }

  return voltageSum / ADC_SAMPLE_COUNT;
}

// ============================================================
// 14. CALIBRATION ACS712
// ============================================================

bool calibrateCurrentSensor() {

  Serial.println();
  Serial.println("=================================");
  Serial.println("     ACS712 CALIBRATION");
  Serial.println("=================================");

  digitalWrite(buck_EN, LOW);
  digitalWrite(backflow_MOSFET, 1);

  writePWM(0);

  currentCalibrated = false;

  Serial.println("Waiting for ACS712 stabilization...");

  delay(1000);

  float voltageSum = 0.0f;

  uint16_t validSamples = 0;

  for (uint16_t i = 0;
       i < CURRENT_CALIBRATION_SAMPLES;
       i++) {

    float adcVoltage =
        readADSChannelVoltage(ADC_CURRENT_CHANNEL);

    if (isfinite(adcVoltage)) {

      float sensorVoltage =
          adcVoltage * CURRENT_DIVIDER_RATIO;

      voltageSum += sensorVoltage;

      validSamples++;
    }

    delay(CURRENT_CALIBRATION_DELAY_MS);
  }

  if (validSamples <
      CURRENT_CALIBRATION_SAMPLES * 0.9f) {

    Serial.println("ERROR: ACS712 calibration failed!");

    return false;
  }

  currentSensorZeroVoltage =
      voltageSum / validSamples;

  currentCalibrated = true;

  Serial.println("---------------------------------");

  Serial.printf(
    "Valid samples: %u/%u\n",
    validSamples,
    CURRENT_CALIBRATION_SAMPLES
  );

  Serial.printf(
    "ADC zero voltage: %.4f V\n",
    currentSensorZeroVoltage / CURRENT_DIVIDER_RATIO
  );

  Serial.printf(
    "ACS712 zero voltage: %.4f V\n",
    currentSensorZeroVoltage
  );

  Serial.println("Calibration SUCCESS!");
  Serial.println("=================================");

  return true;
}

// ============================================================
// 15. DOC VIN
// ============================================================

float readVoltageInput() {

  adcVoltageVIN =
      readADSChannelVoltage(ADC_VIN_CHANNEL);

  if (!isfinite(adcVoltageVIN)) {
    return NAN;
  }

  return adcVoltageVIN * VOLTAGE_DIVIDER_RATIO;
}

// ============================================================
// 16. DOC VOUT
// ============================================================

float readVoltageOutput() {

  adcVoltageVOUT =
      readADSChannelVoltage(ADC_VOUT_CHANNEL);

  if (!isfinite(adcVoltageVOUT)) {
    return NAN;
  }

  return adcVoltageVOUT * VOLTAGE_DIVIDER_RATIO;
}

// ============================================================
// 17. DOC DONG DIEN ACS712
// ============================================================

float readCurrent() {

  if (!currentCalibrated) {
    return NAN;
  }

  adcVoltageCurrent =
      readADSChannelVoltage(ADC_CURRENT_CHANNEL);

  if (!isfinite(adcVoltageCurrent)) {
    return NAN;
  }

  float sensorVoltage =
      adcVoltageCurrent * CURRENT_DIVIDER_RATIO;

  float current =
      (sensorVoltage - currentSensorZeroVoltage)
      / ACS712_SENSITIVITY;

  current = fabsf(current);

  if (current < 0.1f) {
    current = 0.0f;
  }

  return current;
}

// ============================================================
// 18. TINH TOAN CONG SUAT
// ============================================================

void updatePowerCalculation() {

  if (!sensorDataValid) {

    powerInput = NAN;
    powerOutput = NAN;
    currentOutput = NAN;

    return;
  }

  // Công suất đầu vào
  powerInput = voltageInput * currentInput;

  // Công suất đầu ra ước tính theo hiệu suất
  powerOutput = powerInput * EFFICIENCY_RATE;

  // Dòng điện đầu ra ước tính
  if (voltageOutput > 0.1f) {

    currentOutput =
        powerOutput / voltageOutput;

  } else {

    currentOutput = 0.0f;
  }
}

// ============================================================
// 19. CAP NHAT CAM BIEN
// ============================================================

void Read_Sensors() {

  voltageInput  = readVoltageInput();

  voltageOutput = readVoltageOutput();

  currentInput  = readCurrent();

  sensorDataValid =
      isfinite(voltageInput) &&
      isfinite(voltageOutput) &&
      isfinite(currentInput);

  updatePowerCalculation();

  if (!sensorDataValid) {

    Serial.println(
      "ERROR: Invalid sensor data. Check ADC and wiring."
    );
  }
}

// ============================================================
// 20. DIEU KHIEN PWM
// ============================================================

void writePWM(uint16_t newDuty) {

  if (newDuty > PWM_MAX) {
    newDuty = PWM_MAX;
  }

  duty = newDuty;

  ledcWrite(PWM_CHANNEL, duty);
}

// ============================================================
// 21. RESET DU LIEU MPPT
// ============================================================

void resetMPPT() {

  previousPower = NAN;
  previousVoltage = NAN;
  previousCurrent = NAN;

  increaseDuty = true;

  mpptInitialized = false;
}

// ============================================================
// 22. THUAT TOAN MPPT P&O
// ============================================================

void mppt_PO(float V_new, float I_new) {

  if (!isfinite(V_new) || !isfinite(I_new)) {
    return;
  }

  float P_new = V_new * fabsf(I_new);

  if (!mpptInitialized) {

    previousVoltage = V_new;
    previousCurrent = I_new;
    previousPower = P_new;

    mpptInitialized = true;

    Serial.println("MPPT: Initial operating point saved.");

    return;
  }

  int32_t nextDuty = duty;

  if (P_new > previousPower) {

    if (increaseDuty) {
      nextDuty += DUTY_STEP;
    }
    else {
      nextDuty -= DUTY_STEP;
    }

  } else {

    increaseDuty = !increaseDuty;

    if (increaseDuty) {
      nextDuty += DUTY_STEP;
    }
    else {
      nextDuty -= DUTY_STEP;
    }
  }

  if (nextDuty > DUTY_MAX) {
    nextDuty = DUTY_MAX;
  }

  if (nextDuty < DUTY_MIN) {
    nextDuty = DUTY_MIN;
  }

  writePWM((uint16_t)nextDuty);

  previousVoltage = V_new;
  previousCurrent = I_new;
  previousPower = P_new;

  Serial.printf(
    "MPPT | Vin: %.2f V | Iin: %.2f A | P: %.2f W | Duty: %u | Direction: %s\n",
    V_new,
    I_new,
    P_new,
    duty,
    increaseDuty ? "INCREASE" : "DECREASE"
  );
}

// ============================================================
// 23. QUAN LY SAC VA MPPT
// ============================================================

void runMPPT() {

  if (!currentCalibrated || !sensorDataValid) {

    mpptState = MPPT_SENSOR_ERROR;
    mpptEnabled = false;

    writePWM(0);

    digitalWrite(buck_EN, LOW);

    resetMPPT();

    return;
  }

  // Bảo vệ quá áp đầu ra
  if (voltageOutput >= VOL_BATT_MAX) {

    mpptState = MPPT_BATTERY_LIMIT;
    mpptEnabled = false;

    writePWM(0);

    digitalWrite(buck_EN, LOW);

    resetMPPT();

    Serial.println("PROTECTION: Output voltage too high!");

    return;
  }

  // Kiểm tra điện áp đầu vào
  if (voltageInput <= VOL_IN_LIMIT_MIN ||
      voltageInput >= VOL_IN_LIMIT_MAX) {

    mpptState = MPPT_INPUT_ERROR;
    mpptEnabled = false;

    writePWM(0);

    digitalWrite(buck_EN, LOW);

    resetMPPT();

    Serial.println("MPPT: Input voltage out of range.");

    return;
  }

  // Cho phép driver hoạt động
  digitalWrite(buck_EN, HIGH);

  mpptEnabled = true;

  if (!mpptInitialized && duty < DUTY_MIN) {
    writePWM(DUTY_INITIAL);
  }

  // Giới hạn điện áp sạc
  if (voltageOutput >= VOL_BATT_FULL) {

    mpptState = MPPT_BATTERY_LIMIT;

    if (duty > DUTY_MIN) {

      uint16_t nextDuty =
          (duty > DUTY_MIN + DUTY_STEP)
          ? duty - DUTY_STEP
          : DUTY_MIN;

      writePWM(nextDuty);
    }

    resetMPPT();

    Serial.printf(
      "BATTERY LIMIT | Vout: %.2f V | Duty: %u\n",
      voltageOutput,
      duty
    );

    return;
  }

  // Sạc bình thường
  mpptState = MPPT_TRACKING;

  mppt_PO(voltageInput, currentInput);
}

// ============================================================
// 24. DIEU KHIEN CHONG DONG NGUOC
// ============================================================

void backflowControl() {

  if (!currentCalibrated ||
      !sensorDataValid ||
      !isfinite(voltageInput) ||
      !isfinite(voltageOutput)) {

    bypassEnable = 0;

  } else if (voltageInput >
             voltageOutput + voltageDropout) {

    bypassEnable = 1;

  } else {

    bypassEnable = 0;
  }

  digitalWrite(backflow_MOSFET, bypassEnable);
}

// ============================================================
// 25. LCD1602 - HIEN THI 2 DONG
// ============================================================

void updateLCD() {

  char line1[17];
  char line2[17];

  if (!sensorDataValid) {

    snprintf(line1, sizeof(line1), "Sensor Error");
    snprintf(line2, sizeof(line2), "Check ADS1115");

  } else {

    // Dòng 1: Điện áp và dòng điện đầu vào
    snprintf(
      line1,
      sizeof(line1),
      "VI:%5.1f CI:%4.1f",
      voltageInput,
      currentInput
    );

    // Dòng 2: Điện áp và dòng điện đầu ra
    snprintf(
      line2,
      sizeof(line2),
      "VO:%5.1f CO:%4.1f",
      voltageOutput,
      currentOutput
    );
  }

  lcd.setCursor(0, 0);
  lcd.print(line1);

  lcd.setCursor(0, 1);
  lcd.print(line2);
}

// ============================================================
// 26. SERIAL TELEMETRY
// ============================================================

void printSensorData() {

  Serial.println();
  Serial.println("========== MPPT SYSTEM ==========");

  Serial.printf("VIN ADC: %.4f V\n", adcVoltageVIN);
  Serial.printf("VIN: %.2f V\n", voltageInput);

  Serial.printf("VOUT ADC: %.4f V\n", adcVoltageVOUT);
  Serial.printf("VOUT: %.2f V\n", voltageOutput);

  Serial.printf("Current ADC: %.4f V\n", adcVoltageCurrent);

  Serial.printf(
    "ACS712 Zero: %.4f V\n",
    currentSensorZeroVoltage
  );

  Serial.printf("Input current: %.2f A\n", currentInput);
  Serial.printf("Output current (estimated): %.2f A\n", currentOutput);

  Serial.printf("Input power: %.2f W\n", powerInput);
  Serial.printf("Output power (estimated): %.2f W\n", powerOutput);

  Serial.printf("PWM duty: %u / %u\n", duty, PWM_MAX);

  Serial.printf(
    "PWM frequency: %lu Hz\n",
    (unsigned long)PWM_FREQ
  );

  Serial.printf(
    "Backflow MOSFET: %s\n",
    bypassEnable ? "HIGH / ENABLE" : "LOW / DISABLE"
  );

  Serial.print("MPPT state: ");

  switch (mpptState) {

    case MPPT_WAITING:
      Serial.println("WAITING");
      break;

    case MPPT_TRACKING:
      Serial.println("TRACKING");
      break;

    case MPPT_BATTERY_LIMIT:
      Serial.println("BATTERY LIMIT");
      break;

    case MPPT_INPUT_ERROR:
      Serial.println("INPUT ERROR");
      break;

    case MPPT_SENSOR_ERROR:
      Serial.println("SENSOR ERROR");
      break;
  }

  Serial.printf(
    "WiFi: %s\n",
    WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED"
  );

  Serial.printf(
    "Google Sheets: %s\n",
    googleSheetReady ? "READY" : "NOT READY"
  );

  Serial.println("=================================");
}

// ============================================================
// 27. WIFI
// ============================================================

void connectWiFi() {

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to WiFi...");
}

void processGoogleSheets() {

  // Kiểm tra WiFi và tự động kết nối lại
  if (WiFi.status() != WL_CONNECTED) {

    googleSheetReady = false;

    if (millis() - wifiReconnectTimer >= WIFI_RECONNECT_INTERVAL) {

      wifiReconnectTimer = millis();

      Serial.println("WiFi disconnected. Reconnecting...");

      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    return;
  }

  // Khởi tạo Google Sheets khi WiFi đã kết nối
  if (!googleSheetStarted) {

    Serial.println("WiFi connected!");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    GSheet.setTokenCallback(tokenStatusCallback);

    GSheet.setPrerefreshSeconds(600);

    GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);

    googleSheetStarted = true;

    Serial.println("Google Sheets authentication started.");
  }

  // Xử lý xác thực Google
  googleSheetReady = GSheet.ready();

  // Gửi dữ liệu định kỳ
  if (googleSheetReady &&
      millis() - googleSheetTimer >= GOOGLE_SHEET_INTERVAL) {

    googleSheetTimer = millis();

    if (sensorDataValid) {
      sendGoogleSheets();
    }
  }
}

// ============================================================
// 28. GUI DU LIEU GOOGLE SHEETS
// ============================================================

void sendGoogleSheets() {

  FirebaseJson response;

  FirebaseJson valueRange;

  valueRange.add("majorDimension", "COLUMNS");

  // Thứ tự dữ liệu giống code MPPT V1.2.1
  valueRange.set("values/[0]/[0]", voltageInput);
  valueRange.set("values/[1]/[0]", currentInput);
  valueRange.set("values/[2]/[0]", powerInput);

  valueRange.set("values/[3]/[0]", voltageOutput);
  valueRange.set("values/[4]/[0]", currentOutput);
  valueRange.set("values/[5]/[0]", powerOutput);

  Serial.println();
  Serial.println("Appending Google Sheets data...");

  bool success = GSheet.values.append(
    &response,
    spreadsheetId,
    "Sheet1!A1",
    &valueRange
  );

  if (success) {

    Serial.println("Google Sheets: SUCCESS");

    response.toString(Serial, true);

  } else {

    Serial.println("Google Sheets: FAILED");

    Serial.println(GSheet.errorReason());
  }

  Serial.println();
}

// ============================================================
// 29. GOOGLE TOKEN CALLBACK
// ============================================================

void tokenStatusCallback(TokenInfo info) {

  if (info.status == token_status_error) {

    GSheet.printf(
      "Token error: %s\n",
      GSheet.getTokenError(info).c_str()
    );

  } else {

    GSheet.printf(
      "Token info: type = %s, status = %s\n",
      GSheet.getTokenType(info).c_str(),
      GSheet.getTokenStatus(info).c_str()
    );
  }
}

// ============================================================
// 30. SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  delay(300);

  // ----------------------------------------------------------
  // GPIO
  // ----------------------------------------------------------

  pinMode(buck_IN, OUTPUT);
  pinMode(buck_EN, OUTPUT);

  pinMode(backflow_MOSFET, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(Load, OUTPUT);

  digitalWrite(buck_EN, LOW);
  digitalWrite(backflow_MOSFET, LOW);

  digitalWrite(FAN, LOW);
  digitalWrite(Load, LOW);

  // ----------------------------------------------------------
  // PWM - ESP32 CORE 2.x
  // ----------------------------------------------------------

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);

  ledcAttachPin(buck_IN, PWM_CHANNEL);

  writePWM(0);

  // ----------------------------------------------------------
  // I2C HARDWARE ESP32
  // ----------------------------------------------------------

  Wire.begin(I2C_SDA, I2C_SCL);

  // ----------------------------------------------------------
  // LCD1602
  // ----------------------------------------------------------

  lcd.init();
  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("ESP32 MPPT");

  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  // ----------------------------------------------------------
  // ADS1115
  // ----------------------------------------------------------

  if (!ads.begin(0x48, &Wire)) {

    Serial.println("ERROR: ADS1115 not found!");

    lcd.clear();
    lcd.print("ADS1115 ERROR");

    adsReady = false;

    while (true) {

      writePWM(0);

      digitalWrite(buck_EN, LOW);
      digitalWrite(backflow_MOSFET, LOW);

      delay(500);
    }
  }

  adsReady = true;

  ads.setGain(GAIN_ONE);

  // ----------------------------------------------------------
  // ACS712 CALIBRATION
  // ----------------------------------------------------------

  digitalWrite(buck_EN, LOW);
  digitalWrite(backflow_MOSFET, LOW);

  Serial.println();
  Serial.println("=================================");
  Serial.println(" ESP32 MPPT - P&O ALGORITHM");
  Serial.println(" ADS1115 + ACS712 20A");
  Serial.println(" LCD1602 + GOOGLE SHEETS");
  Serial.println(" PWM: 39 kHz, 11-bit");
  Serial.println("=================================");

  if (!calibrateCurrentSensor()) {

    Serial.println("SYSTEM ERROR: CALIBRATION FAILED!");

    lcd.clear();
    lcd.print("CALIBRATION ERR");

    while (true) {

      writePWM(0);

      digitalWrite(buck_EN, LOW);
      digitalWrite(backflow_MOSFET, LOW);

      delay(500);
    }
  }

  // ----------------------------------------------------------
  // WIFI
  // ----------------------------------------------------------

  connectWiFi();

  // ----------------------------------------------------------
  // SYSTEM READY
  // ----------------------------------------------------------

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("MPPT SYSTEM");

  lcd.setCursor(0, 1);
  lcd.print("System Ready");

  Serial.println();
  Serial.println("SYSTEM READY!");
  Serial.println("ACS712 calibration completed.");

  mpptTimer = millis();

  googleSheetTimer = millis();
  wifiReconnectTimer = millis();
}

// ============================================================
// 31. LOOP
// ============================================================

void loop() {

  // 1. Đọc cảm biến
  Read_Sensors();

  // 2. Điều khiển chống dòng ngược
  backflowControl();

  // 3. Cập nhật thuật toán MPPT
  if (millis() - mpptTimer >= MPPT_INTERVAL_MS) {

    mpptTimer = millis();

    runMPPT();

    printSensorData();
  }

  // 4. Cập nhật LCD
  updateLCD();

  // 5. Xử lý WiFi và Google Sheets
  processGoogleSheets();

  delay(10);
}
