#define BLYNK_TEMPLATE_ID "TMPL6Xt1DLE70"
#define BLYNK_TEMPLATE_NAME "Smart Garden KKN Uin Suska 2026"
#define BLYNK_AUTH_TOKEN "UucUQH2zXj1feUWvUcw0JlZELez_qZm9"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// === CONFIGURATION WIFI ===
char ssid[] = "Kamal@wifi";    // Ganti dengan Nama Wi-Fi / Hotspot HP
char pass[] = "12345678";      // Ganti dengan Password Wi-Fi

// === CONFIGURATION PIN ESP32 ===
#define DHTPIN 23       // Pin Data Sensor Suhu & Kelembapan
#define DHTTYPE DHT11   // Tipe Sensor DHT
#define SOIL_PIN 34     // Pin Analog Sensor Kelembapan Tanah (FC-28)
#define RELAY_PIN 19    // Pin Kontrol Relay Pompa Air

// Inisialisasi Objek Hardware & Timer
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
BlynkTimer timer;

// Variabel Global
int soilPercent = 0;
float temp = 0.0;
float hum = 0.0;
bool manualPumpMode = false;

// ===== BLYNK WRITE (Saklar Pompa V3 dari HP) =====
BLYNK_WRITE(V3) {
  int switchState = param.asInt(); // 1 = ON, 0 = OFF
  
  if (switchState == 1) {
    digitalWrite(RELAY_PIN, LOW);  // Pompa NYALA
    manualPumpMode = true;
  } else {
    digitalWrite(RELAY_PIN, HIGH); // Pompa MATI
    delay(300);                    // ⚡ JEDA PENYELAMAT 1: Tunggu percikan relay padam total
    manualPumpMode = false;
  }
}

// ===== FUNGSI BACA SENSOR & OTOMATISASI =====
void processSystemData() {
  // 1. Baca Sensor Tanah (FC-28)
  int rawSoil = analogRead(SOIL_PIN);
  soilPercent = map(rawSoil, 4095, 1400, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  // 2. Logika Otomatisasi Pompa
  if (!manualPumpMode) {
    if (soilPercent < 20) {
      if (digitalRead(RELAY_PIN) == HIGH) {
        digitalWrite(RELAY_PIN, LOW);  // Pompa NYALA
      }
    } else {
      if (digitalRead(RELAY_PIN) == LOW) {
        digitalWrite(RELAY_PIN, HIGH); // Pompa MATI
        delay(300);                    // ⚡ JEDA PENYELAMAT 2: Tunggu percikan relay padam total
      }
    }
  }

  // 3. JIKA POMPA NYALA: SKIP DHT11 & LCD SEGERA!
  if (digitalRead(RELAY_PIN) == LOW) {
    if (Blynk.connected()) {
      Blynk.virtualWrite(V2, soilPercent);
    }
    Serial.print("Soil: "); Serial.print(soilPercent);
    Serial.println("% | POMPA NYALA (DHT11 & LCD Bypass)");
    return; // Stop di sini biar gak crash
  }

  // 4. JIKA POMPA MATI: BACA DHT11 & UPDATE LCD
  temp = dht.readTemperature();
  hum = dht.readHumidity();

  // Kirim Data ke Blynk
  if (Blynk.connected()) {
    if (!isnan(temp)) Blynk.virtualWrite(V0, temp);
    if (!isnan(hum))  Blynk.virtualWrite(V1, hum);
    Blynk.virtualWrite(V2, soilPercent);
  }

  // Update Layar LCD 16x2
  lcd.setCursor(0, 0);
  lcd.print("Temp:"); 
  if(isnan(temp)) lcd.print("--"); else lcd.print((int)temp);
  lcd.print("C Air:"); 
  if(isnan(hum)) lcd.print("--"); else lcd.print((int)hum);
  lcd.print("%   ");

  lcd.setCursor(0, 1);
  lcd.print("Soil:"); lcd.print(soilPercent); lcd.print("% [P:OFF]");

  // Log Serial Monitor
  Serial.print("Temp: "); Serial.print(temp);
  Serial.print("C | Hum: "); Serial.print(hum);
  Serial.print("% | Soil: "); Serial.print(soilPercent);
  Serial.println("% | Status: PUMP OFF");
}

void setup() {
  // Matikan Detektor Brownout Tegangan Drop
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);

  // Set Output Relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Default: Pompa MATI

  // Inisialisasi Wire I2C dengan Timeout
  Wire.begin();
  Wire.setTimeOut(1000);
  
  dht.begin();
  lcd.init();
  lcd.backlight();

  // Tampilan Opening LCD
  lcd.setCursor(0, 0);
  lcd.print("SMART GARDEN KKN");
  lcd.setCursor(0, 1);
  lcd.print("CONNECTING...   ");

  // KONEKSI BLYNK
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" BLYNK CONNECT ");
  lcd.setCursor(0, 1);
  lcd.print(" SYSTEM READY ");
  delay(1500);
  lcd.clear();

  // Eksekusi fungsi pembacaan data setiap 2 detik
  timer.setInterval(2000L, processSystemData);
}

void loop() {
  Blynk.run();
  timer.run();
}