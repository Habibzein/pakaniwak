#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

const char* ssid = "anjay123";
const char* password = "bibabe123";

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Ubah alamat I2C sesuai dengan modul LCD Anda
Servo mekanik;

const int ledPin = 5;  // Pin untuk LED
char time1[6], time2[6];
bool hasFedTime1 = false;
bool hasFedTime2 = false;

unsigned long lastHttpRequestTime = 0;
unsigned long lastTimeCheck = 0;
unsigned long lastToggleTime = 0;
const unsigned long httpRequestInterval = 10000;  // Interval 10 detik untuk cek HTTP
const unsigned long timeCheckInterval = 1000;     // Interval 1 detik untuk cek waktu
const unsigned long toggleInterval = 5000;        // Interval 5 detik untuk toggle tampilan

bool isFeeding = false;
bool displayTime1 = true;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);  // Pastikan LED mati pada awal
  mekanik.attach(12); //D6
  mekanik.write(0);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    lcd.setCursor(0, 1);
    lcd.print("                ");  // Clear baris kedua
    lcd.setCursor(0, 1);
    lcd.print("Connecting WiFi");
  }

  Serial.println("Connected to WiFi");
  lcd.setCursor(0, 1);
  lcd.print("                ");  // Clear baris kedua
  lcd.setCursor(0, 1);
  lcd.print("WiFi Connected");

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    lcd.setCursor(0, 1);
    lcd.print("                ");  // Clear baris kedua
    lcd.setCursor(0, 1);
    lcd.print("RTC not found");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    lcd.setCursor(0, 1);
    lcd.print("                ");  // Clear baris kedua
    lcd.setCursor(0, 1);
    lcd.print("Setting RTC time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set waktu RTC ke waktu kompilasi
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  EEPROM.begin(512);
  readFeedTimes();
}

void loop() {
  unsigned long currentMillis = millis();

  // Pengecekan HTTP tiap 10 detik
  if (currentMillis - lastHttpRequestTime >= httpRequestInterval) {
    lastHttpRequestTime = currentMillis;
    checkFeedTimes();
  }

  // Pengecekan waktu tiap 1 detik
  if (currentMillis - lastTimeCheck >= timeCheckInterval) {
    lastTimeCheck = currentMillis;
    checkTimeAndFeed();
  }

   // Toggle tampilan tiap 5 detik
  if (currentMillis - lastToggleTime >= toggleInterval) {
    lastToggleTime = currentMillis;
    toggleDisplay();
  }
}

void checkFeedTimes() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;

    http.begin(client, "http://192.168.245.90/pakaniwak/get_feed_times.php");

    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Feed times from server: " + payload);
      
      int commaIndex = payload.indexOf(',');
      String t1 = payload.substring(0, commaIndex);
      String t2 = payload.substring(commaIndex + 1);

      // Potong detik dari string waktu
      t1 = t1.substring(0, 5);  // Ambil "HH:MM" dari "HH:MM:SS"
      t2 = t2.substring(0, 5);  // Ambil "HH:MM" dari "HH:MM:SS"

      t1.toCharArray(time1, 6);
      t2.toCharArray(time2, 6);

      saveFeedTimes(time1, time2);
    } else {
      Serial.println("Error on HTTP request");
      lcd.setCursor(0, 1);
      lcd.print("                ");  // Clear baris kedua
      lcd.setCursor(0, 1);
      lcd.print("HTTP Error");
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
    lcd.setCursor(0, 1);
    lcd.print("                ");  // Clear baris kedua
    lcd.setCursor(0, 1);
    lcd.print("WiFi Disconnected");
  }
}

void checkTimeAndFeed() {
  DateTime now = rtc.now();
  char currentTime[6];
  sprintf(currentTime, "%02d:%02d", now.hour(), now.minute());
  char currentSecond[3];
  sprintf(currentSecond, "%02d", now.second());

  Serial.print("Current time: ");
  Serial.print(currentTime);
  Serial.print(":");
  Serial.println(currentSecond);

  lcd.setCursor(0, 0);
  lcd.print("                ");  // Clear baris kedua
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(currentTime);
  lcd.print(":");
  lcd.print(currentSecond);

  if (strcmp(currentTime, time1) == 0 && strcmp(currentSecond, "00") == 0 && !hasFedTime1) {
    feed(time1);
    hasFedTime1 = true;
  } else if (strcmp(currentTime, time2) == 0 && strcmp(currentSecond, "00") == 0 && !hasFedTime2) {
    feed(time2);
    hasFedTime2 = true;
  }

  // Reset status setelah waktu berlalu untuk waktu berikutnya
  if (strcmp(currentTime, time1) != 0) {
    hasFedTime1 = false;
  }
  if (strcmp(currentTime, time2) != 0) {
    hasFedTime2 = false;
  }
}

void kasihPakan(int jumlah) {
  unsigned long startMillis;
  for(int a = 1; a <= jumlah; a++) {
    startMillis = millis();
    mekanik.write(150);
    while (millis() - startMillis < 100) {
      // Tunggu selama 100 ms
    }

    startMillis = millis();
    mekanik.write(0);
    while (millis() - startMillis < 100) {
      // Tunggu selama 100 ms
    }
  }
}

void feed(const char* feedTime) {
  Serial.print("Feeding at ");
  Serial.println(feedTime);
  isFeeding = true;  // Set flag to indicate feeding
  lcd.setCursor(0, 1);
  lcd.print("                ");  // Clear baris kedua
  lcd.setCursor(0, 1);
  lcd.print("Feeding: ON ");
  kasihPakan(10);
  isFeeding = false;  // Reset flag after feeding
}

void toggleDisplay() {
  if (isFeeding) return;  // Jangan lakukan toggle display saat feeding
  
  lcd.setCursor(0, 1);
  lcd.print("                ");  // Clear baris kedua

  if (displayTime1) {
    lcd.setCursor(0, 1);
    lcd.print("Pakan 1: ");
    lcd.print(time1);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Pakan 2: ");
    lcd.print(time2);
  }

  displayTime1 = !displayTime1;  // Toggle flag
}

void saveFeedTimes(const char* t1, const char* t2) {
  for (int i = 0; i < 6; i++) {
    EEPROM.write(i, t1[i]);
    EEPROM.write(i + 6, t2[i]);
  }
  EEPROM.commit();
}

void readFeedTimes() {
  for (int i = 0; i < 6; i++) {
    time1[i] = EEPROM.read(i);
    time2[i] = EEPROM.read(i + 6);
  }
}
