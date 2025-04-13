# 🌱 ESP32 Plant Moisture Monitor

An ESP32-based IoT project for monitoring the soil moisture of your plants. It features a web dashboard with historical and live data, and sends Telegram alerts when your green friends get too thirsty.

---

## 🔧 Features

- 🌡️ **Logs moisture data** for two plants (Alfons & Milla) with timestamps  
- 💾 **Stores historical data** using SPIFFS (circular buffer, CSV format)  
- 🌐 **Serves a responsive local website** on your Wi-Fi network  
- 📊 **Web dashboard** with dual graphs per plant: trends and raw readings  
- 💬 **Sends Telegram alerts** when either plant is too dry  
- 🕒 **Production & development modes** for different logging intervals  
- 🔧 **Configurable thresholds**, sensor pins, and logging behavior  

---

## 🧪 Hardware Requirements

- 1× ESP32 development board  
- 2× Capacitive soil moisture sensors (1 per plant)  
- Jumper wires  
- (Optional) Breadboard  

---

## 🧠 Setup

1. Clone this repo:

```bash
git clone https://github.com/MacDIYver8/plant-moisture-monitor.git
cd plant-moisture-monitor
```
2. Create your include/secrets.h file with your Wi-Fi and Telegram credentials:

```cpp
// secrets.h
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* telegramBotToken = "YOUR_BOT_TOKEN";
const char* telegramChatID = "YOUR_CHAT_ID";
```
3. Open in VSCode + PlatformIO

4. Connect your ESP32 and upload the code

5. 🌐 Accessing the Dashboard

When your ESP32 connects to Wi-Fi, the serial monitor will print its IP address.
Open that IP in a browser:

```cpp
http://192.168.1.xxx/
```

You’ll see:

    📈 Alfons and Milla’s moisture history (hourly averages)

    🔍 A zoomed-in chart of the 20 most recent raw readings

    🔴 A red line showing the dryness threshold for reference

6. 🛎️ Telegram Alerts

Telegram messages are sent when the soil moisture goes above a "dry" threshold.

Adjust the threshold in the code:

const int DRY_THRESHOLD = 2200;

⚙️ Configuration

This project supports two logging modes — production and development — to match your use case:

🔄 Switching Modes

Open main.cpp and locate this line near the top:

```cpp
#define PRODUCTION_MODE 1
```
Set to 1 for Production Mode
→ Logs data every 10 minutes (600 seconds)
→ Ideal for real-world daily monitoring

Set to 0 for Development Mode
→ Logs data every 5 seconds
→ Great for debugging and testing sensor setup

🪛 Other Settings

You can also configure the following parameters in main.cpp:

```cpp
#define SENSOR_PIN_1 34      // GPIO pin for Alfons
#define SENSOR_PIN_2 35      // GPIO pin for Milla
#define DRY_THRESHOLD 2200   // Soil moisture threshold for Telegram alert
#define MAX_LOG_ENTRIES 500  // Max data entries to keep per plant (CSV)
#define FORCE_SPIFFS_FORMAT 0 // Set to 1 to force format SPIFFS on next boot
```

🧼 Set FORCE_SPIFFS_FORMAT to 1 only when you want to wipe existing logs. It will auto-reset to 0 after formatting is done.







## 📄 License

This project is licensed under the [Creative Commons Attribution-NonCommercial 4.0 International License](https://creativecommons.org/licenses/by-nc/4.0/).

> You can use, modify, and share this code for **non-commercial purposes only**, as long as you provide credit.









Made with 🌱 by MacDIYver

---

## ✅ What to do next:

1. Save this file as README.md in your repo root.
2. Stage and commit:

```bash
git add README.md
git commit -m "Add README with project details"
git push
