#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>

#define SENSOR_PIN 34               // Moisture sensor pin
#define DRY_THRESHOLD 2300          // Adjust this for your plant
#define CHECK_INTERVAL_MS 60000   // 1 minute = 60000 milliseconds

// Wi-Fi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Telegram Bot token and chat ID
const char* telegramBotToken = "YOUR_TELEGRAM_TOKEN";  // <-- paste your token here
const char* telegramChatID = "YOUR_CHAT_ID";   // <-- paste your chat ID here

WebServer server(80);

unsigned long lastCheckTime = 0;
bool notificationSent = false;

// Serve graph page
void handleRoot() {
    String html = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Plant Moisture Graph</title>
            <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
        </head>
        <body>
            <h2>Live Soil Moisture Plot</h2>
            <canvas id="moistureChart" width="400" height="200"></canvas>
            <script>
                const ctx = document.getElementById('moistureChart').getContext('2d');
                const moistureChart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Soil Moisture',
                            data: [],
                            borderWidth: 1
                        }]
                    },
                    options: {
                        scales: {
                            y: {
                                beginAtZero: true
                            }
                        }
                    }
                });

                async function fetchData() {
                    const response = await fetch('/data');
                    const moisture = await response.text();
                    const now = new Date().toLocaleTimeString();

                    if (moistureChart.data.labels.length > 20) {
                        moistureChart.data.labels.shift();
                        moistureChart.data.datasets[0].data.shift();
                    }

                    moistureChart.data.labels.push(now);
                    moistureChart.data.datasets[0].data.push(parseInt(moisture));
                    moistureChart.update();
                }

                setInterval(fetchData, 2000);
            </script>
        </body>
        </html>
    )rawliteral";

    server.send(200, "text/html", html);
}

// Serve moisture data for the graph
void handleData() {
    int moisture = analogRead(SENSOR_PIN);
    server.send(200, "text/plain", String(moisture));
}

// Send Telegram notification
void sendTelegramNotification(int moisture) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "https://api.telegram.org/bot" + String(telegramBotToken) + "/sendMessage?chat_id=" + String(telegramChatID) + "&text=🌵 Your plant is too dry! Moisture: " + String(moisture);
        http.begin(url);
        int httpResponseCode = http.GET();
        http.end();
        if (httpResponseCode > 0) {
            Serial.println("Telegram notification sent.");
        } else {
            Serial.println("Failed to send Telegram message.");
        }
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi Connected!");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());

    lastCheckTime = millis();  // Safe timer reset on boot
    Serial.println("Timer reset");

    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.begin();
    Serial.println("Web server started.");
}

void loop() {
    // ✅ Auto-reconnect Wi-Fi if disconnected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi lost, reconnecting...");
        WiFi.disconnect();
        WiFi.reconnect();
        delay(1000);
    }

    server.handleClient();

    // ✅ Hourly moisture check
    if (millis() - lastCheckTime >= CHECK_INTERVAL_MS) {
        lastCheckTime = millis();
        int moisture = analogRead(SENSOR_PIN);
        Serial.println("Minutely moisture check: " + String(moisture));

        if (moisture > DRY_THRESHOLD && !notificationSent) {
            sendTelegramNotification(moisture);
            notificationSent = true;  // Prevent repeated notifications until re-moisturized
        }

        if (moisture <= DRY_THRESHOLD) {
            notificationSent = false; // Reset if soil is moist again
        }
    }
}
