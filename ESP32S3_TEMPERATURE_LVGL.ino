#include "LV_Helper.h"
#include "ui.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "screens.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h> 
#include <TimeLib.h>  
#include <Adafruit_Sensor.h>
#include "index.h"
#include <DHT.h>
#include <Preferences.h>
#include <vector>
#include <set>

// Wifi Credentials
const char* ssid = "TECH_OFFICE";      // Change to your WiFi SSID
const char* password = "tech_4033";

// AP Mode Credentials
const char* apSSID = "ESP32S3_HOTSPOT";
const char* apPassword = "12345678";

AsyncWebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 8 * 3600, 60000);  //

// FreeRTOS Task Handles
TaskHandle_t TaskTimeHandle = NULL;
TaskHandle_t TaskSendDataHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;

std::vector<String> scannedSSIDs;
bool scanInProgress = false;
SemaphoreHandle_t scanMutex;

Preferences prefs;

// Function Prototypes
void TaskUpdateTime(void *pvParameters);
void TaskSendData(void *pvParameters);
void WiFiConnectTask(void *pvParameters);
void switchToAPMode();
void switchToSTAMode();
// void handleSwitchMode();



void connectToWiFi() {
    Serial.print("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);

    int maxRetries = 20;  // Max attempts before timeout
    while (WiFi.status() != WL_CONNECTED && maxRetries > 0) {
        delay(500);
        lv_label_set_text(objects.ui_wifi_status, "Connecting...");  // Update UI
        lv_task_handler();  // Refresh UI
        maxRetries--;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ Wi-Fi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        char ipBuffer[20]; 

        // lv_label_set_text(objects.ui_label_wifi, "Wi-Fi: Connected ✅");
        lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WIFI);
        sprintf(ipBuffer, "STA: %s", WiFi.localIP().toString().c_str());
        lv_label_set_text(objects.header_label_panel_1, ipBuffer);
    } else {
        Serial.println("\n❌ Wi-Fi Connection Failed!");
        lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_CLOSE );
    }
}

void setup()
{       
    Serial.begin(115200);

    lv_helper();
    ui_init();

    lv_label_set_text(objects.ui_wifi_status, "Wi-Fi: Connecting...");

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID, apPassword,1); 
    scanMutex = xSemaphoreCreateMutex();
    delay(1000);
    
    // AsyncWebServer Routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", indexHtml);
    });

    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (scanInProgress) {
        request->send(200, "application/json", "{\"status\":\"scanning\",\"ssids\":[]}");
        return;
    }

    xTaskCreatePinnedToCore(TaskScanWifi, "WifiScan", 4096, NULL, 1, NULL, 0);
    request->send(200, "application/json", "{\"status\":\"scanning\",\"ssids\":[]}");
    });

    server.on("/scanResult", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (scanInProgress) {
        request->send(200, "application/json", "{\"status\":\"scanning\",\"ssids\":[]}");
        return;
    }

    String json = "{\"status\":\"done\",\"ssids\":[";
    xSemaphoreTake(scanMutex, portMAX_DELAY);
    std::set<String> uniqueSSIDs;
    bool first = true;
    for (const auto& ssid : scannedSSIDs) {
        if (uniqueSSIDs.insert(ssid).second) {
            if (!first) json += ",";
            json += "\"" + ssid + "\"";
            first = false;
        }
    }
    xSemaphoreGive(scanMutex);
    json += "]}";

    request->send(200, "application/json", json);
});

    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    String ssid, pass;

    if (request->hasParam("ssid", true)) {
        ssid = request->getParam("ssid", true)->value();
    }
    if (request->hasParam("password", true)) {
        pass = request->getParam("password", true)->value();
    }

    Serial.println("📥 Received WiFi creds:");
    Serial.println("SSID: " + ssid);
    Serial.println("PASS: " + pass);

    // Save to preferences (non-volatile storage)
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();

    // Start connection
    xTaskCreatePinnedToCore(WiFiConnectTask, "WiFiTask", 4096, NULL, 1, &wifiTaskHandle, 0);

    // Send simple response
    request->send(200, "text/html", "<h2>Connecting...<br>Check screen or Serial Monitor.</h2>");
    });

    server.begin();

    // timeClient.begin();  // Start NTP time sync
    // dht.begin();  

    // Set initial label values
    lv_label_set_text(objects.ui_test_datetime, "Time: --:--:--");
    lv_label_set_text(objects.ui_temp, "--°C");  
    lv_label_set_text(objects.ui_upload_status, LV_SYMBOL_UPLOAD);


    // Create FreeRTOS tasks
    // xTaskCreatePinnedToCore(TaskUpdateTime, "UpdateTime", 8192, NULL, 1, &TaskTimeHandle, 1);
    xTaskCreatePinnedToCore(TaskSendData, "SendData", 8192, NULL, 1, &TaskSendDataHandle, 1);
}

void TaskUpdateTime(void *pvParameters) {
    while (1) {
        timeClient.update();  // Sync time from NTP

        // float t = dht.readTemperature();

        // if (isnan(t)) {
        //   Serial.println(F("Failed to read from DHT sensor!"));
        //   lv_label_set_text(objects.ui_temp, "--°C");
        // } else {
        //   char tempStr[15];
        //   sprintf(tempStr, "%.1f°C", t);
        //   lv_label_set_text(objects.ui_temp, tempStr);     
        // }

        time_t rawTime = timeClient.getEpochTime();
        struct tm *timeInfo = localtime(&rawTime);

        char datetimeStr[20];
        sprintf(datetimeStr, "%04d-%02d-%02d  %02d:%02d", 
                timeInfo->tm_year + 1900, timeInfo->tm_mon + 1, timeInfo->tm_mday,
                timeInfo->tm_hour, timeInfo->tm_min);

        // Format time as HH:MM:SS
        // String formattedTime = timeClient.getFormattedTime();
        // lv_label_set_text(objects.ui_test_datetime, formattedTime.c_str());
        lv_label_set_text(objects.ui_test_datetime, datetimeStr);

        lv_task_handler(); 
        vTaskDelay(pdMS_TO_TICKS(2000));  // Wait 5 seconds before next send
    }
}

void TaskSendData(void *pvParameters) {
    while (1) {
        lv_label_set_text(objects.ui_upload_status, LV_SYMBOL_UPLOAD);
        lv_task_handler();

        vTaskDelay(pdMS_TO_TICKS(3000));  // Simulate sending delay

        lv_label_set_text(objects.ui_upload_status, LV_SYMBOL_OK);

        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds before next send
    }
}

void TaskScanWifi(void *pvParameters) {
    scanInProgress = true;

    Serial.println("🔧 Starting Wi-Fi scan");

    // WiFi.disconnect(false, true);  // Disconnect STA, preserve AP
    delay(200);                    // Let things stabilize
    WiFi.scanDelete();             // Clear leftovers

    xSemaphoreTake(scanMutex, portMAX_DELAY);
    scannedSSIDs.clear();
    xSemaphoreGive(scanMutex);

    Serial.println("📡 Scanning...");
    int n = WiFi.scanNetworks();

    if (n > 0) {
        xSemaphoreTake(scanMutex, portMAX_DELAY);
        for (int i = 0; i < n; ++i) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 0) {
                scannedSSIDs.push_back(ssid);
                Serial.println(" 📶 " + ssid);
            }
        }
        xSemaphoreGive(scanMutex);
    } else {
        Serial.println("⚠️ No networks found or scan failed");
    }

    scanInProgress = false;

    vTaskDelete(NULL);
}

void WiFiConnectTask(void *pvParameters) {
    prefs.begin("wifi", true); // read-only
    String savedSSID = prefs.getString("ssid", "");
    String savedPass = prefs.getString("pass", "");
    prefs.end();

    if (savedSSID == "") {
        Serial.println("❌ No saved credentials");
        vTaskDelete(NULL);
        return;
    }

    Serial.println("📡 Connecting to: " + savedSSID);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ Connected to Wi-Fi!");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n❌ Connection failed.");
    }

    vTaskDelete(NULL);
}


void loop()
{
  lv_task_handler();
}
