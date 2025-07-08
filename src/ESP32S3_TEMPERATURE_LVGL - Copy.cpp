#define ENABLE_USER_AUTH
#define ENABLE_FIRESTORE
#define ENABLE_PSRAM 
#define ENABLE_FS
#define ENABLE_ESP_SSLCLIENT
#define ENABLE_SERVICE_AUTH

#include <FirebaseClient.h>
#include "firebase_config.h"
#include <Arduino.h>
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
#include <Adafruit_MAX31865.h>
#include <Preferences.h>
#include "LV_Helper.h"
#include <vector>
#include <set>
#include <LittleFS.h>
#include "fa_icon.c"
#include "temp_icon.c"
#include "sent_icon.c"
#include "firestore_response_icon.c"

LV_FONT_DECLARE(fa_icon);
LV_FONT_DECLARE(temp_icon);
LV_FONT_DECLARE(firestore_response_icon);

#define FIREBASE_PROJECT_ID "schpi-productionsys-cp2ver1"
#define MY_FIRE_ICON "\xEF\x81\xAD"
#define MY_TEMP_ICON "\xEF\x8B\x8B"
#define BIG_TEMP_ICON "\xEF\x8B\x8B"
#define MY_CHECK_ICON "\xEF\x81\x98"
#define MY_CROSS_ICON "\xEF\x81\x97"

String creds_file = "/serviceAccountKey.json";
FileConfig sa_file(creds_file.c_str(), file_operation_callback);
ServiceAuth sa_auth(getFile(sa_file));

FirebaseApp app;

// SSL got conflicted library so use "FirebaseAsyncClient" instead of "AsyncClient"
SSL_CLIENT ssl_client;
using FirebaseAsyncClient = AsyncClientClass;
FirebaseAsyncClient aClient(ssl_client);

// Initiate doc and result for firebase response?
Firestore::Documents Docs;
AsyncResult firestoreResult;

bool sent = false;

// AP Mode Credentials
const char* apSSID = "ESP32S3_HOTSPOT";
const char* apPassword = "12345678";

// Web Server for Index HTML
AsyncWebServer server(80);
WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "pool.ntp.org", 8 * 3600, 60000);  
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 0, 60000);  

Adafruit_MAX31865 thermo = Adafruit_MAX31865(10);
#define RREF      430.0
#define RNOMINAL  100.0

// FreeRTOS Task Handles
TaskHandle_t TaskTimeHandle = NULL;
// TaskHandle_t TaskSendDataHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t TaskWriteToFirestoreHandle = NULL;
TaskHandle_t TaskResendOfflineLogsHandle = NULL;

std::vector<String> scannedSSIDs;
bool scanInProgress = false;
SemaphoreHandle_t scanMutex; // to limit access for scannedSSIDs variable

// Mutex for temperature
float latestTemperature = NAN;
SemaphoreHandle_t tempMutex;

//LVGL Mutex
SemaphoreHandle_t lvglMutex;

// Preferences
Preferences prefs;
String ssid;
String password;

struct FirestoreResult {
  bool success;
  String message;
};

QueueHandle_t firestoreResultQueue;

int retry=0;
bool writeInProgress = false;

volatile bool buttonPressed = false;

// Function Prototypes
void TaskScanWifi(void *pvParameters);
void TaskUpdateTime(void *pvParameters);
void TaskWriteToFirestore(void *pvParameters);
void TaskReadTemp(void *pvParameters);
void TaskResendOfflineLogs(void *pvParameters);
void TaskHandleFirestoreResult(void *pvParameters);
void TaskLVGL(void *pvParameters);
void switchToAPSTAMode();
void switchToSTAMode();

// Callback for Firebase Response
void onFirestoreWriteDone(AsyncResult &aResult)
{
    FirestoreResult result;

    if (!aResult.isResult()){
        result.success = false;
        result.message = "No Result";
        return;
    } else  if (aResult.isEvent()){
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
        result.success = false;
        result.message = "No Result";
    } else if (aResult.isDebug()){
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
        result.success = false;
        result.message = "No Result";
    } else if (aResult.isError()){
       // Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
       result.success = false;
       result.message = "Firestore Error";
    } else if (aResult.available()){
        // Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
        result.success = true;
        result.message = "Data recorded";      
    }

    xQueueSend(firestoreResultQueue, &result, 0);
}


// void onFirestoreWriteDone(AsyncResult &aResult) {
//         Serial.printf("Callback Running on core: %d\n", xPortGetCoreID());

//         if (!aResult.isResult()){

//             xSemaphoreTake(lvglMutex, portMAX_DELAY);

//             // Hide spinner
//             lv_obj_add_flag(objects.ui_upload_spinner_1, LV_OBJ_FLAG_HIDDEN);

//             // Unhide firestore status icon 
//             lv_obj_clear_flag(objects.ui_sent_1, LV_OBJ_FLAG_HIDDEN);
//             lv_obj_set_style_text_font(objects.ui_sent_1, &firestore_response_icon, LV_PART_MAIN);
//             lv_label_set_text(objects.ui_sent_1, MY_CROSS_ICON);

//             // Color the cross RED
//             lv_obj_set_style_text_color(objects.ui_sent_1, lv_color_hex(0xFF0000), LV_PART_MAIN);

//             // Update status label text 
//             lv_label_set_text(objects.ui_sent_status, "No Result Somehow");

//             // Delay screen switch using lv_timer
//             lv_timer_create([](lv_timer_t *timer) {
//                 // Switch screen in safe LVGL context
//                 lv_obj_t *current = lv_scr_act(); 
//                 if (current != objects.main) { 
//                     lv_scr_load(objects.main);  
//                     lv_obj_clean(current);     
//                     lv_obj_del(current);  
//                     objects.send_data = nullptr;  
//                     // create_screen_send_data();  
//                 }
//                 lv_timer_del(timer); // Clean up timer
//             }, 2000, NULL); // 2

            
//             xSemaphoreGive(lvglMutex);
//             return;
//         }

//         if (aResult.isEvent())
//         {
//             Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
//         }

//         if (aResult.isDebug())
//         {
//             Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
//         }
//         if (aResult.isError()) {
//             Serial.printf("❌ Firestore error: %s\n", aResult.error().message().c_str());

//             xSemaphoreTake(lvglMutex, portMAX_DELAY);

//             // Hide spinner
//             lv_obj_add_flag(objects.ui_upload_spinner_1, LV_OBJ_FLAG_HIDDEN);

//             // Unhide firestore status icon 
//             lv_obj_clear_flag(objects.ui_sent_1, LV_OBJ_FLAG_HIDDEN);
//             lv_obj_set_style_text_font(objects.ui_sent_1, &firestore_response_icon, LV_PART_MAIN);
//             lv_label_set_text(objects.ui_sent_1, MY_CROSS_ICON);

//             // Color the cross RED
//             lv_obj_set_style_text_color(objects.ui_sent_1, lv_color_hex(0xFF0000), LV_PART_MAIN);

//             // Update status label text 
//             lv_label_set_text(objects.ui_sent_status, "Firestore Error");

//             // Delay screen switch using lv_timer
//             lv_timer_create([](lv_timer_t *timer) {
//                 // Switch screen in safe LVGL context
//                 lv_obj_t *current = lv_scr_act(); 
//                 if (current != objects.main) { 
//                     lv_scr_load(objects.main);  
//                     lv_obj_clean(current);     
//                     lv_obj_del(current);  
//                     objects.send_data = nullptr;  
//                     // create_screen_send_data();  
//                 }
//                 lv_timer_del(timer); // Clean up timer
//             }, 2000, NULL); // 2

            
//              xSemaphoreGive(lvglMutex);
//         }

//         if (aResult.available()) {

//             xSemaphoreTake(lvglMutex, portMAX_DELAY);

//             lv_obj_add_flag(objects.ui_upload_spinner_1, LV_OBJ_FLAG_HIDDEN);

//             lv_obj_clear_flag(objects.ui_sent_1, LV_OBJ_FLAG_HIDDEN);
//             lv_obj_set_style_text_font(objects.ui_sent_1, &firestore_response_icon, LV_PART_MAIN);
//             lv_label_set_text(objects.ui_sent_1, MY_CHECK_ICON);

//             lv_label_set_text(objects.ui_sent_status, "Data recorded");
//             lv_obj_set_style_text_color(objects.ui_sent_status, lv_color_hex(0x2cff2c), LV_PART_MAIN);
//             //#35b977

//             // Delay screen switch using lv_timer
//             lv_timer_create([](lv_timer_t *timer) {
//                 // Switch screen in safe LVGL context
//                 lv_obj_t *current = lv_scr_act(); 
//                 if (current != objects.main) { 
//                     lv_scr_load(objects.main);  
//                     lv_obj_clean(current);     
//                     lv_obj_del(current);  
//                     objects.send_data = nullptr;  
//                     // create_screen_send_data();  
//                 }
//                 lv_timer_del(timer); // Clean up timer
//             }, 2000, NULL); // 2

//              xSemaphoreGive(lvglMutex);


//             Serial.println("✅ Doc written successfully");
//             Serial.println(String("Response: ") + aResult.c_str()); 

//         }

// }

void onFirestoreSyncLogs(AsyncResult &aResult) {
        Serial.printf("Callback Running on core: %d\n", xPortGetCoreID());
        if (aResult.isError()) {
            Serial.printf("❌ Firestore error: %s\n", aResult.error().message().c_str());
            return;
        }

        if (aResult.available()) {
            Serial.println("Offline Sync Successful");
            Serial.println(String("Response: ") + aResult.c_str()); 
        }
}

void switchToAPSTAMode() {
    Serial.println("🔄 Switching to AP Mode...");

    // WiFi.disconnect(true);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID, apPassword,1);

    String apIP = WiFi.softAPIP().toString();
    char ipBuffer[30];
    sprintf(ipBuffer, "%s", apIP.c_str());
    lv_label_set_text(objects.ui_ip, ipBuffer);

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
        }

        Serial.println(WiFi.localIP());
        char ipBuffer[20];

        if (objects.ui_wifi_status) {
            lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WIFI);
            // lv_label_set_text(objects.ui_wifi_status, MY_FIRE_ICON);
        }
        sprintf(ipBuffer, "%s", WiFi.localIP().toString().c_str());
        lv_label_set_text(objects.ui_ip, ipBuffer);

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
    lv_helper();
    ui_init();
    pinMode(14, INPUT_PULLUP);  // IO14 = active low
    scanMutex = xSemaphoreCreateMutex();
    tempMutex = xSemaphoreCreateMutex();
    lvglMutex = xSemaphoreCreateMutex();
    firestoreResultQueue = xQueueCreate(5, sizeof(FirestoreResult));

    prefs.begin("wifi", false);
    ssid = prefs.getString("ssid", ""); 
    password = prefs.getString("pass", "");
    prefs.end();

    #if defined(ENABLE_FS)
    MY_FS.begin();
    #endif


    if (ssid == "" || password == "") {
        switchToAPSTAMode();
        lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WARNING);
    } else {
        switchToSTAMode();

        timeClient.begin();  // Start NTP time sync
        timeClient.update();

        set_ssl_client_insecure_and_buffer(ssl_client);

        time_t epoch = timeClient.getEpochTime();
        setTime(epoch);
        app.setTime(epoch); //Error code:-119 time was not set or valid

        // initializeApp(aClient, app, getAuth(user_auth), auth_debug_print, "🔐 authTask");
  
        initializeApp(aClient, app, getAuth(sa_auth), auth_debug_print, "🔐 authTask");
        app.getApp<Firestore::Documents>(Docs);

    }


    initWebServer();

    // Set initial label values
    lv_label_set_text(objects.ui_datetime, "Time: --:--:--");
    lv_label_set_text(objects.ui_temp, "--°C");  

    lv_obj_set_style_text_font(objects.ui_temp_icon, &temp_icon, LV_PART_MAIN);
    lv_label_set_text(objects.ui_temp_icon, BIG_TEMP_ICON);
    // lv_label_set_text(objects.ui_firebase_status, LV_SYMBOL_UPLOAD);

    lv_obj_set_style_text_font(objects.ui_firebase_status, &fa_icon, LV_PART_MAIN);
    lv_label_set_text(objects.ui_firebase_status, MY_FIRE_ICON);
    lv_obj_set_style_text_color(objects.ui_firebase_status, lv_color_hex(0x808080), LV_PART_MAIN);

    thermo.begin(MAX31865_3WIRE); 

    // Create FreeRTOS tasks
    xTaskCreatePinnedToCore(TaskUpdateTime, "UpdateTime", 4096, NULL, 1, &TaskTimeHandle, 1);
    xTaskCreatePinnedToCore(TaskReadTemp, "ReadTemp", 2304, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskLVGL, "LVGL Task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(TaskHandleFirestoreResult, "HandleFirestore", 4096, NULL, 1, NULL, 1);
}

void TaskUpdateTime(void *pvParameters) {
    while (1) {
        timeClient.update();  // Sync time from NTP

        // time_t rawTime = timeClient.getEpochTime();
        time_t rawTime = timeClient.getEpochTime() + 8 * 3600;
        struct tm *timeInfo = localtime(&rawTime);

        char datetimeStr[20];
        sprintf(datetimeStr, "%04d-%02d-%02d  %02d:%02d", 
                timeInfo->tm_year + 1900, timeInfo->tm_mon + 1, timeInfo->tm_mday,
                timeInfo->tm_hour, timeInfo->tm_min);

        // Format time as HH:MM:SS
        // String formattedTime = timeClient.getFormattedTime();
        // lv_label_set_text(objects.ui_test_datetime, formattedTime.c_str());
        if (xSemaphoreTake(lvglMutex, portMAX_DELAY)) {
            lv_label_set_text(objects.ui_datetime, datetimeStr);
            xSemaphoreGive(lvglMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(30000));  // Wait 5 seconds before next send
    }
}

void TaskScanWifi(void *pvParameters) {
    scanInProgress = true;

    Serial.println("🔧 Starting Wi-Fi scan");
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

void logTemperatureToCSV(const char *docId, const String &timestampStr, float temp) {
    char filename[32];
    snprintf(filename, sizeof(filename), "/%s.csv", docId);  // e.g., /2025-06-23.csv

    File logFile = MY_FS.open(filename, FILE_APPEND);
    if (!logFile) {
        Serial.printf("Failed to open %s for writing\n", filename);
        return;
    }

    logFile.printf("%s,%.2f\n", timestampStr.c_str(), temp);
    logFile.close();

    Serial.printf("📄 Logged to %s: %s, %.2f\n", filename, timestampStr.c_str(), temp);
}

void TaskWriteToFirestore(void *pvParameters) {
    Serial.printf("📍 Firestore Running on core: %d\n", xPortGetCoreID());

    if(timeNotSet){
        Serial.println("❌ Skipping Firestore write: System time not synced");
        vTaskDelete(NULL);
        return;
    }

    float tempToWrite = NAN;
    if (xSemaphoreTake(tempMutex, pdMS_TO_TICKS(100))) {
        tempToWrite = latestTemperature;
        xSemaphoreGive(tempMutex);
    }

    if (isnan(tempToWrite)) {
        Serial.println("❌ Temperature not ready, skipping write");
        vTaskDelete(NULL);
        return;
    }

    // Timestamp string in RFC3339 format
    time_t now = timeClient.getEpochTime();
    struct tm* t = gmtime(&now);

    char docId[11];
    strftime(docId, sizeof(docId), "%Y-%m-%d", t);

    char timestampStr[40];
    strftime(timestampStr, sizeof(timestampStr), "%Y-%m-%dT%H:%M:%SZ", t);

    Serial.println("Logging to file before sending to Firestore");
    logTemperatureToCSV(docId, timestampStr, tempToWrite); 

    if(WiFi.status() == WL_CONNECTED) {
        String key = "data";
        String documentPath = "wz_test/"+ String(docId);

        Values::MapValue entryMap("temperature", Values::DoubleValue(number_t(tempToWrite, 2)));
        entryMap.add("timestamp", Values::TimestampValue(String(timestampStr)));

        Values::ArrayValue arrV(entryMap);

        FieldTransform::AppendMissingElements<Values::ArrayValue> appendValue(arrV);
        FieldTransform::FieldTransform fieldTransforms(key, appendValue);
        DocumentTransform transform(documentPath, fieldTransforms);

        Writes writes(Write(transform, Precondition()));
        Docs.commit(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), writes, onFirestoreWriteDone, "commitTask");
    }

    UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL); // NULL = current task
    Serial.print("🧠 Stack left (words): ");
    Serial.println(stackRemaining);

    writeInProgress = false; 


    vTaskDelete(NULL);
}

void TaskResendOfflineLogs(void *pvParameters) {
    time_t now = timeClient.getEpochTime();
    struct tm *t = localtime(&now);

    char filename[32];
    strftime(filename, sizeof(filename), "/%Y-%m-%d.csv", t);  

    Serial.printf("📁 Looking for today's file: %s\n", filename);

    if (!MY_FS.exists(filename)) {
        Serial.println("❌ Today's CSV not found. Nothing to resend.");
        vTaskDelete(NULL);
        return;
    }

    File file = MY_FS.open(filename, FILE_READ);
    if (!file) {
        Serial.println("❌ Failed to open today's CSV file");
        vTaskDelete(NULL);
        return;
    }

    Serial.println("📤 Reading unsent log entries...");

    String line = "";
    int sentCount = 0;

    while (file.available()) {
        char c = file.read();
        if (c == '\n') {
            line.trim();
            if (line.length() > 0) {
                int commaIndex = line.indexOf(',');
                if (commaIndex != -1) {
                    String timestampStr = line.substring(0, commaIndex);
                    float tempVal = line.substring(commaIndex + 1).toFloat();

                    String docId = timestampStr.substring(0, 10); // yyyy-MM-dd
                    String documentPath = "wz_test/" + docId;
                    String key = "data";

                    Values::MapValue entryMap("temperature", Values::DoubleValue(number_t(tempVal, 2)));
                    entryMap.add("timestamp", Values::TimestampValue(timestampStr));

                    Values::ArrayValue arrV(entryMap);

                    FieldTransform::AppendMissingElements<Values::ArrayValue> appendValue(arrV);
                    FieldTransform::FieldTransform fieldTransforms(key, appendValue);
                    DocumentTransform transform(documentPath, fieldTransforms);

                    Writes writes(Write(transform, Precondition()));
                    Docs.commit(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), writes, onFirestoreSyncLogs, "commitTask");

                    Serial.printf("✅ Sent to Firestore: %s, %.2f\n", timestampStr.c_str(), tempVal);
                    sentCount++;
                }
            }
            line = "";
        } else if (c >= 32 || c == '\r') {
            line += c;
        }
    }

    // Handle final line if it doesn't end with newline
    line.trim();
    if (line.length() > 0) {
        int commaIndex = line.indexOf(',');
        if (commaIndex != -1) {
            String timestampStr = line.substring(0, commaIndex);
            float tempVal = line.substring(commaIndex + 1).toFloat();

            String docId = timestampStr.substring(0, 10);
            String documentPath = "wz_test/" + docId;
            String key = "data";

            Values::MapValue entryMap("temperature", Values::DoubleValue(number_t(tempVal, 2)));
            entryMap.add("timestamp", Values::TimestampValue(timestampStr));

            Values::ArrayValue arrV(entryMap);

            FieldTransform::AppendMissingElements<Values::ArrayValue> appendValue(arrV);
            FieldTransform::FieldTransform fieldTransforms(key, appendValue);
            DocumentTransform transform(documentPath, fieldTransforms);

            Writes writes(Write(transform, Precondition()));
            Docs.commit(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), writes, onFirestoreWriteDone, "commitTask");

            Serial.printf("✅ Sent to Firestore: %s, %.2f\n", timestampStr.c_str(), tempVal);
            sentCount++;
        }
    }

    file.close();

    MY_FS.remove(filename);

    Serial.printf("📦 Resend complete. Total entries sent: %d\n", sentCount);
    UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL); // NULL = current task
    Serial.print("🧠 Stack left (words): ");
    Serial.println(stackRemaining);
    
    vTaskDelete(NULL);
}

void TaskHandleFirestoreResult(void *pvParameters) {
    FirestoreResult result;

    while (1) {
        // Block until new result arrives
        if (xQueueReceive(firestoreResultQueue, &result, portMAX_DELAY)) {
            if (xSemaphoreTake(lvglMutex, portMAX_DELAY)) {
                lv_obj_add_flag(objects.ui_upload_spinner_1, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(objects.ui_sent_1, LV_OBJ_FLAG_HIDDEN);

                if (result.success) {
                    lv_obj_set_style_text_font(objects.ui_sent_1, &firestore_response_icon, LV_PART_MAIN);
                    lv_label_set_text(objects.ui_sent_1, MY_CHECK_ICON);
                    lv_label_set_text(objects.ui_sent_status, result.message.c_str());
                    lv_obj_set_style_text_color(objects.ui_sent_status, lv_color_hex(0x2cff2c), LV_PART_MAIN);
                    //#35b977
                } else {
                    lv_obj_set_style_text_font(objects.ui_sent_1, &firestore_response_icon, LV_PART_MAIN);
                    lv_obj_set_style_text_color(objects.ui_sent_1, lv_color_hex(0xFF0000), LV_PART_MAIN);  
                    lv_label_set_text(objects.ui_sent_1, MY_CROSS_ICON);
                    lv_obj_set_style_text_color(objects.ui_sent_status, lv_color_hex(0xFF0000), LV_PART_MAIN);
                    lv_label_set_text(objects.ui_sent_status, result.message.c_str());
                }

                // Optional: timer to return to main screen
                lv_timer_create([](lv_timer_t *timer) {
                    lv_obj_t *current = lv_scr_act();
                    if (current != objects.main) {
                        lv_scr_load(objects.main);
                        lv_obj_clean(current);
                        lv_obj_del(current);
                        objects.send_data = nullptr;
                    }
                    lv_timer_del(timer);
                }, 2000, NULL);

                xSemaphoreGive(lvglMutex);
            }
        }
    }
}

void TaskReadTemp(void *pvParameters) {
    while (1) {
        // Serial.println("TaskReadTemp started");
        uint16_t rtd = thermo.readRTD();

        // Serial.print("RTD value: "); Serial.println(rtd);
        float ratio = rtd;
        ratio /= 32768;
        // Serial.print("Ratio = "); Serial.println(ratio, 8);
        // Serial.print("Resistance = "); Serial.println(RREF * ratio, 8);


        float temp = thermo.temperature(RNOMINAL, RREF);
        // Serial.print("Temperature = "); Serial.println(temp);

        if (xSemaphoreTake(tempMutex, portMAX_DELAY)) {
            latestTemperature = thermo.temperature(RNOMINAL, RREF);
            xSemaphoreGive(tempMutex);
        }

        char tempText[16];
        snprintf(tempText, sizeof(tempText), "%.2f°C", temp);

        if (xSemaphoreTake(lvglMutex, portMAX_DELAY)) {
            lv_label_set_text(objects.ui_temp, tempText);
            xSemaphoreGive(lvglMutex);
        }

        // uint8_t fault = thermo.readFault();
        // if (fault) {
        //     Serial.print("Fault 0x"); Serial.println(fault, HEX);
        //     if (fault & MAX31865_FAULT_HIGHTHRESH) Serial.println("RTD High Threshold");
        //     if (fault & MAX31865_FAULT_LOWTHRESH) Serial.println("RTD Low Threshold");
        //     if (fault & MAX31865_FAULT_REFINLOW) Serial.println("REFIN- > 0.85 x Bias");
        //     if (fault & MAX31865_FAULT_REFINHIGH) Serial.println("REFIN- < 0.85 x Bias - FORCE- open");
        //     if (fault & MAX31865_FAULT_RTDINLOW) Serial.println("RTDIN- < 0.85 x Bias - FORCE- open");
        //     if (fault & MAX31865_FAULT_OVUV) Serial.println("Under/Over voltage");

        //     thermo.clearFault();
        // }
        // UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL); // NULL = current task
        // Serial.print("🧠 Stack left (words): ");
        // Serial.println(stackRemaining);
        vTaskDelay(pdMS_TO_TICKS(1000));

    }
}

void TaskLVGL(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));  // 10 ms delay

        // Try to take the semaphore, run LVGL, and release
        if (xSemaphoreTake(lvglMutex, portMAX_DELAY) == pdTRUE) {
            lv_task_handler();  // or lv_task_handler() for older LVGL versions
            xSemaphoreGive(lvglMutex);
        }

    }
}

void handleFirebaseRetry() {

    static unsigned long lastInitAttempt = 0;
    if (!app.ready() && timeSet && millis() - lastInitAttempt > 5000) {
        Serial.println("🔁 Retrying Firebase initialization...");

        time_t epoch = timeClient.getEpochTime();
        setTime(epoch);
        app.setTime(epoch);

        initializeApp(aClient, app, getAuth(sa_auth), auth_debug_print, "🔐 authTask");
        app.getApp<Firestore::Documents>(Docs);

        lastInitAttempt = millis();
    }

}

void handleOfflineResend() {
    static unsigned long lastOfflineCheck = 0;
    const unsigned long checkInterval = 60000;

    if (app.ready() && millis() - lastOfflineCheck >= checkInterval) {
        lastOfflineCheck = millis();
        Serial.println("Checking for unsent logs (5 min timer)");
        xTaskCreatePinnedToCore(TaskResendOfflineLogs, "ResendOfflineLogs", 4096, NULL, 1, &TaskResendOfflineLogsHandle, 0);
    }
}

void handleButtonPress(bool currentState, bool lastState) {
    if (app.ready() && !writeInProgress && currentState == LOW && lastState == HIGH) {
        buttonPressed = true;
        writeInProgress = true;
        Serial.println("🚀 Button Pressed, writing to Firestore");


        xSemaphoreTake(lvglMutex, portMAX_DELAY);
        // lv_scr_load_anim(objects.send_data, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, true);
        if (!objects.send_data) {
            Serial.println("takde screen");
            create_screen_send_data();  // Only if not already created
        }

        lv_scr_load(objects.send_data);
        lv_obj_add_flag(objects.ui_sent_1, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(objects.ui_sent_status, "Sending To Firestore");
        xSemaphoreGive(lvglMutex);


        xTaskCreatePinnedToCore(TaskWriteToFirestore, "writeToFirestore", 8192, NULL, 1, &TaskWriteToFirestoreHandle, 0);
    }
}

void updateFirebaseIcon() {
    static bool appWasReady = false;
    if (!appWasReady && app.ready()) {
        appWasReady = true;
        lv_obj_set_style_text_font(objects.ui_firebase_status, &fa_icon, LV_PART_MAIN);
        lv_label_set_text(objects.ui_firebase_status, MY_FIRE_ICON);
        lv_obj_set_style_text_color(objects.ui_firebase_status, lv_color_hex(0xff931c), LV_PART_MAIN);
    }
}

void updateWiFiStatus() {
    if (WiFi.status() != WL_CONNECTED) {
        lv_obj_set_style_text_color(objects.ui_wifi_status, lv_color_hex(0x808080), LV_PART_MAIN);
        lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WIFI);
    }
}

void loop()
{
    // Serial.printf("📦 Heap: %d bytes free\n", ESP.getFreeHeap()); // Check heap for memory leaks

    static bool lastButtonState = HIGH;
    bool currentState = digitalRead(14); // Button state

    // updateLVGL(); // UI Refresh
    app.loop(); // Firebase App Loop
    updateFirebaseIcon(); // Change icon to orange if Firebase is ready
    updateWiFiStatus(); // Change icon to grey if WiFi is down

    // Retry Firebase initialization if it failed 
    // Usually failed due to network issues or failed to get correct time
    handleFirebaseRetry();
    handleButtonPress(currentState, lastButtonState);
    handleOfflineResend();

    lastButtonState = currentState;
}

// With C, you unfortunately have to remember to set object references to NULL after you delete them if you wish to check for their existence later.
// [598324][E][ssl_client.cpp:37] _handle_error(): [data_to_read():361]: (-76) UNKNOWN ERROR CODE (004C)

// if (MY_FS.exists("/data_log.txt")) {
//     Serial.println("✅ data_log.txt exists");
//      if (MY_FS.remove("/data_log.txt")) {
//         Serial.println("✅ Deleted");
//     } else {
//         Serial.println("❌ Failed to delete");
//  }
// } else {
//     Serial.println("📭 data_log.txt not found");
// }