// #define ENABLE_USER_AUTH
// #define ENABLE_FIRESTORE
// #define ENABLE_PSRAM 
// #define ENABLE_FS
// #define ENABLE_ESP_SSLCLIENT
// #define ENABLE_SERVICE_AUTH

// #include <FirebaseClient.h>
// #include "firebase_config.h"
// #include "ui.h"
// #include <NTPClient.h>
// #include <WiFiUdp.h>
// #include "screens.h"
// #include <AsyncTCP.h>
// #include <ESPAsyncWebServer.h>
// #include <WiFi.h> 
// #include <TimeLib.h>  
// #include <Adafruit_Sensor.h>
// #include "index.h"
// #include <DHT.h>
// #include <Adafruit_MAX31865.h>
// #include <Preferences.h>
// #include "LV_Helper.h"
// #include <vector>
// #include <set>

// #define FIREBASE_PROJECT_ID "schpi-productionsys-cp2ver1"

// String creds_file = "/serviceAccountKey.json";
// FileConfig sa_file(creds_file.c_str(), file_operation_callback);
// ServiceAuth sa_auth(getFile(sa_file));


// // UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000 /* expire period in seconds (<3600) */);

// FirebaseApp app;

// // SSL got conflicted library so use "FirebaseAsyncClient" instead of "AsyncClient"
// SSL_CLIENT ssl_client;
// using FirebaseAsyncClient = AsyncClientClass;
// FirebaseAsyncClient aClient(ssl_client);

// // Initiate doc and result for firebase response?
// Firestore::Documents Docs;
// AsyncResult firestoreResult;

// // int data_count = 0;
// // unsigned long dataMillis = 0;
// bool sent = false;

// // AP Mode Credentials
// const char* apSSID = "ESP32S3_HOTSPOT";
// const char* apPassword = "12345678";

// // Web Server for Index HTML
// AsyncWebServer server(80);
// WiFiUDP ntpUDP;
// // NTPClient timeClient(ntpUDP, "pool.ntp.org", 8 * 3600, 60000);  
// NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  
// // bool sntpTimeReady = false;

// Adafruit_MAX31865 thermo = Adafruit_MAX31865(10);
// // Adafruit_MAX31865 thermo = Adafruit_MAX31865(10, 11, 12, 13);
// #define RREF      430.0
// #define RNOMINAL  100.0

// // FreeRTOS Task Handles
// TaskHandle_t TaskTimeHandle = NULL;
// TaskHandle_t TaskSendDataHandle = NULL;
// TaskHandle_t wifiTaskHandle = NULL;
// TaskHandle_t TaskWriteToFirestoreHandle = NULL;

// std::vector<String> scannedSSIDs;
// bool scanInProgress = false;
// SemaphoreHandle_t scanMutex; // to limit access for scannedSSIDs variable

// Preferences prefs;

// String ssid;
// String password;

// int retry=0;
// bool writeInProgress = false;
// bool writeSuccess = false;


// // Function Prototypes
// void TaskUpdateTime(void *pvParameters);
// void TaskSendData(void *pvParameters);
// void TaskWriteToFirestore(void *pvParameters);
// void TaskReadTemp(void *pvParameters);
// void switchToAPSTAMode();
// void switchToSTAMode();

// // void handleSwitchMode();

// // Callback for Firebase Response
// void onFirestoreWriteDone(AsyncResult &aResult) {
//     writeInProgress = false; 
//     if (aResult.isError()) {
//         Serial.printf("❌ Firestore error: %s\n", aResult.error().message().c_str());
//         return;
//     }

//     if (aResult.available()) {
//         Serial.println("✅ Doc written successfully");
//         Serial.println(String("Response: ") + aResult.c_str()); 

//     }
// }

// void switchToAPSTAMode() {
//     Serial.println("🔄 Switching to AP Mode...");

//     WiFi.disconnect(true);
//     WiFi.mode(WIFI_AP_STA);
//     WiFi.softAP(apSSID, apPassword,1);
//     delay(1000);

//     String apIP = WiFi.softAPIP().toString();
//     char ipBuffer[30];
//     sprintf(ipBuffer, "AP: %s", apIP.c_str());
//     lv_label_set_text(objects.header_label_panel_1, ipBuffer);

//     Serial.println("✅ AP Mode Started!");
//     Serial.print("ESP32 AP IP Address: ");
//     Serial.println(WiFi.softAPIP());
// }

// // Function to switch back to STA Mode
// void switchToSTAMode() {
//     WiFi.disconnect(true);
//     WiFi.mode(WIFI_STA);
    
//     if (ssid == "" || password == "") {
//         return;
//     } else {
//         WiFi.begin(ssid.c_str(), password.c_str());
//         while (WiFi.status() != WL_CONNECTED) {
//         Serial.print('.');
//         // delay(1000);
//         }

//         Serial.println(WiFi.localIP());
//         char ipBuffer[20];

//         if (objects.ui_wifi_status) {
//             lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WIFI);
//         }
//         sprintf(ipBuffer, "STA: %s", WiFi.localIP().toString().c_str());
//         lv_label_set_text(objects.header_label_panel_1, ipBuffer);

//     }
// }

// void initWebServer() {
//     // Register all AsyncWebServer routes
//     server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
//         request->send(200, "text/html", indexHtml);
//     });

//     server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
//         if (scanInProgress) {
//             request->send(200, "application/json", "{\"status\":\"scanning\",\"ssids\":[]}");
//             return;
//         }
//         xTaskCreatePinnedToCore(TaskScanWifi, "WifiScan", 4096, NULL, 1, NULL, 0);
//         request->send(200, "application/json", "{\"status\":\"scanning\",\"ssids\":[]}");
//     });

//     server.on("/scanResult", HTTP_GET, [](AsyncWebServerRequest *request) {
//         if (scanInProgress) {
//             request->send(200, "application/json", "{\"status\":\"scanning\",\"ssids\":[]}");
//             return;
//         }

//         String json = "{\"status\":\"done\",\"ssids\":[";
//         xSemaphoreTake(scanMutex, portMAX_DELAY);
//         std::set<String> uniqueSSIDs;
//         bool first = true;
//         for (const auto& ssid : scannedSSIDs) {
//             if (uniqueSSIDs.insert(ssid).second) {
//                 if (!first) json += ",";
//                 json += "\"" + ssid + "\"";
//                 first = false;
//             }
//         }
//         xSemaphoreGive(scanMutex);
//         json += "]}";
//         request->send(200, "application/json", json);
//     });

//     server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
//         String ssid, pass;

//         if (request->hasParam("ssid", true)) {
//             ssid = request->getParam("ssid", true)->value();
//         }
//         if (request->hasParam("password", true)) {
//             pass = request->getParam("password", true)->value();
//         }

//         Serial.println("📥 Received WiFi creds:");
//         Serial.println("SSID: " + ssid);
//         Serial.println("PASS: " + pass);

//         Preferences prefs;
//         if (!prefs.begin("wifi", false)) {  // false = read/write
//             Serial.println("❌ Failed to open preferences (read/write)");
//             request->send(500, "text/plain", "Failed to save WiFi credentials.");
//             return;
//         }
//         prefs.putString("ssid", ssid);
//         prefs.putString("pass", pass);
//         prefs.end();

//         request->send(200, "text/html", "<h2>Connecting...<br>Check screen or Serial Monitor.</h2>");
//         // switchToSTAMode(); 
//     });

//         server.on("/wifi_config", HTTP_GET, [](AsyncWebServerRequest *request) {
//             Preferences prefs;
//             String savedSSID = "";
//             String savedPass = "";

//             if (prefs.begin("wifi", true)) {
//                 savedSSID = prefs.getString("ssid", "");
//                 savedPass = prefs.getString("pass", "");
//                 prefs.end();
//             } else {
//                 Serial.println("❌ Failed to open preferences namespace!");
//                 request->send(500, "application/json", "{\"error\":\"prefs_failed\"}");
//                 return;
//             }

//             String json = "{\"ssid\":\"" + savedSSID + "\",\"pass\":\"" + savedPass + "\"}";
//             request->send(200, "application/json", json);
//         });


//     server.begin();
// }



// void setup()
// {       

//     lv_helper();
//     ui_init();
//     scanMutex = xSemaphoreCreateMutex();

//     prefs.begin("wifi", false);
//     ssid = prefs.getString("ssid", ""); 
//     password = prefs.getString("pass", "");
//     prefs.end();

//     #if defined(ENABLE_FS)
//     MY_FS.begin();
//     #endif



//     if (ssid == "" || password == "") {
//         switchToAPSTAMode();
//         lv_label_set_text(objects.ui_wifi_status, LV_SYMBOL_WARNING);
//     } else {
//         switchToSTAMode();

//         timeClient.begin();  // Start NTP time sync
//         timeClient.update();

//         set_ssl_client_insecure_and_buffer(ssl_client);

//         time_t epoch = timeClient.getEpochTime();
//         setTime(epoch);
//         app.setTime(epoch); //Error code:-119 time was not set or valid

//         // app.setTime(get_ntp_time());


//         // timeClient.update();                   
//         // setTime(timeClient.getEpochTime()); 
//         // dht.begin();  

//         // initializeApp(aClient, app, getAuth(user_auth), auth_debug_print, "🔐 authTask");
  
//         initializeApp(aClient, app, getAuth(sa_auth), auth_debug_print, "🔐 authTask");
//         app.getApp<Firestore::Documents>(Docs);

//     }

//     Serial.println("burrrr");

//     initWebServer();

//     // Set initial label values
//     lv_label_set_text(objects.ui_test_datetime, "Time: --:--:--");
//     lv_label_set_text(objects.ui_temp, "--°C");  
//     lv_label_set_text(objects.ui_upload_status, LV_SYMBOL_UPLOAD);

//     thermo.begin(MAX31865_3WIRE); 


//     // Create FreeRTOS tasks
//     xTaskCreatePinnedToCore(TaskUpdateTime, "UpdateTime", 8192, NULL, 1, &TaskTimeHandle, 1);
//     xTaskCreatePinnedToCore(TaskSendData, "SendData", 8192, NULL, 1, &TaskSendDataHandle, 1);
//     xTaskCreatePinnedToCore(TaskReadTemp, "ReadTemp", 8192, NULL, 1, NULL, 1);
// }

// void TaskUpdateTime(void *pvParameters) {
//     while (1) {
//         timeClient.update();  // Sync time from NTP


//         // float t = dht.readTemperature();

//         // if (isnan(t)) {
//         //   Serial.println(F("Failed to read from DHT sensor!"));
//         //   lv_label_set_text(objects.ui_temp, "--°C");
//         // } else {
//         //   char tempStr[15];
//         //   sprintf(tempStr, "%.1f°C", t);
//         //   lv_label_set_text(objects.ui_temp, tempStr);     
//         // }

//         // time_t now = time(nullptr);
//         // struct tm *timeInfo = localtime(&now);


//         // time_t rawTime = timeClient.getEpochTime();
//         time_t rawTime = timeClient.getEpochTime() + 8 * 3600;
//         struct tm *timeInfo = localtime(&rawTime);

//         char datetimeStr[20];
//         sprintf(datetimeStr, "%04d-%02d-%02d  %02d:%02d", 
//                 timeInfo->tm_year + 1900, timeInfo->tm_mon + 1, timeInfo->tm_mday,
//                 timeInfo->tm_hour, timeInfo->tm_min);

//         // Format time as HH:MM:SS
//         // String formattedTime = timeClient.getFormattedTime();
//         // lv_label_set_text(objects.ui_test_datetime, formattedTime.c_str());
//         lv_label_set_text(objects.ui_test_datetime, datetimeStr);

//         lv_task_handler(); 
//         vTaskDelay(pdMS_TO_TICKS(2000));  // Wait 5 seconds before next send
//     }
// }

// void TaskSendData(void *pvParameters) {
//     while (1) {
//         lv_label_set_text(objects.ui_upload_status, LV_SYMBOL_UPLOAD);
//         lv_task_handler();

//         vTaskDelay(pdMS_TO_TICKS(3000));  // Simulate sending delay

//         lv_label_set_text(objects.ui_upload_status, LV_SYMBOL_OK);

//         lv_task_handler();
//         vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds before next send
//     }
// }

// void TaskScanWifi(void *pvParameters) {
//     scanInProgress = true;

//     Serial.println("🔧 Starting Wi-Fi scan");

//     // WiFi.disconnect(false, true);  // Disconnect STA, preserve AP
//     // delay(200);                    // Let things stabilize
//     WiFi.scanDelete();             // Clear leftovers

//     xSemaphoreTake(scanMutex, portMAX_DELAY);
//     scannedSSIDs.clear();
//     xSemaphoreGive(scanMutex);

//     Serial.println("📡 Scanning...");
//     int n = WiFi.scanNetworks();

//     if (n > 0) {
//         xSemaphoreTake(scanMutex, portMAX_DELAY);
//         for (int i = 0; i < n; ++i) {
//             String ssid = WiFi.SSID(i);
//             if (ssid.length() > 0) {
//                 scannedSSIDs.push_back(ssid);
//                 Serial.println(" 📶 " + ssid);
//             }
//         }
//         xSemaphoreGive(scanMutex);
//     } else {
//         Serial.println("⚠️ No networks found or scan failed");
//     }

//     scanInProgress = false;

//     vTaskDelete(NULL);
// }

// void TaskWriteToFirestore(void *pvParameters) {
//     if (WiFi.status() != WL_CONNECTED) {
//         Serial.println("❌ Skipping Firestore write: WiFi not connected");
//         vTaskDelete(NULL);
//         return;
//     }

//     if(timeNotSet){
//         Serial.println("❌ Skipping Firestore write: System time not synced");
//         vTaskDelete(NULL);
//         return;
//     }

//     // Timestamp string in RFC3339 format
//     time_t now = timeClient.getEpochTime();
//     struct tm* t = gmtime(&now);

//     char docId[11];
//     strftime(docId, sizeof(docId), "%Y-%m-%d", t);

//     char timestampStr[40];
//     strftime(timestampStr, sizeof(timestampStr), "%Y-%m-%dT%H:%M:%SZ", t);

//     float temperature = 90.5 + random(-100, 100) / 100.0;

//     String tsKey = String(now); // unix Timestamp

//     Values::MapValue entryMap("temperature", Values::DoubleValue(number_t(temperature, 2)));
//     entryMap.add("timestamp", Values::TimestampValue(String(timestampStr)));

//     // Build the Firestore document
//     Document<Values::Value> doc(tsKey, Values::Value(entryMap));

//     // Patch options to mimic { merge: true }
//     PatchDocumentOptions patchOptions(
//         DocumentMask(tsKey), // only affect this one key
//         DocumentMask(),      // optional response mask
//         Precondition()       // allow creating the doc if it doesn't exist
//     );

//     // Document path
//     String documentPath = "wz_test/"+ String(docId);

//     Docs.patch(
//         aClient,
//         Firestore::Parent(FIREBASE_PROJECT_ID),
//         documentPath,
//         patchOptions,
//         doc,
//         onFirestoreWriteDone,           
//         "writeTemp"    // label
//     );
//     vTaskDelete(NULL);
// }

// void TaskReadTemp(void *pvParameters) {
//     while (1) {
//         Serial.println("TaskReadTemp started");
//         uint16_t rtd = thermo.readRTD();

//         Serial.print("RTD value: "); Serial.println(rtd);
//         float ratio = rtd;
//         ratio /= 32768;
//         Serial.print("Ratio = "); Serial.println(ratio, 8);
//         Serial.print("Resistance = "); Serial.println(RREF * ratio, 8);
//         Serial.print("Temperature = "); Serial.println(thermo.temperature(RNOMINAL, RREF));

//         uint8_t fault = thermo.readFault();
//         if (fault) {
//             Serial.print("Fault 0x"); Serial.println(fault, HEX);
//             if (fault & MAX31865_FAULT_HIGHTHRESH) Serial.println("RTD High Threshold");
//             if (fault & MAX31865_FAULT_LOWTHRESH) Serial.println("RTD Low Threshold");
//             if (fault & MAX31865_FAULT_REFINLOW) Serial.println("REFIN- > 0.85 x Bias");
//             if (fault & MAX31865_FAULT_REFINHIGH) Serial.println("REFIN- < 0.85 x Bias - FORCE- open");
//             if (fault & MAX31865_FAULT_RTDINLOW) Serial.println("RTDIN- < 0.85 x Bias - FORCE- open");
//             if (fault & MAX31865_FAULT_OVUV) Serial.println("Under/Over voltage");

//             thermo.clearFault();
//         }

//         Serial.println();
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }


// void loop()
// {
//     lv_task_handler();
//     app.loop();

//     // if (app.ready() && retry < 3) {
//     //     // sent = true;
//     //     xTaskCreatePinnedToCore(TaskWriteToFirestore, "writeToFirestore", 16384, NULL, 1, &TaskWriteToFirestoreHandle, 0);  
//     //     retry++;
//     // }

//     static unsigned long lastRetry = 0;
//     static int retryCount = 0;
 
//     if (app.ready() && !writeInProgress && !writeSuccess && retryCount < 3 && millis() - lastRetry > 5000) {
//         Serial.println("🚀 Trying Firestore write");
//         writeInProgress = true;
//         lastRetry = millis();
//         retryCount++;
//         xTaskCreatePinnedToCore(TaskWriteToFirestore, "writeToFirestore", 16384, NULL, 1, &TaskWriteToFirestoreHandle, 0);
//     }
 
// }


// // void TaskWriteToFirestore(void *pvParameters) {
// //     if (WiFi.status() != WL_CONNECTED) {
// //         Serial.println("❌ Skipping Firestore write: WiFi not connected");
// //         vTaskDelete(NULL);
// //         return;
// //     }

// //     if(timeNotSet){
// //         Serial.println("❌ Skipping Firestore write: System time not synced");
// //         vTaskDelete(NULL);
// //         return;
// //     }

// //     // Timestamp string in RFC3339 format
// //     time_t now = timeClient.getEpochTime();
// //     struct tm* t = gmtime(&now);
// //     char timestampStr[40];
// //     strftime(timestampStr, sizeof(timestampStr), "%Y-%m-%dT%H:%M:%SZ", t);

// //     float temperature = 90.5 + random(-100, 100) / 100.0;

// //     // Build the Firestore document
// //     Document<Values::Value> doc("temperature", Values::Value(Values::DoubleValue(number_t(temperature, 2))));
// //     doc.add("timestamp", Values::Value(Values::TimestampValue(String(timestampStr))));

// //     // Document path
// //     String documentPath = "wz_test";

// //     Docs.createDocument(
// //         aClient,
// //         Firestore::Parent(FIREBASE_PROJECT_ID),
// //         documentPath,
// //         DocumentMask(),
// //         doc,
// //         onFirestoreWriteDone,            // callback to handle result
// //         "writeTemp"    // label
// //     );
// //     vTaskDelete(NULL);
// // }