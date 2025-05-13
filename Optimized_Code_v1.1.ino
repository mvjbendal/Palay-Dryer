#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// Pin Definitions
#define ONE_WIRE_BUS 2
#define PIN_BUZZ 11
#define PIN_RUNLED 5
#define PIN_HEATER 13
#define PIN_FAN 3
#define PIN_MOTOR 8
#define PIN_HUMIDITY A1
#define PIN_BUTTON 12
#define PIN_VERT_UP 7
#define PIN_VERT_DN 6

// LCD and Sensor Setup
LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SoftwareSerial mySerial(9, 10);

// Global Variables
float temperatureC;
float humidity;
String HEAT_STATUS = "OFF";
String LCD1 = "", LCD2 = "";

int countdown_time = 900;
int convcountdow_time = 600;
int countdown = 0;
int convcountdown = 0;

bool countdownStarted = false;
bool convStarted = false;
bool OPER_FLAG = false;
bool OPER_INPADDY = false;
int OPER_cnt = 0;
int OPER_READY = 0;

unsigned long previousMillis = 0;
unsigned long lastOperStartTime = 0;
unsigned long lastCountdownTime = 0;
unsigned long lastConvCountdownTime = 0;
bool lcdVisible = true;

const unsigned long blinkInterval = 500;
const unsigned long operStartInterval = 500;
const unsigned long countdownInterval = 1000;

// Async Temperature
bool tempRequested = false;
unsigned long lastTempRequestTime = 0;
const unsigned long tempReadDelay = 100;

// Function Declarations
void LCD_INIT();
void LCD_OUT();
void GET_HUM();
void asyncGetTemp();
void OPER_START();
String formatTime(int seconds);
void BUZZ_ON();
void BUZZ_OFF();
void FAN_ON();
void FAN_OFF();
void HEATING_ON();
void HEATING_OFF();
void CONVEYOR_ON();
void CONVEYOR_OFF();
void VERT_UP();
void VERT_DN();
void RELEASE();

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  lcd.init();
  lcd.backlight();

  LCD_INIT();

  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_MOTOR, OUTPUT);
  pinMode(PIN_VERT_UP, OUTPUT);
  pinMode(PIN_VERT_DN, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_HEATER, OUTPUT);
  pinMode(PIN_RUNLED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  digitalWrite(PIN_RUNLED, HIGH);
  CONVEYOR_ON();
  CONVEYOR_OFF();
  VERT_DN();
  BUZZ_ON();
  delay(200);
  BUZZ_OFF();

  sensors.begin();
  sensors.setResolution(9);   // Set temperature sensor resolution to 8 bits
  sensors.requestTemperatures();
  lastTempRequestTime = millis();
  tempRequested = true;

  
}

void loop() {
  asyncGetTemp();
  GET_HUM();

  OPER_READY = !digitalRead(PIN_BUTTON);
  if (OPER_READY) {
    OPER_cnt++;
    if (OPER_cnt > 5) {
      OPER_FLAG = true;
    }
  }

  if (OPER_FLAG) {
    digitalWrite(PIN_RUNLED, LOW);

    if (!countdownStarted) {
      countdown = countdown_time;
      lastCountdownTime = millis();
      lastOperStartTime = millis();
      countdownStarted = true;
      convStarted = false;
    }

    if (!convStarted) {
      FAN_ON();
      OPER_START();

      if (temperatureC > 45) {
        HEATING_OFF();
        HEAT_STATUS = "OFF";
      } else {
        HEATING_ON();
        HEAT_STATUS = "ON";
      }

      if (countdown == 0) {
        HEATING_OFF();
        RELEASE();
        VERT_UP();

        CONVEYOR_ON();
        convcountdown = convcountdow_time;
        lastConvCountdownTime = millis();
        convStarted = true;
      }
    }

    if (convStarted) {
      if (millis() - lastConvCountdownTime >= 1000) {
        lastConvCountdownTime = millis();

        if (convcountdown > 0) {
          convcountdown--;
          LCD1 = "  CONVEYOR ON   ";
          LCD2 = "Timer:     " + formatTime(convcountdown);
          LCD_OUT();
        } else {
          OPER_READY = 0;
          countdownStarted = false;
          convStarted = false;
          CONVEYOR_OFF();
          digitalWrite(PIN_RUNLED, HIGH);
          LCD1 = "RICEPADDY DRYING";
          LCD2 = "  IS COMPLETED  ";
          FAN_OFF();
          LCD_OUT();
          for (int i = 0; i < 5; i++) BUZZ_ON();
          BUZZ_OFF();
          OPER_cnt = 0;
          OPER_FLAG = false;
          OPER_INPADDY = false;
          lcd.clear();
        }
      }
    }

  } else {
    LCD1 = "   WAITING TO   ";
    LCD2 = "LOAD RICE PADDY ";
    countdown = 0;
    convcountdown = 30;
    countdownStarted = false;
    convStarted = false;

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= blinkInterval) {
      previousMillis = currentMillis;
      lcdVisible = !lcdVisible;

      if (lcdVisible) LCD_OUT();
      else lcd.clear();
    }
  }
}

void asyncGetTemp() {
  unsigned long currentMillis = millis();
  if (!tempRequested) {
    sensors.requestTemperatures();
    lastTempRequestTime = currentMillis;
    tempRequested = true;
  }
  if (tempRequested && (currentMillis - lastTempRequestTime >= tempReadDelay)) {
    temperatureC = sensors.getTempCByIndex(0);
    tempRequested = false;
  }
}

void GET_HUM() {
  int humRaw = analogRead(PIN_HUMIDITY);
  float humVoltage = humRaw * (5.0 / 1023.0);
  humidity = humVoltage * 20.0 + 20.0;
}

void OPER_START() {
  if (millis() - lastOperStartTime >= operStartInterval) {
    lastOperStartTime = millis();
    LCD1 = "T: " + String(temperatureC, 2) + "C H: " + String(humidity, 0) + "%";
    LCD2 = "HEATER:" + HEAT_STATUS + "  " + formatTime(countdown);
    LCD_OUT();

    int TEMP_RAW = temperatureC * 100;
    int HUMID_RAW = humidity * 100;
    Serial.println(String(TEMP_RAW) + "_" + String(HUMID_RAW));
    mySerial.println(String(TEMP_RAW) + "_" + String(HUMID_RAW));
  }

  if (millis() - lastCountdownTime >= countdownInterval) {
    lastCountdownTime = millis();
    if (countdown > 0) countdown--;
  }
}

String formatTime(int seconds) {
  int minutes = seconds / 60;
  int secs = seconds % 60;
  char buffer[6];
  sprintf(buffer, "%02d:%02d", minutes, secs);
  return String(buffer);
}

void LCD_INIT() {
  lcd.setCursor(0, 0);
  lcd.print("   RICE PADDY   ");
  lcd.setCursor(0, 1);
  lcd.print(" DRYING MACHINE ");
  delay(2000);
  lcd.setCursor(0, 0);
  lcd.print("INITIALIZING... ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
}

void LCD_OUT() {
  lcd.setCursor(0, 0);
  lcd.print(LCD1);
  lcd.setCursor(0, 1);
  lcd.print(LCD2);
}

void BUZZ_ON() { digitalWrite(PIN_BUZZ, HIGH); }
void BUZZ_OFF() { digitalWrite(PIN_BUZZ, LOW); }
void HEATING_ON() { digitalWrite(PIN_HEATER, HIGH); }
void HEATING_OFF() { digitalWrite(PIN_HEATER, LOW); }
void FAN_ON() { digitalWrite(PIN_FAN, HIGH); }
void FAN_OFF() { digitalWrite(PIN_FAN, LOW); }
void CONVEYOR_ON() { digitalWrite(PIN_MOTOR, HIGH); }
void CONVEYOR_OFF() { digitalWrite(PIN_MOTOR, LOW); }

void VERT_UP() {
  digitalWrite(PIN_VERT_UP, LOW);
  digitalWrite(PIN_VERT_DN, HIGH);
  delay(5000);
}

void VERT_DN() {
  digitalWrite(PIN_VERT_UP, HIGH);
  digitalWrite(PIN_VERT_DN, LOW);
  delay(5000);
}

void RELEASE() {
  LCD1 = "    RELEASE     ";
  LCD2 = "     PADDY      ";
  LCD_OUT();
}
