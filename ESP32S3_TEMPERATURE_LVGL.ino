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

String ssid;
String password;

// Function Prototypes
void TaskUpdateTime(void *pvParameters);
void TaskSendData(void *pvParameters);
void WiFiConnectTask(void *pvParameters);
void switchToAPSTAMode();
void switchToSTAMode();
// void handleSwitchMode();


void switchToAPSTAMode() {
    Serial.println("🔄 Switching to AP Mode...");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID, apPassword,1);
    delay(1000);

    String apIP = WiFi.softAPIP().toString();
    char ipBuffer[30];
    sprintf(ipBuffer, "AP: %s", apIP.c_str());
    lv_label_set_text(objects.header_label_panel_1, ipBuffer);

    Serial.println("✅ AP Mode Started!");
    Serial.print("ESP32 AP IP Address: ");
    Serial.println(WiFi.softAPIP());
}

// Function to switch back to STA Mode
void switchToSTAMode() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    
    if (ssid == "" || password == "") {
        return;
    } else {
        WiFi.begin(ssid.c_str(), password.c_str());
        while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
        }

        Serial.println(WiFi.localIP());
        char ipBuffer[20];

        if (objects.ui_wifi_status) {
            lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WIFI);
        }
        sprintf(ipBuffer, "STA: %s", WiFi.localIP().toString().c_str());
        lv_label_set_text(objects.header_label_panel_1, ipBuffer);

    }
}

void initWebServer() {
    // Register all AsyncWebServer routes
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

        Preferences prefs;
        if (!prefs.begin("wifi", false)) {  // false = read/write
            Serial.println("❌ Failed to open preferences (read/write)");
            request->send(500, "text/plain", "Failed to save WiFi credentials.");
            return;
        }
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
        prefs.end();

        request->send(200, "text/html", "<h2>Connecting...<br>Check screen or Serial Monitor.</h2>");
        // switchToSTAMode(); 
    });

        server.on("/wifi_config", HTTP_GET, [](AsyncWebServerRequest *request) {
            Preferences prefs;
            String savedSSID = "";
            String savedPass = "";

            if (prefs.begin("wifi", true)) {
                savedSSID = prefs.getString("ssid", "");
                savedPass = prefs.getString("pass", "");
                prefs.end();
            } else {
                Serial.println("❌ Failed to open preferences namespace!");
                request->send(500, "application/json", "{\"error\":\"prefs_failed\"}");
                return;
            }

            String json = "{\"ssid\":\"" + savedSSID + "\",\"pass\":\"" + savedPass + "\"}";
            request->send(200, "application/json", json);
        });


    server.begin();
}



void setup()
{       
    Serial.begin(115200);
    delay(100);

    lv_helper();
    ui_init();
    scanMutex = xSemaphoreCreateMutex();

    prefs.begin("wifi", false);
    ssid = prefs.getString("ssid", ""); 
    password = prefs.getString("pass", "");
    prefs.end();


    if (ssid == "" || password == "") {
        switchToAPSTAMode();
        lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WARNING);
    } else {
        switchToSTAMode();
        timeClient.begin();  // Start NTP time sync
        // dht.begin();  
    }

    initWebServer();



    // Set initial label values
    lv_label_set_text(objects.ui_test_datetime, "Time: --:--:--");
    lv_label_set_text(objects.ui_temp, "--°C");  
    lv_label_set_text(objects.ui_upload_status, LV_SYMBOL_UPLOAD);


    // Create FreeRTOS tasks
    xTaskCreatePinnedToCore(TaskUpdateTime, "UpdateTime", 8192, NULL, 1, &TaskTimeHandle, 1);
    xTaskCreatePinnedToCore(TaskSendData, "SendData", 8192, NULL, 1, &TaskSendDataHandle, 1);
    xTaskCreatePinnedToCore(WiFiConnectTask, "WiFiTask", 8192, NULL, 1, &wifiTaskHandle, 0);
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
  

    if (ssid == "" || password == "") {
        vTaskDelete(NULL);
        return;
    }else {
        WiFi.begin(ssid.c_str(), password.c_str());
        while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Serial.println(WiFi.localIP());
        // char ipBuffer[20];

        // if (objects.ui_wifi_status) {
        //     lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WIFI);
        // }
        // sprintf(ipBuffer, "STA: %s", WiFi.localIP().toString().c_str());
        // lv_label_set_text(objects.header_label_panel_1, ipBuffer);

    }

    // int retry = 0;
    // while (WiFi.status() != WL_CONNECTED && retry < 20) {
    //     vTaskDelay(pdMS_TO_TICKS(500));
    //     Serial.print(".");
    //     retry++;
    // }

    // if (WiFi.status() == WL_CONNECTED) {
    //     // Serial.println("\n✅ Connected to Wi-Fi!");
    //     // Serial.println(WiFi.localIP());
    //     // char ipBuffer[20];

    //     // if (objects.ui_wifi_status) {
    //     //     lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WIFI);
    //     // }
    //     // sprintf(ipBuffer, "STA: %s", WiFi.localIP().toString().c_str());
    //     // lv_label_set_text(objects.header_label_panel_1, ipBuffer);
    // } else {
    //     // Serial.println("\n❌ Connection failed.");
    //     // if (objects.ui_wifi_status) {
    //     //     lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WARNING);
    //     // }
    // }

    vTaskDelete(NULL);
}


void loop()
{
  lv_task_handler();
}
