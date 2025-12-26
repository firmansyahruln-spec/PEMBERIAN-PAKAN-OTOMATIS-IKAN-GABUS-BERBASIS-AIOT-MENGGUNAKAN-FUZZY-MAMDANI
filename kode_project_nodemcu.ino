#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID   "TMPL6ZUTuxnM2"
#define BLYNK_TEMPLATE_NAME "Monitoring Kolam"
#define BLYNK_AUTH_TOKEN    "KkLGzfavs9_9-pb-7fnwAoJ-SSnYYf28"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <SoftwareSerial.h>

// ============ WIFI ============
char ssid[] = "Yoiiiii";
char pass[] = "Homenizam";

// ============ SERIAL ============
SoftwareSerial unoSerial(D5, D6); // RX, TX

WidgetRTC rtc;
BlynkTimer timer;

// ============ DATA ============
float suhu = 0, ph = 0, ntu = 0;
String buffer = "";

// ============ SETUP ============
void setup() {
  Serial.begin(9600);
  unoSerial.begin(9600);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  rtc.begin();

  timer.setInterval(3000L, kirimBlynk);

  Serial.println("NodeMCU READY");
}

// ============ LOOP ============
void loop() {
  Blynk.run();
  timer.run();
  bacaArduino();
}

// ============ SERIAL ARDUINO ============
void bacaArduino() {
  while (unoSerial.available()) {
    char c = unoSerial.read();
    if (c == '\n') {
      buffer.trim();
      parseData(buffer);
      buffer = "";
    } else {
      buffer += c;
    }
  }
}

// ============ PARSE DATA ============
void parseData(String data) {
  if (data.startsWith("DATA|")) {
    int p1 = data.indexOf('|',5);
    int p2 = data.indexOf('|',p1+1);

    suhu = data.substring(5,p1).toFloat();
    ph   = data.substring(p1+1,p2).toFloat();
    ntu  = data.substring(p2+1).toFloat();
  }
  else if (data.startsWith("EVENT|FEED|")) {
    String waktu = data.substring(11);
    Serial.print("INFO: Pakan pada ");
    Serial.println(waktu);
    Blynk.virtualWrite(V4, waktu);
  }
}

// ============ KIRIM KE BLYNK ============
void kirimBlynk() {
  Blynk.virtualWrite(V1, suhu);
  Blynk.virtualWrite(V2, ph);
  Blynk.virtualWrite(V3, ntu);
}

// ============ TOMBOL MANUAL ============
BLYNK_WRITE(V6) {
  if (param.asInt() == 1) {
    Serial.println("TOMBOL BLYNK: FEED MANUAL");
    unoSerial.println("FEED_NOW");
  }
}

