#define VSP 33
#define VBT 34
#define CIN 32
#define BT 23         //button
#define IN_DRIVER 19  //pwm
#define SD_DRIVER 18  //shutdown
#define RELAY 5
#define LOAD 4

#define PWM_FREQ 39000  // tan so xung pwm
#define PWM_RES 10      // do phan giai 10 bit 0 - 1023
#define VOL_IN_LIMIT_MAX 25.00
#define VOL_IN_LIMIT_MIN 15.00
#define VOL_BATT_FULL 13.80
#define VOL_BATT_MIN 11.50
#define VOL_BATT_MAX 14.20
#define DUTY_MIN 500
#define DUTY_MAX 980

#define RELAY_ON digitalWrite(RELAY, HIGH);
#define RELAY_OFF digitalWrite(RELAY, LOW);

#define DRIVER_ON digitalWrite(SD_DRIVER, HIGH);
#define DRIVER_OFF digitalWrite(SD_DRIVER, LOW);

//WIFI
#include <WiFi.h>
#include <HTTPClient.h>
const char* ssid = "LM";
const char* password = "12341234";
String GOOGLE_SCRIPT_ID = "AKfycbwp4GiYHj0UIysrtpDzqdnhQRqP0L2o7_bp0QYa13CdQ9EYi1TLcDxNhZkqCGqbpmSD";
//LCD
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 20, 4);
//khai bao bien
float
  V_in = 0,
  V_out = 0,
  I = 0,
  V_batt = 0,
  V_old = 0,
  I_old = 0,
  P_old = 0,
  kWh = 0;

uint32_t
  timer1 = 0,  //timer lcd
  timer2 = 0,  //timer relay
  timer3 = 0;  //timer gg sheet

int
  sample = 10,
  duty = 500,
  count = 0;

bool
  load_st = 0,
  increase = true;

String
  state = "OFF",
  load_state = "OFF",
  param = "";
const int deltaDuty = 3;

void setup() {
  Serial.begin(115200);
  //input
  pinMode(BT, INPUT_PULLUP);
  //output
  pinMode(RELAY, OUTPUT);
  pinMode(LOAD, OUTPUT);
  pinMode(IN_DRIVER, OUTPUT);
  pinMode(SD_DRIVER, OUTPUT);

  //LCD2004 - I2C
  lcd.init();
  lcd.backlight();
  lcd.clear();

  digitalWrite(LOAD, load_st);
  RELAY_OFF
  DRIVER_ON
  ledcAttach(IN_DRIVER, PWM_FREQ, PWM_RES);

  timer3 = millis();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && count < 20) {
    lcd.setCursor(2, 0);
    lcd.print("Connecting...");
    count++;
    delay(500);
    lcd.clear();
  }
  if (WiFi.status() == WL_CONNECTED) {
    lcd.setCursor(2, 0);
    lcd.print("Connected to Wifi:");
    lcd.setCursor(2, 1);
    lcd.print(ssid);
    delay(2000);
    lcd.clear();
  } else {
    lcd.setCursor(2, 0);
    lcd.print("Can't Connect!");
    delay(2000);
    lcd.clear();
  }
}

void loop() {
  readSensors();
  readButton();
  runMode();
  relay_Run();
  if (millis() - timer1 >= 1000) {
    timer1 = millis();
    lcd_display();
    kWh += (I * V_in) / 3600;
  }
  print_gg_sheet();
}
void runMode() {
  if (V_in > VOL_IN_LIMIT_MIN && V_in < VOL_IN_LIMIT_MAX) {
    //RELAY_ON
    state = "CHARGE";
    if (V_out < 14.00) {
      mppt_PO(V_in, I);
    } else {
      duty -= deltaDuty;
      if (duty < DUTY_MIN) duty = DUTY_MIN;
      ledcWrite(IN_DRIVER, duty);
    }
  } else if (V_out <= 11.00 || V_out > 15.00 || V_in <= VOL_IN_LIMIT_MIN || V_in >= VOL_IN_LIMIT_MAX) {
    //RELAY_OFF
    state = "OFF";
  }
}
void relay_Run() {
  if (millis() - timer2 > 5000) {
    timer2 = millis();
    if (V_in > VOL_IN_LIMIT_MIN && V_in < VOL_IN_LIMIT_MAX) {
      state = "CHARGE";
    } else if (V_out <= 11.00 || V_out > 15.00 || V_in <= VOL_IN_LIMIT_MIN || V_in >= VOL_IN_LIMIT_MAX) {
      state = "OFF";
    }
    if (state != "CHARGE") {
      RELAY_OFF
    } else RELAY_ON
  }
}
void mppt_PO(float V_new, float I_new) {
  float P_new = V_new * I_new;

  if (P_new > P_old) {
    duty += (increase ? deltaDuty : -deltaDuty);
  } else {
    increase = !increase;
    duty += (increase ? deltaDuty : -deltaDuty);
  }

  if (duty > DUTY_MAX) duty = DUTY_MAX;
  if (duty < DUTY_MIN) duty = DUTY_MIN;

  ledcWrite(IN_DRIVER, duty);

  V_old = V_new;
  I_old = I_new;
  P_old = P_new;
}
void readSensors() {  //doc 3 cam bien
  V_out = readVoltageOutput();
  V_in = readVoltageInput();
  I = readCurrent();
}
void lcd_display() {  //hien thi LCD
  lcd.setCursor(4, 0);
  lcd.print("MPPT CHARGER");
  lcd.setCursor(3, 1);
  lcd.print("MODE: ");
  if (state == "OFF") {
    lcd.setCursor(9, 1);
    lcd.print("OFF   ");
  } else {
    lcd.setCursor(9, 1);
    lcd.print("CHARGE");
  }
  
  lcd.setCursor(0, 2);
  lcd.print(V_in);
  lcd.setCursor(5, 2);
  lcd.print("V");

  if (state == "OFF") {
    lcd.setCursor(7, 2);
    lcd.print(0.05);
  } else {
    lcd.setCursor(7, 2);
    lcd.print(I);
  }
  lcd.setCursor(11, 2);
  lcd.print("A");

  lcd.setCursor(13, 2);
  lcd.print(V_in * I);
  lcd.setCursor(18, 2);
  lcd.print("W");

  lcd.setCursor(0, 3);
  lcd.print(V_out);
  lcd.setCursor(5, 3);
  lcd.print("V");

  lcd.setCursor(11, 3);
  lcd.print("Duty:");
  lcd.setCursor(16, 3);
  lcd.print(duty);
  if (duty >= 1000) {
    lcd.setCursor(19, 3);
    lcd.print(" ");
  }
}
float readVoltageInput() {  //doc gt dien ap vao
  int adc = 0;
  for (int i = 0; i < sample; i++) {
    adc += analogRead(VSP);
  }
  float v_adc = (adc / 3160.0 / sample) * 2.543 * 10.2;
  if (v_adc > 11 && v_adc < 17) {
    return v_adc + 0.7;
  } else if (v_adc > 17 && v_adc < 22) {
    return v_adc + 0.5;
  } else if (v_adc > 22) {
    return v_adc + 0.2;
  }
}
float readVoltageOutput() {  //doc gt dien ap ra
  int adc = 0;
  for (int i = 0; i < sample; i++) {
    adc += analogRead(VBT);
  }
  float v_adc = (adc / 1750.0 / sample) * 1.425 * 10.1;
  if (v_adc > 11 && v_adc < 14) {
    return v_adc + 0.3;
  } else if (v_adc > 14) {
    return v_adc;
  }
}
float readCurrent() {  //doc gt dong dien
  float voltage = 0;
  float current = 0;
  for (int i = 0; i < sample; i++) {
    int adc = analogRead(CIN);
    voltage += adc * 3.3 / 4095.0;
  }
  voltage = voltage / sample + 0.1;
  //Serial.println(voltage);
  current = ((voltage - 1.68) / 0.125) - 0.2;
  if (current <= 0) current = abs(current);
  //else if (state != "CHARGE ") current = 0.05;
  return current;
}
void readButton() {
  if (digitalRead(BT) == 0) {
    load_st = !load_st;
    delay(200);
  }
  digitalWrite(LOAD, load_st);
  if (load_st == 0) load_state = "OFF";
  else load_state = "ON";
}
void print_gg_sheet() {
  if (millis() - timer3 >= 60000) {
    param = "mode=" + state + "&" + "Vin=" + String(V_in) + "&" + "I=" + String(I)
            + "&" + "Vout=" + String(V_out) + "&" + "kWh=" + String(kWh) + "&" + "load=" + String(load_state);
    write_to_google_sheet(param);
    //Serial.println("updated");
    timer3 = millis();
  }
}
void write_to_google_sheet(String params) {
  HTTPClient http;
  String url = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + params;
  http.begin(url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();
  // String payload;
  // if (httpCode > 0) {
  //   payload = http.getString();
  //   Serial.println("Payload: " + payload);
  // }
  http.end();
}

void test() {
  //DRIVER_OFF
  //driver_state = 1;
  RELAY_ON
  if (V_out < 13.80) {
    mppt_PO(V_in, I);
  } else {
    duty -= deltaDuty;
    if (duty < DUTY_MIN) duty = DUTY_MIN;
    ledcWrite(IN_DRIVER, duty);
  }
  //ledcWrite(IN_DRIVER, 700);
}