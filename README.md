# 🌱 ESP32 Plant Moisture Monitor - Under development

An ESP32-based IoT solution for monitoring the soil moisture of your plant, with a live web dashboard and Telegram notifications when it gets too dry.

---

## 🔧 Features

- 📊 **Web dashboard** with live-updating graph (Chart.js)
- 🌡️ **Logs moisture data** every 5 minutes
- 💾 **Stores up to 500 readings** in a circular buffer (using SPIFFS)
- 🌐 **Serves a local website** you can access on your Wi-Fi
- 💬 **Sends Telegram alerts** when the plant is too dry
- 🧠 **Smart formatting**: only wipes memory on first boot

---

## 🧪 Hardware Requirements

- 1× ESP32 development board  
- 1× Capacitive soil moisture sensor  
- Jumper wires
- (Optional) Breadboard

---

## 🧠 Setup

1. Clone this repo:

   ```bash
   git clone https://github.com/your-username/your-repo-name.git
   cd your-repo-name
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

Once connected to Wi-Fi, the serial monitor will print your ESP32’s IP address.
Open it in a browser like:

http://192.168.1.xxx/

You’ll see a live-updating graph of the moisture readings.

6. 🛎️ Telegram Alerts

You’ll get a message when the plant's soil moisture is too dry.

You can adjust the dryness threshold in the code via:

#define DRY_THRESHOLD XXXX

## 📄 License

This project is licensed under the [Creative Commons Attribution-NonCommercial 4.0 International License](https://creativecommons.org/licenses/by-nc/4.0/).

> You can use, modify, and share this code for **non-commercial purposes only**, as long as you provide credit.


Made with 🌱 by MacDIYver

---

## ✅ What to do next:

1. Copy the above into a file called `README.md` at the root of your repo
2. Stage and commit:

```bash
git add README.md
git commit -m "Add README with project details"
git push
