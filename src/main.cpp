#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <time.h>
#include <secrets.h>
#include <Preferences.h>

#define SENSOR_PIN 34               // Moisture sensor pin
#define MAX_LOG_ENTRIES 500         // Max number of data points
#define LOG_INTERVAL_SECONDS 5
#define FORCE_SPIFFS_FORMAT true    // Set to 'false' when you don't want to reformat

const int DRY_THRESHOLD = 2200;     // Adjust this for your plant

WebServer server(80);

unsigned long lastCheckTime = 0;
bool notificationSent = false;
time_t lastLoggedTime = 0;

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

void handleRoot() {
    String html = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Plant Moisture Graph</title>
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <link rel="preconnect" href="https://fonts.googleapis.com">
            <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600&display=swap" rel="stylesheet">
            <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
            <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-annotation@1.1.0"></script>
            <style>
                body {
                    font-family: 'Inter', sans-serif;
                    background-color: #f4f4f8;
                    margin: 0;
                    padding: 0;
                }
                .container {
                    max-width: 800px;
                    margin: 40px auto;
                    padding: 20px;
                    background: #fff;
                    border-radius: 16px;
                    box-shadow: 0 4px 20px rgba(0,0,0,0.05);
                }
                h2 {
                    margin-top: 0;
                    font-weight: 600;
                    color: #333;
                }
                canvas {
                    border-radius: 12px;
                    box-shadow: 0 2px 8px rgba(0,0,0,0.05);
                }
            </style>
        </head>
        <body>
            <div class="container">
                <h2>Smoothed Moisture History</h2>
                <canvas id="mainChart" width="400" height="200"></canvas>
            </div>

            <div class="container">
                <h2>Recent Raw Readings</h2>
                <canvas id="miniChart" width="400" height="200"></canvas>
            </div>

            <script>
                const DRY_THRESHOLD = 2200;

                const mainCtx = document.getElementById('mainChart').getContext('2d');
                const miniCtx = document.getElementById('miniChart').getContext('2d');

                const mainChart = new Chart(mainCtx, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Smoothed Moisture',
                            data: [],
                            borderColor: 'rgba(33, 150, 243, 0.9)',
                            backgroundColor: 'rgba(33, 150, 243, 0.1)',
                            borderWidth: 2,
                            tension: 0.4,
                            fill: true
                        }]
                    },
                    options: {
                        animation: {
                            duration: 500,
                            easing: 'easeOutQuart'
                        },
                        scales: {
                            x: {
                                type: 'category',
                                ticks: { autoSkip: true, maxTicksLimit: 20, maxRotation: 45, minRotation: 0 },
                                title: { display: true, text: 'Minute' }
                            },
                            y: {
                                beginAtZero: true,
                                title: { display: true, text: 'Soil Moisture' }
                            }
                        },
                        plugins: {
                            annotation: {
                                annotations: {
                                    threshold: {
                                        type: 'line',
                                        yMin: DRY_THRESHOLD,
                                        yMax: DRY_THRESHOLD,
                                        borderColor: 'red',
                                        borderWidth: 2,
                                    }
                                }
                            },
                            legend: {
                                labels: {
                                    font: { size: 14 },
                                    color: '#444'
                                }
                            },
                            title: {
                                display: false
                            }
                        }
                    }
                });

                const miniChart = new Chart(miniCtx, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Recent Moisture',
                            data: [],
                            borderColor: 'rgba(76, 175, 80, 0.9)',
                            backgroundColor: 'rgba(76, 175, 80, 0.1)',
                            borderWidth: 1,
                            fill: true
                        }]
                    },
                    options: {
                        animation: {
                            duration: 400,
                            easing: 'easeOutQuart'
                        },
                        scales: {
                            x: {
                                type: 'category',
                                title: { display: true, text: 'Time' }
                            },
                            y: {
                                beginAtZero: true,
                                title: { display: true, text: 'Soil Moisture' }
                            }
                        },
                        plugins: {
                            annotation: {
                                annotations: {
                                    threshold: {
                                        type: 'line',
                                        yMin: DRY_THRESHOLD,
                                        yMax: DRY_THRESHOLD,
                                        borderColor: 'red',
                                        borderWidth: 2,
                                    }
                                }
                            },
                            legend: {
                                labels: {
                                    font: { size: 14 },
                                    color: '#444'
                                }
                            }
                        }
                    }
                });

                function movingAverage(data, windowSize = 5) {
                    const result = [];
                    for (let i = 0; i < data.length; i++) {
                        const start = Math.max(0, i - windowSize + 1);
                        const window = data.slice(start, i + 1);
                        const avg = window.reduce((a, b) => a + b, 0) / window.length;
                        result.push(avg);
                    }
                    return result;
                }

                async function fetchCSV() {
                    const response = await fetch('/log');
                    const text = await response.text();
                    const lines = text.trim().split("\n");

                    // Main chart: per-minute averages + smoothing
                    const minuteBuckets = {};
                    lines.forEach(line => {
                        const [timestamp, value] = line.trim().split(",");
                        const ts = parseInt(timestamp.trim()) * 1000;
                        const date = new Date(ts);
                        const minuteKey = date.getFullYear() + "-" +
                  String(date.getMonth() + 1).padStart(2, '0') + "-" +
                  String(date.getDate()).padStart(2, '0') + " " +
                  String(date.getHours()).padStart(2, '0') + ":" +
                  String(date.getMinutes()).padStart(2, '0');
                        if (!minuteBuckets[minuteKey]) minuteBuckets[minuteKey] = [];
                        minuteBuckets[minuteKey].push(parseInt(value));
                    });

                    const minuteAverages = Object.entries(minuteBuckets)
                    .filter(([_, values]) => values.length > 0) // Remove empty buckets
                    .map(([minute, values]) => ({
                    label: minute,
                     avg: values.reduce((a, b) => a + b, 0) / values.length
                     }));

                    const mainLabels = minuteAverages.map(item => item.label);
                    const smoothedData = movingAverage(minuteAverages.map(item => item.avg), 5);

                    mainChart.data.labels = mainLabels;
                    mainChart.data.datasets[0].data = smoothedData;
                    mainChart.update();

                    // Mini chart: last 20 raw entries
                    const lastLines = lines.slice(-20);
                    const miniLabels = [];
                    const miniData = [];

                    lastLines.forEach(line => {
                        const [timestamp, value] = line.trim().split(",");
                        const ts = parseInt(timestamp.trim()) * 1000;
                        const date = new Date(ts);
                        miniLabels.push(date.toLocaleTimeString());
                        miniData.push(parseInt(value));
                    });

                    miniChart.data.labels = miniLabels;
                    miniChart.data.datasets[0].data = miniData;
                    miniChart.update();
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

    // Check if SPIFFS has already been formatted
    bool isFormatted = preferences.getBool("isFormatted", false);

    if (FORCE_SPIFFS_FORMAT || !isFormatted) {
        Serial.println("Formatting SPIFFS...");
        if (SPIFFS.format()) {
            Serial.println("SPIFFS formatted successfully.");
            preferences.putBool("isFormatted", true); // Remember that we formatted
        } else {
            Serial.println("SPIFFS formatting failed!");
        }
    } else {
        Serial.println("SPIFFS already formatted. Skipping format step.");
    }

    preferences.end(); // Close preferences

    // Mount SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    Serial.println("SPIFFS mounted successfully");

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi Connected!");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize RTC with NTP
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println("Time initialized");

    // Start the web server
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

    time_t now;
    time(&now); // Get the current RTC time

    // Check if the logging interval has passed
    if (now - lastLoggedTime >= LOG_INTERVAL_SECONDS) {
        lastLoggedTime = now; // Update the last logged time

        int moisture = analogRead(SENSOR_PIN); // Read moisture sensor value
        Serial.println("Moisture check: " + String(moisture));

        logMoisture(moisture); // Log the moisture value to the CSV file

        // Send a notification if the moisture exceeds the dry threshold
        if (moisture > DRY_THRESHOLD && !notificationSent) {
            sendTelegramNotification(moisture);
            notificationSent = true;
        }

        // Reset the notification flag if the moisture is below the threshold
        if (moisture <= DRY_THRESHOLD) {
            notificationSent = false;
        }
    }
}
