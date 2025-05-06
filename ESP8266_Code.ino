#include <Wire.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <MySQL_Generic.h>
#include <string.h>

#define WIFI_SSID     "connect"
#define WIFI_PASSWORD "22222222"
#define BUILTIN_LED_PIN LED_BUILTIN
#define GPI16_PIN 16

char          server[]                = "184.168.102.202";
uint16_t      server_port             = 3306;

char          user[]                  = "iotdash_db";
char          password[]              = "iotdash_db";
char          database[]              = "iotdash_db";

MySQL_Connection conn((Client *)&client);
MySQL_Query *query_mem;
SoftwareSerial nodeSerial(D5, D6);

char          buffers[16];
String        IP_Address_str          = "";
String        UPDATE_SENSOR           = "";
int           cnt = 0;
void setup() {
  pinMode(BUILTIN_LED_PIN, OUTPUT);
  pinMode(GPI16_PIN, OUTPUT);
  Serial.begin(9600);

  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("CONNECTING....");
    delay(500);
  }
  nodeSerial.begin(9600);
  IPAddress ip = WiFi.localIP();
  Serial.println("CONNECTED TO WIFI");
  Serial.println(WiFi.localIP());

  digitalWrite(GPI16_PIN, HIGH);
  sprintf(buffers, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  IP_Address_str = String(buffers);
}

void loop() {
  if (nodeSerial.available()) {
    cnt = 0;
    String msg = nodeSerial.readStringUntil('\n');
    Serial.println("Received: " + msg);

    if (msg.length() >= 9 && msg.charAt(4) == '_') {
      String t_top = msg.substring(0, 4);       // First 4 digits
      String hum = msg.substring(5, 9);         // Last 4 digits after underscore
      UPDATE_SENSOR = "UPDATE iotdash_db.tbl_rice_sensors SET t_top = '" + t_top + "', hum = '" + hum + "', status = 'OPERATION' WHERE id = 1";

      if (conn.connectNonBlocking(server, server_port, user, password) != RESULT_FAIL) {
        MySQL_Query query_mem = MySQL_Query(&conn);

        if (conn.connected()) {
          query_mem.execute(UPDATE_SENSOR.c_str());
          Serial.println(UPDATE_SENSOR);
          Serial.println("Sensor data updated to database.");
        } else {
          Serial.println("NOT CONNECTED connect fail");
        }

        delay(100);
      } else {
        Serial.println("NOT CONNECTED server fail");
      }
    } else {
     
      Serial.println("Invalid message format.");
    }
  }
  else{
     cnt++;
  }

  if(cnt > 5){
     UPDATE_SENSOR = "UPDATE iotdash_db.tbl_rice_sensors SET status = 'STOP' WHERE id = 1";

      if (conn.connectNonBlocking(server, server_port, user, password) != RESULT_FAIL) {
        MySQL_Query query_mem = MySQL_Query(&conn);

        if (conn.connected()) {
          query_mem.execute(UPDATE_SENSOR.c_str());
          Serial.println(UPDATE_SENSOR);
          Serial.println("Sensor data updated to database.");
        } else {
          Serial.println("NOT CONNECTED connect fail");
        }

        delay(100);
      } else {
        Serial.println("NOT CONNECTED server fail");
      }
  }

  // Only UPDATE statement



  delay(1000);  // Delay before next update
}