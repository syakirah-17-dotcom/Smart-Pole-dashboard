# Smart Pole Monitoring System

An IoT smart pole monitoring prototype built with **ESP32** that collects real-time sensor readings, triggers 3-tier local visual and audio alerts, syncs telemetry to **Supabase**, and displays live data on a web dashboard alongside official **DOE** air quality data.

## Features
- **Environmental & Gas Sensing:** Reads temperature and humidity (`DHT22`), light level (`LDR`), and carbon monoxide (`SC16-CO` over UART2).
- **3-Tier Local Alert System:** Drives status LEDs (Green, Yellow, Red) and a buzzer for `NORMAL`, `WARNING`, and `DANGER` conditions directly on the ESP32.
- **Cloud Telemetry Sync:** Sends sensor data every 10 seconds via HTTPS POST to the Supabase `sensor_data` table using a 10-second `reading_slot` merge key and automatic Wi-Fi reconnection.
- **Web Dashboard & DOE Integration:** Displays live telemetry via `index.html` together with Department of Environment (DOE) air quality reference data.

## Project Structure
- `Smart_Intelligent_Pole.ino` — ESP32 firmware for sensor reading, 3-tier alert logic, Wi-Fi retry, and Supabase sync.
- `index.html` — Web dashboard interface for viewing live readings and DOE data.
- `README.md` — Project documentation and setup guide.

## Hardware & Pinout (ESP32)
- **DHT22:** Data pin `GPIO 13`
- **LDR:** Analog pin `GPIO 35`
- **SC16-CO (UART2):** RX `GPIO 39`, TX `GPIO 18` (9600 baud)
- **Status LEDs:** Green `GPIO 19`, Yellow `GPIO 23`, Red `GPIO 14`
- **Buzzer:** `GPIO 32`

## Setup & Usage
1. Open `Smart_Intelligent_Pole.ino` in the Arduino IDE.
2. Fill in your local `WIFI_SSID`, `WIFI_PASSWORD`, `SUPABASE_URL`, and `SUPABASE_KEY` values before flashing to the ESP32. *(Do not commit real secrets to a public repository.)*
3. Open `index.html` in a browser or host it on your web deployment platform (such as Vercel or Netlify).
