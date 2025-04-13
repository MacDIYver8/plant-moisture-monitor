#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <time.h>
#include <secrets.h>
#include <Preferences.h>

#define PRODUCTION_MODE 1

#if PRODUCTION_MODE
  #define LOG_INTERVAL_SECONDS 600     // 10 minutes
  #define MAX_LOG_ENTRIES 500
#else
  #define LOG_INTERVAL_SECONDS 5       // dev mode
  #define MAX_LOG_ENTRIES 500
#endif

#define SENSOR_PIN_1 34            // Moisture sensor 1 pin
#define SENSOR_PIN_2 35            // Moisture sensor 2 pin
#define FORCE_SPIFFS_FORMAT true   // Set to 'false' when you don't want to reformat
const int DRY_THRESHOLD = 2200;    // Adjust this for your plant

WebServer server(80);

unsigned long lastCheckTime = 0;
bool notificationSent1 = false;
bool notificationSent2 = false;
time_t lastLoggedTime = 0;

// Log moisture to CSV with circular buffer behavior (RTC time instead of elapsed seconds)
void logMoisture(int sensor, int moisture) {
    time_t now;
    time(&now); // Get current RTC time as Unix timestamp

    String filename = (sensor == 1) ? "/data1.csv" : "/data2.csv";

    // Read existing lines
    std::vector<String> lines;
    File file = SPIFFS.open(filename, FILE_READ);
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
    file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    for (const auto& line : lines) {
        file.printf("%s\r\n", line.c_str());
    }
    file.close();

    String sensorName = (sensor == 1) ? "Alfons" : "Milla";
    Serial.println("Logged: " + String(now) + "," + String(moisture) + " to " + sensorName + "'s file");
}

// Send Telegram notification
void sendTelegramNotification(int sensor, int moisture) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String sensorName = (sensor == 1) ? "Alfons" : "Milla";
        String url = "https://api.telegram.org/bot" + String(telegramBotToken) + 
                    "/sendMessage?chat_id=" + String(telegramChatID) + 
                    "&text=\U0001F335 " + sensorName + " is too dry! Moisture: " + 
                    String(moisture);
        http.begin(url);
        int httpResponseCode = http.GET();
        http.end();
        if (httpResponseCode > 0) {
            Serial.println("Telegram notification sent for " + sensorName);
        } else {
            Serial.println("Failed to send Telegram message for " + sensorName);
        }
    }
}

void handleRoot() {
    String html = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Plant Moisture Graphs</title>
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
                <h2>Alfons' Moisture History</h2>
                <canvas id="mainChart1" width="400" height="200"></canvas>
            </div>

            <div class="container">
                <h2>Alfons' Recent Moisture Readings</h2>
                <canvas id="miniChart1" width="400" height="200"></canvas>
            </div>

            <div class="container">
                <h2>Milla's Moisture History</h2>
                <canvas id="mainChart2" width="400" height="200"></canvas>
            </div>

            <div class="container">
                <h2>Milla's Recent Moisture Readings</h2>
                <canvas id="miniChart2" width="400" height="200"></canvas>
            </div>

            <script>
                const PRODUCTION_MODE = %PRODUCTION_MODE%; // This will be replaced
                const BUCKET_MINUTES = PRODUCTION_MODE ? 30 : 1;

                const DRY_THRESHOLD = 2200;

                const mainCtx1 = document.getElementById('mainChart1').getContext('2d');
                const miniCtx1 = document.getElementById('miniChart1').getContext('2d');
                const mainCtx2 = document.getElementById('mainChart2').getContext('2d');
                const miniCtx2 = document.getElementById('miniChart2').getContext('2d');

                const mainChart1 = new Chart(mainCtx1, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Moisture Level',
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
                            },
                            title: {
                                display: false
                            }
                        }
                    }
                });

                const miniChart1 = new Chart(miniCtx1, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Moisture Level',
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

                const mainChart2 = new Chart(mainCtx2, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Moisture Level',
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
                            },
                            title: {
                                display: false
                            }
                        }
                    }
                });

                const miniChart2 = new Chart(miniCtx2, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Moisture Level',
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

                async function fetchCSV(sensor) {
                    const response = await fetch(`/log?sensor=${sensor}`);
                    const text = await response.text();
                    const lines = text.trim().split("\n");

                    // Main chart: per-minute averages + smoothing
                    const minuteBuckets = {};
                    lines.forEach(line => {
                        const [timestamp, value] = line.trim().split(",");
                        const ts = parseInt(timestamp.trim()) * 1000;
                        const date = new Date(ts);
                        const roundedMinutes = Math.floor(date.getMinutes() / BUCKET_MINUTES) * BUCKET_MINUTES;
                        const minuteKey = date.getFullYear() + "-" +
                    String(date.getMonth() + 1).padStart(2, '0') + "-" +
                    String(date.getDate()).padStart(2, '0') + " " +
                    String(date.getHours()).padStart(2, '0') + ":" +
                    String(roundedMinutes).padStart(2, '0');

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
                    const bucketedData = minuteAverages.map(item => item.avg);

                    if (sensor === 1) {
                        mainChart1.data.labels = mainLabels;
                        mainChart1.data.datasets[0].data = bucketedData;  // Changed from smoothedData
                        mainChart1.update();

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

                        miniChart1.data.labels = miniLabels;
                        miniChart1.data.datasets[0].data = miniData;
                        miniChart1.update();
                    } else {
                        mainChart2.data.labels = mainLabels;
                        mainChart2.data.datasets[0].data = bucketedData;  // Changed from smoothedData
                        mainChart2.update();

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

                        miniChart2.data.labels = miniLabels;
                        miniChart2.data.datasets[0].data = miniData;
                        miniChart2.update();
                    }
                }

                setInterval(() => fetchCSV(1), 5000);
                setInterval(() => fetchCSV(2), 5000);
            </script>
        </body>
        </html>
    )rawliteral";

    // Replace the placeholder with the actual production mode value
    html.replace("%PRODUCTION_MODE%", PRODUCTION_MODE ? "true" : "false");

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
        String sensor = server.arg("sensor");
        String filename = (sensor == "1") ? "/data1.csv" : "/data2.csv";
        File file = SPIFFS.open(filename, FILE_READ);
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

        int moisture1 = analogRead(SENSOR_PIN_1); // Read moisture sensor 1 value
        int moisture2 = analogRead(SENSOR_PIN_2); // Read moisture sensor 2 value

        Serial.println("Moisture check Sensor 1: " + String(moisture1));
        Serial.println("Moisture check Sensor 2: " + String(moisture2));

        logMoisture(1, moisture1); // Log the moisture value for sensor 1
        logMoisture(2, moisture2); // Log the moisture value for sensor 2

        // Send a notification if the moisture exceeds the dry threshold
        if (moisture1 > DRY_THRESHOLD && !notificationSent1) {
            sendTelegramNotification(1, moisture1);
            notificationSent1 = true;
        }
        if (moisture2 > DRY_THRESHOLD && !notificationSent2) {
            sendTelegramNotification(2, moisture2);
            notificationSent2 = true;
        }

        // Reset the notification flag if the moisture is below the threshold
        if (moisture1 <= DRY_THRESHOLD) {
            notificationSent1 = false;
        }
        if (moisture2 <= DRY_THRESHOLD) {
            notificationSent2 = false;
        }
    }
}
