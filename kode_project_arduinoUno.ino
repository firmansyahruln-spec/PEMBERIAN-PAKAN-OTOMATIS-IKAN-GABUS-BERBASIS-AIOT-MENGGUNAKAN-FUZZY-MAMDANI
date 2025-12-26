#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Servo.h>
#include <RTClib.h>
#include <SoftwareSerial.h>

// ================= HARDWARE =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

RTC_DS3231 rtc;
Servo servoMotor;

// SoftwareSerial ke NodeMCU
SoftwareSerial espSerial(2, 3); // RX, TX

// Sensor
const int phPin = A0;
const int turbidityPin = A1;
const int servoPin = 9;

// ================= JADWAL PAKAN =================
int feedHour[3] = {8, 12, 17};
int feedMin[3]  = {0, 0, 0};
bool fedFlag[3] = {false, false, false};

// ================= VARIABEL =================
String serialBuf = "";
char lastFeed[16] = "--:--";

// ================= SETUP =================
void setup() {
  Serial.begin(9600);      // USB Monitor
  espSerial.begin(9600);   // ke NodeMCU

  lcd.init();
  lcd.backlight();
  tempSensor.begin();

  // servoMotor.attach(servoPin);
  // servoMotor.write(0);

  if (!rtc.begin()) {
    Serial.println("RTC ERROR");
    lcd.print("RTC ERROR");
    while (1);
  }

  lcd.clear();
  lcd.print("IoT Kolam Ready");
  Serial.println("Arduino READY");
  delay(1500);
}

// ================= LOOP =================
void loop() {
  DateTime now = rtc.now();

  float suhu = readTemp();
  float ph   = readPH();
  float ntu  = readTurbidity();  // NTU final, sudah mapping

  tampilLCD(suhu, ph, ntu);      // tampil di LCD

  // Kirim data ke NodeMCU
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 3000) {
    lastSend = millis();
    espSerial.print("DATA|");
    espSerial.print(suhu, 2);
    espSerial.print("|");
    espSerial.print(ph, 2);
    espSerial.print("|");
    espSerial.println(ntu, 2);  // kirim NTU 2 desimal
  }

  // Jadwal pakan otomatis
  cekJadwal(now);

  // Perintah manual dari Blynk
  bacaSerial();

  delay(200);
}


// // ================= LOOP =================
// void loop() {
//   DateTime now = rtc.now();

//   float suhu = readTemp();
//   float ph   = readPH();
//   float ntu  = readTurbidity();

//   tampilLCD(suhu, ph, ntu);

//   // Kirim data ke NodeMCU
//   static unsigned long lastSend = 0;
//   if (millis() - lastSend > 3000) {
//     lastSend = millis();
//     espSerial.println(
//       "DATA|" + String(suhu,2) + "|" +
//       String(ph,2) + "|" +
//       String(ntu,1)
//     );
//   }

//   // Jadwal pakan otomatis
//   cekJadwal(now);

//   // Perintah manual dari Blynk
//   bacaSerial();

//   delay(200);
// }

// ================= JADWAL =================
void cekJadwal(DateTime now) {
  for (int i = 0; i < 3; i++) {
    if (now.hour() == feedHour[i] &&
        now.minute() == feedMin[i] &&
        !fedFlag[i]) {

      Serial.println("PAKAN OTOMATIS AKTIF");
      beriPakan();
      fedFlag[i] = true;
    }
  }

  for (int i = 0; i < 3; i++) {
    if (now.minute() != feedMin[i]) {
      fedFlag[i] = false;
    }
  }
}

// ================= SERIAL =================
void bacaSerial() {
  while (espSerial.available()) {
    char c = espSerial.read();
    if (c == '\n') {
      serialBuf.trim();

      if (serialBuf == "FEED_NOW") {
        Serial.println(">>> PAKAN MANUAL AKTIF <<<");
        beriPakan();
      }

      serialBuf = "";
    } else {
      serialBuf += c;
    }
  }
}

// ================= SERVO =================
void beriPakan() {
  DateTime now = rtc.now();

  Serial.println("Servo aktif (beri pakan)");

  servoMotor.attach(servoPin);   // attach hanya saat perlu
  delay(50);

  servoMotor.write(160);         // buka penuh
  delay(1200);

  servoMotor.write(20);          // tutup
  delay(500);

  servoMotor.detach();           // hentikan PWM (INI KUNCI)

  sprintf(lastFeed, "%02d:%02d", now.hour(), now.minute());

  Serial.print("Pakan diberikan: ");
  Serial.println(lastFeed);

  espSerial.println("EVENT|FEED|" + String(lastFeed));
}

// void beriPakan() {
//   DateTime now = rtc.now();

//   servoMotor.write(90);
//   delay(800);
//   servoMotor.write(0);

//   sprintf(lastFeed, "%02d:%02d", now.hour(), now.minute());

//   Serial.print("Pakan diberikan: ");
//   Serial.println(lastFeed);

//   espSerial.println("EVENT|FEED|" + String(lastFeed));
// }

// ================= LCD =================
void tampilLCD(float t, float ph, float ntu) {
  static unsigned long last = 0;
  static int page = 0;

  if (millis() - last > 2500) {
    last = millis();
    page = (page + 1) % 2;
    lcd.clear();

    if (page == 0) {
      lcd.setCursor(0,0);
      lcd.print("T:"); lcd.print(t,1); lcd.print("C");
      lcd.setCursor(0,1);
      lcd.print("pH:"); lcd.print(ph,1);
    } else {
      lcd.setCursor(0,0);
      lcd.print("NTU:");
      lcd.print(ntu,0);
      lcd.setCursor(0,1);
      lcd.print("Feed:");
      lcd.print(lastFeed);
    }
  }
}

// ================= SENSOR =================
float readTemp() {
  tempSensor.requestTemperatures();
  delay(200);  // waktu konversi DS18B20
  return tempSensor.getTempCByIndex(0);
}

float readTurbidity() {
  int total = 0;
  const int samples = 20;
  for (int i = 0; i < samples; i++) {
    total += analogRead(turbidityPin);
    delay(10);
  }
  float raw = total / (float)samples;

  // Mapping fleksibel sesuai sensor
  const float rawMin = 373;  // air jernih
  const float rawMax = 233;  // keruh
  const float ntuMax = 35.0;

  float ntu = (raw - rawMin) * (ntuMax / (rawMax - rawMin));
  ntu = constrain(ntu, 0, ntuMax);

  Serial.print("Raw turbidity: "); Serial.print(raw);
  Serial.print(" | NTU: "); Serial.println(ntu);

  return ntu;
}


float readPH() {
  delay(200); // beri jeda sebelum baca pH setelah sensor lain aktif
  int total = 0;
  for (int i = 0; i < 10; i++) { // ambil sampel lebih banyak biar stabil
    total += analogRead(phPin);
    delay(10);
  }
  int raw = total / 10;
  float voltage = (raw / 1023.0) * 5.0;
  float phValue = 7 - ((voltage - 3.4) / 0.18);
  return constrain(phValue, 0, 14);
}