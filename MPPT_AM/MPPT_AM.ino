#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
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

float voltageDropout = 0.5f;
uint8_t bypassEnable = 0;

// ============================================================
// 2. PWM - ARDUINO ESP32 CORE 2.x
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
// 3. GIOI HAN MPPT / SAC
// ============================================================

constexpr float VOL_IN_LIMIT_MIN = 15.00f;
constexpr float VOL_IN_LIMIT_MAX = 25.00f;

constexpr float VOL_BATT_FULL = 13.80f;
constexpr float VOL_BATT_MAX  = 14.20f;

// ============================================================
// 4. ADS1115
// ============================================================

Adafruit_ADS1115 ads;

constexpr uint8_t ADC_VOUT_CHANNEL    = 1;
constexpr uint8_t ADC_VIN_CHANNEL     = 2;
constexpr uint8_t ADC_CURRENT_CHANNEL = 3;

constexpr uint8_t ADC_SAMPLE_COUNT = 8;
constexpr float ADC_MAX_INPUT_VOLTAGE = 4.096f;

bool adsReady = false;

// ============================================================
// 5. HE SO CAU PHAN AP
// ============================================================

constexpr float VOLTAGE_R_TOP    = 100000.0f;
constexpr float VOLTAGE_R_BOTTOM = 5100.0f;

constexpr float VOLTAGE_DIVIDER_RATIO = 20.90f;

// ============================================================
// 6. ACS712 - CURRENT CALIBRATION
// ============================================================

constexpr float CURRENT_R_TOP    = 3300.0f;
constexpr float CURRENT_R_BOTTOM = 10000.0f;

constexpr float CURRENT_DIVIDER_RATIO =1.333f;

constexpr float ACS712_SENSITIVITY = 0.1f;

// Điện áp zero được cập nhật khi calibration
float currentSensorZeroVoltage = 2.500f;

// Số mẫu calibration
constexpr uint16_t CURRENT_CALIBRATION_SAMPLES = 200;

// Thời gian giữa hai mẫu
constexpr uint8_t CURRENT_CALIBRATION_DELAY_MS = 1;

// Trạng thái calibration
bool currentCalibrated = false;

// ============================================================
// 7. BIEN CAM BIEN
// ============================================================

float voltageInput = NAN;
float voltageOutput = NAN;
float currentOutput = NAN;

float adcVoltageVIN = NAN;
float adcVoltageVOUT = NAN;
float adcVoltageCurrent = NAN;

bool sensorDataValid = false;

// ============================================================
// 8. BIEN MPPT P&O
// ============================================================

uint16_t duty = DUTY_INITIAL;

float previousPower = NAN;
float previousVoltage = NAN;
float previousCurrent = NAN;

bool increaseDuty = true;

uint32_t mpptTimer = 0;

bool mpptEnabled = false;
bool mpptInitialized = false;

enum MpptState {
  MPPT_WAITING,
  MPPT_TRACKING,
  MPPT_BATTERY_LIMIT,
  MPPT_INPUT_ERROR,
  MPPT_SENSOR_ERROR
};

MpptState mpptState = MPPT_WAITING;

// ============================================================
// 9. DOC DIEN AP ADS1115
// ============================================================

float readADSChannelVoltage(uint8_t channel) {

  if (!adsReady || channel > 3) {
    return NAN;
  }

  float voltageSum = 0.0f;

  for (uint8_t i = 0; i < ADC_SAMPLE_COUNT; i++) {

    int16_t raw = ads.readADC_SingleEnded(channel);

    if (raw < 0) {
      Serial.printf(
        "ERROR: ADC A%u returned negative raw value!\n",
        channel
      );

      return NAN;
    }

    float volts = ads.computeVolts(raw);

    if (!isfinite(volts) ||
        volts >= ADC_MAX_INPUT_VOLTAGE * 0.995f) {

      Serial.printf(
        "WARNING: ADC A%u near/at full scale: %.4f V\n",
        channel, volts
      );

      return NAN;
    }

    voltageSum += volts;
  }

  // Tinh dien ap trung binh
  float averageVoltage = voltageSum / ADC_SAMPLE_COUNT;

  // Chi lam tron 2 chu so thap phan cho ACS712 (A3)
  // if (channel == ADC_CURRENT_CHANNEL) {
  //   averageVoltage = roundf(averageVoltage * 100.0f) / 100.0f;
  // }

  return averageVoltage;
}

// ============================================================
// 10. CALIBRATION ACS712
// ============================================================

bool calibrateCurrentSensor() {

  Serial.println();
  Serial.println("=================================");
  Serial.println("     ACS712 CALIBRATION");
  Serial.println("=================================");

  // Tat toan bo mach cong suat
  digitalWrite(buck_EN, LOW);
  digitalWrite(backflow_MOSFET, 1);

  writePWM(0);

  currentCalibrated = false;

  // Cho nguon ACS712 on dinh
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

  // Kiem tra so mau hop le
  if (validSamples <
      CURRENT_CALIBRATION_SAMPLES * 0.9f) {

    Serial.println("ERROR: ACS712 calibration failed!");

    return false;
  }

  // Tinh dien ap zero trung binh
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
// 11. DOC VIN
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
// 12. DOC VOUT
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
// 13. DOC DONG DIEN ACS712
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

    // Luôn lấy độ lớn dòng điện
    current = fabsf(current);

    // Dưới 0.1 A thì xem như bằng 0
    if (current < 0.1f) {
      current = 0.0f;
    }

    return current;
}

// ============================================================
// 14. CAP NHAT CAM BIEN
// ============================================================

void Read_Sensors() {

  voltageInput  = readVoltageInput();
  voltageOutput = readVoltageOutput();
  currentOutput = readCurrent();

  sensorDataValid =
      isfinite(voltageInput) &&
      isfinite(voltageOutput) &&
      isfinite(currentOutput);

  if (!sensorDataValid) {

    Serial.println(
      "ERROR: Invalid sensor data. Check ADC and wiring."
    );
  }
}

// ============================================================
// 15. DIEU KHIEN PWM - LEDC CHANNEL
// ============================================================

void writePWM(uint16_t newDuty) {

  if (newDuty > PWM_MAX) {
    newDuty = PWM_MAX;
  }

  duty = newDuty;

  ledcWrite(PWM_CHANNEL, duty);
}

// ============================================================
// 16. RESET DU LIEU MPPT
// ============================================================

void resetMPPT() {

  previousPower = NAN;
  previousVoltage = NAN;
  previousCurrent = NAN;

  increaseDuty = true;

  mpptInitialized = false;
}

// ============================================================
// 17. THUAT TOAN MPPT P&O
// ============================================================

void mppt_PO(float V_new, float I_new) {

  if (!isfinite(V_new) || !isfinite(I_new)) {
    return;
  }

  float P_new = V_new * fabsf(I_new);

  // Lan dau tien
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

  }
  else {

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
    "MPPT | Vin: %.2f V | I: %.3f A | P: %.2f W | Duty: %u | Direction: %s\n",
    V_new,
    I_new,
    P_new,
    duty,
    increaseDuty ? "INCREASE" : "DECREASE"
  );
}

// ============================================================
// 18. QUAN LY SAC VA MPPT
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

  // Bao ve qua ap dau ra
  if (voltageOutput >= VOL_BATT_MAX) {

    mpptState = MPPT_BATTERY_LIMIT;
    mpptEnabled = false;

    writePWM(0);
    digitalWrite(buck_EN, LOW);

    resetMPPT();

    Serial.println("PROTECTION: Output voltage too high!");

    return;
  }

  // Kiem tra VIN
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

  // Cho phep driver hoat dong
  digitalWrite(buck_EN, HIGH);

  mpptEnabled = true;

  if (!mpptInitialized && duty < DUTY_MIN) {
    writePWM(DUTY_INITIAL);
  }

  // Gioi han dien ap sac
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

  // Sac binh thuong
  mpptState = MPPT_TRACKING;

  mppt_PO(voltageInput, currentOutput);
}

// ============================================================
// 19. DIEU KHIEN CHONG DONG NGUOC
// ============================================================

void backflowControl() {

  // Khong cho phep neu chua calibration
  if (!currentCalibrated ||
      !sensorDataValid ||
      !isfinite(voltageInput) ||
      !isfinite(voltageOutput)) {

    bypassEnable = 0;
  }

  else if (voltageInput >
           voltageOutput + voltageDropout) {

    bypassEnable = 1;
  }

  else {

    bypassEnable = 0;
  }

  digitalWrite(backflow_MOSFET, bypassEnable);
}

// ============================================================
// 20. IN DU LIEU CAM BIEN VA MPPT
// ============================================================

void printSensorData() {

  Serial.println();
  Serial.println("========== MPPT SYSTEM ==========");

  Serial.printf("VIN ADC: %.4f V\n", adcVoltageVIN);
  Serial.printf("VIN:     %.2f V\n", voltageInput);

  Serial.printf("VOUT ADC: %.4f V\n", adcVoltageVOUT);
  Serial.printf("VOUT:     %.2f V\n", voltageOutput);

  Serial.printf("Current ADC: %.4f V\n", adcVoltageCurrent);
  Serial.printf("Current votlage: %.4f V\n", adcVoltageCurrent * 1.333f);
  Serial.printf("Current:     %.3f A\n", currentOutput);

  Serial.printf(
    "ACS712 Zero: %.4f V\n",
    currentSensorZeroVoltage
  );

  Serial.printf(
    "Input power: %.2f W\n",
    sensorDataValid
      ? voltageInput * fabsf(currentOutput)
      : 0.0f
  );

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

  Serial.println("=================================");
}

// ============================================================
// 21. SETUP
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

  // Tat mach cong suat ngay khi khoi dong
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
  // ADS1115
  // ----------------------------------------------------------

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!ads.begin(0x48, &Wire)) {

    Serial.println("ERROR: ADS1115 not found!");

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
  Serial.println(" ADS1115 + ACS712");
  Serial.println(" PWM: 39 kHz, 11-bit");
  Serial.println("=================================");

  Serial.println("Starting ACS712 calibration...");

  if (!calibrateCurrentSensor()) {

    Serial.println("SYSTEM ERROR: CALIBRATION FAILED!");

    while (true) {

      writePWM(0);

      digitalWrite(buck_EN, LOW);
      digitalWrite(backflow_MOSFET, LOW);

      delay(500);
    }
  }

  // ----------------------------------------------------------
  // SYSTEM READY
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("SYSTEM READY!");
  Serial.println("ACS712 calibration completed.");
  Serial.println("Backflow control is now permitted.");

  mpptTimer = millis();
}

// ============================================================
// 22. LOOP
// ============================================================

void loop() {

  // Doc cam bien
  Read_Sensors();

  // Dieu khien MOSFET chong dong nguoc
  backflowControl();

  // Cap nhat MPPT theo chu ky
  if (millis() - mpptTimer >= MPPT_INTERVAL_MS) {

    mpptTimer = millis();

    // runMPPT();
    printSensorData();
  }

  delay(10);
}
