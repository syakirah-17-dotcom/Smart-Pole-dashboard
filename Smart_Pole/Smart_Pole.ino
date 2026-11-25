#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// WIFI CONFIG
char ssid[] = "lotuseuu";
char pass[] = "sizzling";

// SUPABASE CONFIG
const char* supabaseUrl = "https://rmwmcahrjqbkqrzmjegm.supabase.co";
const char* supabaseKey = "sb_publishable_ujMJCWVSOgrXoDyuQ7cGmw_o-9ILntI";

// =========================
// Pin Definitions
// =========================
#define DHTPIN 13
#define DHTTYPE DHT22

#define LDRPIN 35
#define MQ7PIN 34 // Pin Analog ADC untuk Sensor MQ-7 (A0 MQ-7 -> GPIO34)

#define GREEN_LED 19
#define YELLOW_LED 23
#define RED_LED 14
#define BUZZER 32

DHT dht(DHTPIN, DHTTYPE);

// Pembolehubah Global
float temperature = 0;
float humidity = 0;
int lightPercent = 0;
float coPPM = 0.0;
String statusText = "NORMAL";

// Pemasa untuk baca sensor & hantar ke Supabase
unsigned long lastSensorMillis = 0;
unsigned long lastSupabaseMillis = 0;
const unsigned long SENSOR_INTERVAL = 2000;    // Baca setiap 2 saat
const unsigned long SUPABASE_INTERVAL = 10000; // Hantar ke Supabase setiap 10 saat

// Pemasa kawalan cuba semula WiFi supaya tidak berlaku ralat config
unsigned long lastReconnectAttempt = 0;

// ====================================================
// FUNGSI NADA BUZZER BERBEZA
// ====================================================
void playWarningBeep() {
  // Nada amaran (1000 Hz, durasi 150 ms)
  tone(BUZZER, 1000, 150); 
}

void playDangerBeep() {
  // Nada bahaya tinggi / siren kecemasan (2500 Hz, durasi 80 ms)
  tone(BUZZER, 2500, 80); 
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

  // --- C. BACA MQ-7 CO (CALIBRATED FOR 5V VIN SUPPLY) ---
  int mq7Raw = analogRead(MQ7PIN);
  
  if (mq7Raw < 3500) {
    coPPM = map(mq7Raw, 0, 3500, 0, 5);       // Udara bersih normal (0-5 PPM)
  } else {
    coPPM = map(mq7Raw, 3500, 4095, 5, 100);  // Kenaikan gas CO jika ada pencemaran
  }

  if (coPPM < 0) coPPM = 0;
  Serial.print("[MQ-7 CO Sensor] Raw ADC: ");
  Serial.print(mq7Raw);
  Serial.print(" | Anggaran Kepekatan: ");
  Serial.print(coPPM);
  Serial.println(" PPM");

  // --- D. LOGIK STATUS, LED & BUZZER ---
  bool isDanger = (temperature > 35.0) ||
                  (coPPM > 30.0) ||         // Threshold Bahaya (>30 PPM)
                  (humidity > 90.0) ||
                  (lightPercent < 5);

  bool isWarning = ((temperature > 30.0 && temperature <= 35.0) ||
                    (coPPM >= 10.0 && coPPM <= 30.0) || // Threshold Warning (10-30 PPM)
                    (humidity > 75.0 && humidity <= 90.0) ||
                    (lightPercent >= 5 && lightPercent <= 20));

  if (isDanger) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    
    playDangerBeep(); // Bunyi siren bahaya tinggi
    statusText = "DANGER";

  } else if (isWarning) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    
    playWarningBeep(); // Bunyi amaran pendek
    statusText = "WARNING";

  } else {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);
    
    noTone(BUZZER); // Matikan buzzer jika kondisi normal
    statusText = "NORMAL";
  }

  // --- E. PAPARAN SERIAL MONITOR ---
  Serial.println("\n========================================");
  Serial.println("     🌐 SMART POLE MONITORING 🌐      ");
  Serial.println("========================================");
  Serial.print(" 🌡️  Temperature : "); Serial.print(temperature, 1); Serial.println(" °C");
  Serial.print(" 💧  Humidity    : "); Serial.print(humidity, 1);    Serial.println(" %");
  Serial.print(" ☀️  Light Level : "); Serial.print(lightPercent);    Serial.println(" %");
  Serial.print(" 💨  CO Gas (MQ7): "); Serial.print(coPPM, 1);        Serial.println(" PPM");
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
// 2. Hantar Rekod Data ke Supabase Database
// ----------------------------------------------------
void sendToSupabase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Supabase] Can't connect to WiFi!");
    return;
  }

  HTTPClient http;
  String fullUrl = String(supabaseUrl) + "/rest/v1/sensor_data";

  http.begin(fullUrl);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  http.addHeader("Prefer", "return=representation");

  String jsonPayload = "{";
  jsonPayload += "\"temperature\":" + String(temperature, 2) + ",";
  jsonPayload += "\"humidity\":" + String(humidity, 2) + ",";
  jsonPayload += "\"light_level\":" + String(lightPercent) + ",";
  jsonPayload += "\"co_ppm\":" + String(coPPM, 2) + ",";
  jsonPayload += "\"status\":\"" + statusText + "\"";
  jsonPayload += "}";

  Serial.println("[Supabase] Sending ESP32-A data...");
  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    Serial.println("[Supabase] ESP32-A data sent successfully!");
  } else {
    Serial.print("[Supabase] FAILED! Response code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

// ----------------------------------------------------
// SETUP & LOOP
// ----------------------------------------------------
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(ssid, pass);

  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected Successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi Connection Failed! Akan cuba semula secara automatik.");
  }
}

void setup() {
  Serial.begin(115200);

  dht.begin();
  pinMode(LDRPIN, INPUT);
  pinMode(MQ7PIN, INPUT);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  connectWiFi();

  Serial.println("Smart Pole System Activated (Tanpa Blynk)!");
}

void loop() {
  // Pengurusan sambungan semula WiFi dengan jeda masa (elak ralat config)
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastReconnectAttempt >= 10000) {
      lastReconnectAttempt = currentMillis;
      Serial.println("⚠️  WiFi terputus. Cuba sambung semula...");
      connectWiFi();
    }
  }

  unsigned long currentMillis = millis();

  // Baca sensor setiap 2 saat
  if (currentMillis - lastSensorMillis >= SENSOR_INTERVAL) {
    lastSensorMillis = currentMillis;
    readSensors();
  }

  // Hantar ke Supabase setiap 10 saat (hanya jika WiFi bersambung)
  if (WiFi.status() == WL_CONNECTED) {
    if (currentMillis - lastSupabaseMillis >= SUPABASE_INTERVAL) {
      lastSupabaseMillis = currentMillis;
      sendToSupabase();
    }
  }
}