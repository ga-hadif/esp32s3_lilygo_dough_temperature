#include "LV_Helper.h"
#include "ui.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "screens.h"
#include <WiFi.h> 
#include <TimeLib.h>  
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WebServer.h>

// Wifi Credentials
const char* ssid = "TECH_OFFICE";      // Change to your WiFi SSID
const char* password = "tech_4033";

// AP Mode Credentials
const char* apSSID = "ESP32_HOTSPOT";
const char* apPassword = "12345678";

WebServer server(80);
bool isAPMode = false;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 8 * 3600, 60000);  // UTC+8 timezone

// FreeRTOS Task Handles
TaskHandle_t TaskTimeHandle = NULL;
TaskHandle_t TaskSendDataHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;


// Function Prototypes
void TaskUpdateTime(void *pvParameters);
void TaskSendData(void *pvParameters);
void WiFiConnectTask(void *pvParameters);
void switchToAPMode();
void switchToSTAMode();
void handleSwitchMode();



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

void switchToAPMode() {
    Serial.println("🔄 Switching to AP Mode...");
    WiFi.disconnect(true);
    delay(1000);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    delay(1000);

    String apIP = WiFi.softAPIP().toString();
    char ipBuffer[30];
    sprintf(ipBuffer, "AP: %s", apIP.c_str());
    lv_label_set_text(objects.header_label_panel_1, ipBuffer);

    Serial.println("✅ AP Mode Started!");
    Serial.print("ESP32 AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    isAPMode = true;
}

// Function to switch back to STA Mode
void switchToSTAMode() {
    Serial.println("🔄 Switching to STA Mode...");
    WiFi.disconnect(true);
    delay(1000);
    connectToWiFi();
    isAPMode = false;
}

// Handle Web Requests
void handleSwitchMode() {
    if (server.hasArg("mode")) {
        String mode = server.arg("mode");

        if (mode == "AP") {
            server.send(200, "text/html", "<h1>Switching to AP Mode...</h1>");
            delay(1000);
            switchToAPMode();
            // ESP.restart();  
        } 
        else if (mode == "STA") {
            server.send(200, "text/html", "<h1>Switching to STA Mode...</h1>");
            delay(1000);
            switchToSTAMode();
            // ESP.restart();  
        } 
        else {
            server.send(400, "text/html", "<h1>Invalid mode. Use ?mode=AP or ?mode=STA</h1>");
        }
    } else {
        server.send(200, "text/html", "<h1>ESP32 Mode Switch</h1><p>Use /switch?mode=AP or /switch?mode=STA</p>");
    }
}


void setup()
{       
  Serial.begin(115200);
//   LCD_Init();
//   Lvgl_Init();
  lv_helper();
  ui_init();

  lv_label_set_text(objects.ui_wifi_status, "Wi-Fi: Connecting...");
  connectToWiFi(); 

  // Web Server Routes
  server.on("/", []() {
    server.send(200, "text/html", "<h1>ESP32 Web Control</h1><p>Use /switch?mode=AP or /switch?mode=STA to change modes</p>");
  });
  server.on("/switch", handleSwitchMode);
  server.begin();

  timeClient.begin();  // Start NTP time sync
  // dht.begin();  
  
  // Set initial label values
  lv_label_set_text(objects.ui_test_datetime, "Time: --:--:--");
  lv_label_set_text(objects.ui_temp, "--°C");  
  lv_label_set_text(objects.ui_upload_status, LV_SYMBOL_UPLOAD);

  


  // Create FreeRTOS tasks
  xTaskCreatePinnedToCore(TaskUpdateTime, "UpdateTime", 8192, NULL, 1, &TaskTimeHandle, 1);
  xTaskCreatePinnedToCore(TaskSendData, "SendData", 8192, NULL, 1, &TaskSendDataHandle, 1);
  xTaskCreatePinnedToCore(
    WiFiConnectTask,
    "WiFiTask",
    4096,
    NULL,
    1,
    &wifiTaskHandle,
    0  // Core 0
);
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

void WiFiConnectTask(void *pvParameters) {
    const TickType_t retryInterval = pdMS_TO_TICKS(10000); // 10 seconds
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (true) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("📡 Connecting to Wi-Fi...");

            WiFi.disconnect(true); // Reset Wi-Fi
            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid, password);

            int waitTime = 0;
            while (WiFi.status() != WL_CONNECTED && waitTime < 10000) {
                vTaskDelay(pdMS_TO_TICKS(500)); // Poll every 500ms for 10s
                waitTime += 500;
            }

            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("✅ Wi-Fi Connected!");
                Serial.println(WiFi.localIP());
                char ipBuffer[20];
                lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WIFI);
                sprintf(ipBuffer, "STA: %s", WiFi.localIP().toString().c_str());
                lv_label_set_text(objects.header_label_panel_1, ipBuffer);
            } else {
                Serial.println("❌ Wi-Fi connect failed. Retrying in 10s...");
                lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_CLOSE);
            }
        }

        // Wait 10s before retrying or re-checking
        vTaskDelayUntil(&lastWakeTime, retryInterval);
    }
}


void loop()
{
  server.handleClient();
  lv_task_handler();
  // Timer_Loop();
  // delay(5);

  // timeClient.update();  // Sync time from NTP

  // time_t rawTime = timeClient.getEpochTime();
  // struct tm *timeInfo = localtime(&rawTime);

  // char datetimeStr[20];
  // sprintf(datetimeStr, "%04d-%02d-%02d  %02d:%02d", 
  //         timeInfo->tm_year + 1900, timeInfo->tm_mon + 1, timeInfo->tm_mday,
  //         timeInfo->tm_hour, timeInfo->tm_min);

  // // Format time as HH:MM:SS
  // String formattedTime = timeClient.getFormattedTime();
  // // lv_label_set_text(objects.ui_test_datetime, formattedTime.c_str());
  // lv_label_set_text(objects.ui_test_datetime, datetimeStr);

  // lv_task_handler(); 
}
