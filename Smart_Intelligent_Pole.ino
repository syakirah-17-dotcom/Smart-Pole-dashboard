
/*
  ======================================================================
  ESP32-A SMART POLE SYSTEM (FULL SENSOR TRIGGER & CUSTOM BUZZER)
  (DHT22 Temp/Humid + LDR Light + SC16-CO Gas + Blynk + Supabase)
  ======================================================================
*/

// ---------- BLYNK CONFIG ----------
#define BLYNK_TEMPLATE_ID   "TMPL6PnXPsbC-"
#define BLYNK_TEMPLATE_NAME "ESP32 weather station"
#define BLYNK_AUTH_TOKEN    "XOIGn82N6kwoEfhqdBAcjGOkrh7u5N4_"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>

// ---------- WIFI CONFIG ----------
char ssid[] = "MATRIX-IOT_2.4G";
char pass[] = "Davinci321!";

// --------- SUPABASE CONFIG ---------
const char* supabaseUrl = "https://rmwmcahrjqbkqrzmjegm.supabase.co";
// Same working publishable/anon key already proven on ESP32-B for this project
const char* supabaseKey = "sb_publishable_ujMJCWVSOgrXoDyuQ7cGmw_o-9ILntI";

// TLS client for Supabase HTTPS requests (mirrors ESP32-B's proven approach)
WiFiClientSecure supabaseClient;

// =========================
// Pin Definitions (TIDAK DIUBAH)
// =========================
#define DHTPIN 13
#define DHTTYPE DHT22

#define LDRPIN 35 

// Pin UART2 untuk Sensor SC16-CO
#define SC16_RXD 39 
#define SC16_TXD 18 

#define GREEN_LED 19
#define YELLOW_LED 23
#define RED_LED 14
#define BUZZER 32

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

// Pembolehubah Global
float temperature = 0;
float humidity = 0;
int lightPercent = 0;
float coPPM = 0.0;
String statusText = "NORMAL";

// Flag & Timer untuk kawal spam Push Notification
bool warningNotified = false;
bool dangerNotified = false;
unsigned long lastNotifyTime = 0;
const unsigned long NOTIFY_COOLDOWN = 60000; 

void playWarningBeep() {
  tone(BUZZER, 1000, 100); 
}

void playDangerBeep() {
  tone(BUZZER, 2000); 
}

// ----------------------------------------------------
// 1. Baca All Sensors & Kawal Perkakasan (Setiap 2 Saat)
// ----------------------------------------------------
void readSensors() {
  // --- A. BACA DHT22 ---
  float tempRead = dht.readTemperature();
  float humRead = dht.readHumidity();

  if (!isnan(tempRead) && !isnan(humRead)) {
    temperature = tempRead;
    humidity = humRead;
  } else {
    Serial.println("[Warning] Gagal membaca dari sensor DHT22!");
  }

  // --- B. BACA LDR ---
  int ldrRaw = analogRead(LDRPIN);
  lightPercent = map(ldrRaw, 4095, 0, 0, 100); 
  lightPercent = constrain(lightPercent, 0, 100);

  // --- C. BACA SC16-CO ---
  if (Serial2.available() >= 9) {
    byte response[9];
    Serial2.readBytes(response, 9);

    if (response[0] == 0xFF) {
      int highByte = response[2];
      int lowByte = response[3];
      coPPM = (highByte * 256) + lowByte; 
    }
  } else if (Serial2.available() > 0) {
    float rawPPM = Serial2.parseFloat();
    if (rawPPM >= 0) {
      coPPM = rawPPM;
    }
  }

  // --- D. LOGIK STATUS, LED & BUZZER ---
  bool isDanger = (temperature > 35.0) || 
                  (coPPM > 35.0) || 
                  (humidity > 90.0) || 
                  (lightPercent < 5);

  bool isWarning = ((temperature > 30.0 && temperature <= 35.0) || 
                    (coPPM >= 9.0 && coPPM <= 35.0) || 
                    (humidity > 75.0 && humidity <= 90.0) || 
                    (lightPercent >= 5 && lightPercent <= 20));

  if (isDanger) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    playDangerBeep();
    statusText = "DANGER";

  } else if (isWarning) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    playWarningBeep();
    statusText = "WARNING";

  } else {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);
    noTone(BUZZER);
    statusText = "NORMAL";
  }

  // --- E. PAPARAN SERIAL MONITOR ---
  Serial.println("\n========================================");
  Serial.println("     🌐 SMART POLE MONITORING 🌐      ");
  Serial.println("========================================");
  Serial.print(" 🌡️  Temperature : "); Serial.print(temperature, 1); Serial.println(" °C");
  Serial.print(" 💧  Humidity    : "); Serial.print(humidity, 1);    Serial.println(" %");
  Serial.print(" ☀️  Light Level : "); Serial.print(lightPercent);    Serial.println(" %");
  Serial.print(" 💨  CO Gas      : "); Serial.print(coPPM, 1);        Serial.println(" PPM");
  Serial.println("----------------------------------------");
  
  if (statusText == "DANGER") {
    Serial.println(" 🚨 SYSTEM STATUS : [ DANGER ] ⚠️");
  } else if (statusText == "WARNING") {
    Serial.println(" ⚠️  SYSTEM STATUS : [ WARNING ] 🟡");
  } else {
    Serial.println(" ✅ SYSTEM STATUS : [ NORMAL ] 💚");
  }
  Serial.println("========================================\n");
}

// ----------------------------------------------------
// 2. Hantar Data ke Blynk (Setiap 5 Saat)
// ----------------------------------------------------
void sendToBlynk() {
  if (!Blynk.connected()) return;

  unsigned long currentMillis = millis();

  if (statusText == "DANGER") {
    Blynk.virtualWrite(V3, 0);   
    Blynk.virtualWrite(V4, 0);   
    Blynk.virtualWrite(V5, 255); 

    if (!dangerNotified || (currentMillis - lastNotifyTime >= NOTIFY_COOLDOWN)) {
      Blynk.logEvent("danger_alert", "🚨 DANGER: Smart Pole sensor alert triggered!");
      dangerNotified = true;
      warningNotified = false;
      lastNotifyTime = currentMillis;
    }

  } else if (statusText == "WARNING") {
    Blynk.virtualWrite(V3, 0);   
    Blynk.virtualWrite(V4, 255); 
    Blynk.virtualWrite(V5, 0);   

    if (!warningNotified || (currentMillis - lastNotifyTime >= NOTIFY_COOLDOWN)) {
      Blynk.logEvent("warning_alert", "⚠️ WARNING: Smart Pole sensor values increasing!");
      warningNotified = true;
      dangerNotified = false;
      lastNotifyTime = currentMillis;
    }

  } else {
    Blynk.virtualWrite(V3, 255); 
    Blynk.virtualWrite(V4, 0);   
    Blynk.virtualWrite(V5, 0);   

    warningNotified = false;
    dangerNotified = false;
  }

  // Telemetry Datastreams
  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V2, statusText);
  Blynk.virtualWrite(V6, lightPercent);
  Blynk.virtualWrite(V7, coPPM);
}

// ----------------------------------------------------
// 3. Hantar Rekod Data ke Supabase Database (DENGAN LOGIK UPSERT & MERGE)
// ----------------------------------------------------
void sendToSupabase() {
  if (WiFi.status() == WL_CONNECTED) {

    // TLS setup for Supabase HTTPS endpoint (matches ESP32-B's proven approach)
    supabaseClient.setInsecure();

    HTTPClient http;

    // A. URL dengan parameter on_conflict supaya data digabungkan ke reading_slot
    String fullUrl = String(supabaseUrl) + "/rest/v1/sensor_data?on_conflict=reading_slot";
    http.begin(supabaseClient, fullUrl);
    
    // B. Header Wajib Supabase & Header Merge
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", String("Bearer ") + supabaseKey);
    http.addHeader("Prefer", "resolution=merge-duplicates,return=minimal");

    // C. Penjanaan reading_slot berasaskan masa (Masa semasa diwakili dalam blok 10 saat)
    // Ini memastikan ESP32-A dan ESP32-B menulis ke slot ID yang sama dalam sela masa 10s
    unsigned long currentReadingSlot = millis() / 10000;

    // D. Payload JSON (Ditambah parameter reading_slot)
    String jsonPayload = "{";
    jsonPayload += "\"reading_slot\":" + String(currentReadingSlot) + ",";
    jsonPayload += "\"temperature\":" + String(temperature) + ",";
    jsonPayload += "\"humidity\":" + String(humidity) + ",";
    jsonPayload += "\"light_level\":" + String(lightPercent) + ",";
    jsonPayload += "\"co_ppm\":" + String(coPPM) + ",";
    jsonPayload += "\"status\":\"" + statusText + "\"";
    jsonPayload += "}";

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode == 201 || httpResponseCode == 200) {
      Serial.print("[Supabase] Success Data Merged/Created! Code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("[Supabase] Can't deliver Data! Code: ");
      Serial.println(httpResponseCode);
      if (httpResponseCode > 0) {
        Serial.print("[Supabase] Response: ");
        Serial.println(http.getString());
      }
    }

    http.end();
  } else {
    Serial.println("[Supabase] Can't connect to WiFi!");
  }
}

// ----------------------------------------------------
// SETUP & LOOP
// ----------------------------------------------------
void setup() {
  Serial.begin(115200);

  Serial2.begin(9600, SERIAL_8N1, SC16_RXD, SC16_TXD);

  dht.begin();
  pinMode(LDRPIN, INPUT);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  WiFi.begin(ssid, pass);
  Blynk.config(BLYNK_AUTH_TOKEN);

  timer.setInterval(2000L, readSensors);     
  timer.setInterval(5000L, sendToBlynk);     
  timer.setInterval(10000L, sendToSupabase);  

  Serial.println("Smart Pole System Activated Successfully!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!Blynk.connected()) {
      Blynk.connect(1000);
    } else {
      Blynk.run();
    }
  }

  timer.run();
}