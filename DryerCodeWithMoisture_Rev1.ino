#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <math.h>  // Needed for log and pow

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

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial mySerial(9, 10);

const float final_moisture_content = 14.0; // Target MC
float initial_moisture_content = 28.0;     // Set manually or by sensor in future

// Struct to define moisture ranges and drying time
struct MoistureRange {
  float min_mc; //Minimum Range of the Moisture Content
  float max_mc; //Maximum Range of the Moisture Content
  int drying_time_seconds; //Variable for Timer per initial moisture
};

// Drying time map by MC range
MoistureRange mc_ranges[] = {
  {14.0, 21.9, 300}, //{min_mc, max_mc, drying_time_seconds}
  {22.0, 29.9, 600}, //{min_mc, max_mc, drying_time_seconds}
  {30.0, 37.9, 900}, //{min_mc, max_mc, drying_time_seconds}
  {38.0, 45.0, 1200} //{min_mc, max_mc, drying_time_seconds}
};

// Function to determine drying time based on initial MC 
int getDryingTime(float mc) {
  for (int i = 0; i < sizeof(mc_ranges) / sizeof(mc_ranges[0]); i++) {
    if (mc >= mc_ranges[i].min_mc && mc <= mc_ranges[i].max_mc) {
      return mc_ranges[i].drying_time_seconds;
    }
  }
  return 600; // default
}

//Formula sa research paper ng Me
float computeEquilibriumMoisture(float RH, float T) {
  float rh_decimal = RH / 100.0;
  float ln_term = log(1.0 - rh_decimal);
  float numerator = pow(ln_term, 0.409);
  float denominator = 0.000019187 * (T + 511.61);
  float Me = 0.01 * (numerator / denominator);
  return Me;
}

float computeMoistureRatio(float M, float Mi, float Me) {
  return (M - Me) / (Mi - Me);
}

int countdown_time = 600;
int convcountdow_time = 600;
String HEAT_STATUS = "OFF";
String LCD1 = "";
String LCD2 = "";
int OPER_READY = 0;
float temperatureC;
float humidity;
unsigned long previousMillis = 0;
const long blinkInterval = 500;
bool lcdVisible = true;
unsigned long lastOperStartTime = 0;
unsigned long lastCountdownTime = 0;
const unsigned long operStartInterval = 500;
const unsigned long countdownInterval = 1000;

int countdown;
bool countdownStarted = false;
bool convStarted = false;
int convcountdown;

unsigned long lastConvCountdownTime = 0;
bool OPER_FLAG = false;
bool OPER_INPADDY = false;

int OPER_cnt = 0;

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  LCD_INIT();
  GET_HUM();
  GET_TEMP();

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

  // === Moisture-Based Timing Initialization ===
  countdown_time = getDryingTime(initial_moisture_content);

  float Me = computeEquilibriumMoisture(humidity, temperatureC);
  float MR = computeMoistureRatio(final_moisture_content, initial_moisture_content, Me);

  Serial.print("Initial MC: "); Serial.println(initial_moisture_content);
  Serial.print("Final MC: "); Serial.println(final_moisture_content);
  Serial.print("Equilibrium MC (Me): "); Serial.println(Me, 4);
  Serial.print("Moisture Ratio (MR): "); Serial.println(MR, 4);
  Serial.print("Drying Time (sec): "); Serial.println(countdown_time);
}

void loop() {
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

      if (temperatureC > 42) {
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
          LCD2 = "Timer:     " + formatTime(convcountdown) + "   ";
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
          BUZZ_ON(); BUZZ_ON(); BUZZ_ON(); BUZZ_ON(); BUZZ_ON();
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

void OPER_START() {
  if (millis() - lastOperStartTime >= operStartInterval) {
    lastOperStartTime = millis();
    GET_TEMP();
    GET_HUM();
    LCD1 = "T: " + String(temperatureC, 2) + "C H: " + String(humidity, 0) + "%";
    LCD2 = "HEATER:" + HEAT_STATUS + "  " + formatTime(countdown) + "   ";
    LCD_OUT();
    int TEMP_RAW = temperatureC * 100;
    int HUMID_RAW = humidity * 100;
    Serial.println(String(TEMP_RAW) + "_" + String(HUMID_RAW));
    mySerial.println(String(TEMP_RAW) + "_" + String(HUMID_RAW));
  }

  if (millis() - lastCountdownTime >= countdownInterval) {
    lastCountdownTime = millis();
    if (countdown > 0) {
      countdown--;
    }
  }
}

String formatTime(int seconds) {
  int minutes = seconds / 60;
  int secs = seconds % 60;
  char buffer[6];
  sprintf(buffer, "%02d:%02d", minutes, secs);
  return String(buffer);
}

void VERT_UP() {
  digitalWrite(PIN_VERT_UP, LOW);
  digitalWrite(PIN_VERT_DN, HIGH);
  delay(5000);
}

void RELEASE() {
  LCD1 = "    RELEASE     ";
  LCD2 = "     PADDY      ";
  LCD_OUT();
}

void VERT_DN() {
  digitalWrite(PIN_VERT_UP, HIGH);
  digitalWrite(PIN_VERT_DN, LOW);
  delay(5000);
}

void GET_HUM() {
  int humRaw = analogRead(PIN_HUMIDITY);
  float humVoltage = humRaw * (5.0 / 1023.0);
  humidity = humVoltage * 20.0 + 20.0;
}

void GET_TEMP() {
  sensors.requestTemperatures();
  temperatureC = sensors.getTempCByIndex(0);
}

void BUZZ_ON() {
  digitalWrite(PIN_BUZZ, HIGH);
}

void BUZZ_OFF() {
  digitalWrite(PIN_BUZZ, LOW);
}

void HEATING_ON() {
  digitalWrite(PIN_HEATER, HIGH);
}

void HEATING_OFF() {
  digitalWrite(PIN_HEATER, LOW);
}

void FAN_ON() {
  digitalWrite(PIN_FAN, HIGH);
}

void FAN_OFF() {
  digitalWrite(PIN_FAN, HIGH);
}

void CONVEYOR_ON() {
  digitalWrite(PIN_MOTOR, HIGH);
}

void CONVEYOR_OFF() {
  digitalWrite(PIN_MOTOR, LOW);
}

void LCD_INIT() {
  lcd.init();
  lcd.backlight();
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
