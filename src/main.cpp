#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <time.h>
#include <secrets.h>
#include <Preferences.h>

#define SENSOR_PIN 34               // Moisture sensor pin
#define DRY_THRESHOLD 2200          // Adjust this for your plant
#define CHECK_INTERVAL_MS 300000      // Checks every 5 minutes
#define MAX_LOG_ENTRIES 500          // Max number of data points

WebServer server(80);

unsigned long lastCheckTime = 0;
bool notificationSent = false;

// Log moisture to CSV with circular buffer behavior (RTC time instead of elapsed seconds)
void logMoisture(int moisture) {
    time_t now;
    time(&now); // Get current RTC time as Unix timestamp

    // Read existing lines
    std::vector<String> lines;
    File file = SPIFFS.open("/data.csv", FILE_READ);
    if (file) {
        while (file.available()) {
            lines.push_back(file.readStringUntil('\n'));
        }
        file.close();
    }

    // Keep only last MAX_LOG_ENTRIES - 1
    if (lines.size() >= MAX_LOG_ENTRIES) {
        lines.erase(lines.begin()); // Remove oldest
    }

    // Add new entry using RTC time
    lines.push_back(String(now) + "," + String(moisture));

    // Write back all lines
    file = SPIFFS.open("/data.csv", FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    for (const auto& line : lines) {
        file.printf("%s\r\n", line.c_str());
    }
    file.close();

    Serial.println("Logged: " + String(now) + "," + String(moisture));
}

// Send Telegram notification
void sendTelegramNotification(int moisture) {
  if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = "https://api.telegram.org/bot" + String(telegramBotToken) + "/sendMessage?chat_id=" + String(telegramChatID) + "&text=\U0001F335 Your plant is too dry! Moisture: " + String(moisture);
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

// Serve the graph page
void handleRoot() {
    String html = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Plant Moisture Graph</title>
            <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
        </head>
        <body>
            <h2>Bean Plant Soil Moisture History</h2>
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
                            x: { 
                                type: 'category', // Use category to display formatted labels
                                title: { display: true, text: 'Time' },
                            },
                            y: { 
                                beginAtZero: true,
                                title: { display: true, text: 'Soil Moisture' }
                            }
                        }
                    }
                });

                async function fetchCSV() {
                    const response = await fetch('/log');
                    const text = await response.text();
                    const lines = text.trim().split("\n");
                    const timestamps = [];
                    const moistures = [];

                    lines.forEach(line => {
                        const parts = line.trim().split(",");
                        if (parts.length === 2) {
                            const [time, value] = parts;
                            const date = new Date(parseInt(time.trim()) * 1000); // Convert Unix timestamp to milliseconds
                            const formattedTime = date.toLocaleString(); // Format as human-readable date and time
                            timestamps.push(formattedTime);
                            moistures.push(parseInt(value));
                        }
                    });

                    // Update chart data
                    moistureChart.data.labels = timestamps;
                    moistureChart.data.datasets[0].data = moistures;

                    moistureChart.update();
                }

                setInterval(fetchCSV, 5000);
            </script>
        </body>
        </html>
    )rawliteral";

    server.send(200, "text/html", html);
}

void setup() {
    Serial.begin(115200);

    Preferences preferences;
    preferences.begin("PlantMonitor", false); // Open namespace "PlantMonitor"

    bool isFormatted = preferences.getBool("isFormatted", false); // Check if formatted

    if (!isFormatted) {
        Serial.println("First boot detected. Formatting SPIFFS...");
        if (SPIFFS.format()) {
            Serial.println("SPIFFS formatted successfully.");
            preferences.putBool("isFormatted", true); // Set flag to indicate formatting is done
        } else {
            Serial.println("SPIFFS formatting failed!");
        }
    } else {
        Serial.println("SPIFFS already formatted. Skipping format step.");
    }

    preferences.end(); // Close preferences

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    Serial.println("SPIFFS mounted successfully");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi Connected!");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println("Time initialized");

    lastCheckTime = millis();
    Serial.println("Timer reset");

    server.on("/", handleRoot);

    server.on("/log", []() {
        File file = SPIFFS.open("/data.csv", FILE_READ);
        if (!file) {
            server.send(500, "text/plain", "Failed to open file");
            return;
        }
        server.streamFile(file, "text/plain");
        file.close();
    });

    server.begin();
    Serial.println("Web server started.");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi lost, reconnecting...");
        WiFi.disconnect();
        WiFi.reconnect();
        delay(1000);
    }

    server.handleClient();

    if (millis() - lastCheckTime >= CHECK_INTERVAL_MS) {
        lastCheckTime = millis();
        int moisture = analogRead(SENSOR_PIN);
        Serial.println("Moisture check: " + String(moisture));

        logMoisture(moisture);

        if (moisture > DRY_THRESHOLD && !notificationSent) {
            sendTelegramNotification(moisture);
            notificationSent = true;
        }

        if (moisture <= DRY_THRESHOLD) {
            notificationSent = false;
        }
    }
}
