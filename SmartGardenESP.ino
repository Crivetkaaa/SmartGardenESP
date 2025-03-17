#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// Настройки точки доступа
const char* ssid = "ESP8266";
const char* password = "123456789";

int user_id;
const char* ardiono_name;
const char* arduino_description;
int arduino_id;


ESP8266WebServer server(80);

void handlePost() {
  // Проверяем, есть ли тело запроса
  if (server.hasArg("plain")) {
    // Получаем тело запроса
    String json = server.arg("plain");

    // Создаем объект для разбора JSON
    JsonDocument doc;

    // Разбираем JSON
    DeserializationError error = deserializeJson(doc, json);

    // Проверяем на ошибки
    if (error) {
      Serial.print(F("Ошибка разбора JSON: "));
      Serial.println(error.f_str());
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    // Извлекаем значения
    const char* wifiSSID = doc["wifiSSID"];
    const char* wifiPassword = doc["wifiPassword"];
    ardiono_name = doc["ardiono_name"];
    arduino_description = doc["arduino_description"];
    user_id = doc["user_id"];

    // Выводим значения на Serial Monitor
    Serial.println("Полученные данные:");
    Serial.println(wifiSSID);
    Serial.println(wifiPassword);
    Serial.println(ardiono_name);
    Serial.println(arduino_description);
    Serial.println(user_id);


    WiFi.begin(wifiSSID, wifiPassword);
    unsigned long starttime = millis();
    unsigned long endtime = millis();

    while (WiFi.status() != WL_CONNECTED && endtime-starttime <=30000) {
      delay(500);
      Serial.print(".");
      endtime = millis();
    }

    Serial.print("Подключение к Wi-Fi");
    // Отправляем ответ клиенту
    server.send(200, "application/json", "{\"status\":\"True\"}");
    sendArduinoData();

  } else {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
  }
}

void sendArduinoData(){
  if (WiFi.status() == WL_CONNECTED) { // Проверяем, подключены ли мы к Wi-Fi
     WiFiClient client;
        HTTPClient http;
    http.begin(client, "http://188.32.24.142:12000/api/arduino/");
    http.addHeader("Content-Type", "application/json");

    String mac_address = WiFi.macAddress();

    JsonDocument doc;
    Serial.print("id пользователя: ");
    Serial.println(user_id);
    doc["user_id"] = user_id;
    doc["arduino_name"] = ardiono_name;
    doc["arduino_description"] = arduino_description;
    doc["mac_address"] = mac_address;

    String jsonString;
    serializeJson(doc, jsonString);

    int httpResponseCode = http.POST(jsonString);
    if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Ответ от сервера: " + response);
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);

            if (error) {
        Serial.print(F("Ошибка разбора JSON: "));
        Serial.println(error.f_str());
        return;
    }
            arduino_id = doc["arduino_id"];
            Serial.print("Arduino id: ");
            Serial.println(arduino_id);
        } else {
            Serial.print("Ошибка при отправке запроса: ");
            Serial.println(httpResponseCode);
        }
    http.end();
    WiFi.softAPdisconnect(true);

  }
}

void setup() {
  Serial.begin(115200);
  
  // Настраиваем точку доступа
  WiFi.softAP(ssid, password);
  Serial.println("Точка доступа запущена");

  // Устанавливаем обработчик для POST-запроса
  server.on("/newarduino/", HTTP_POST, handlePost);

  // Запускаем сервер
  server.begin();
  Serial.println("HTTP сервер запущен");
  randomSeed(analogRead(0));
}

unsigned long start_time = 0;
void loop() {
    server.handleClient();
    if (millis() - start_time > 30 && WiFi.status() == WL_CONNECTED) {
      start_time = millis();
      sendData();
    }
}

void sendData(){
  if (WiFi.status() == WL_CONNECTED) { // Проверяем, подключены ли мы к Wi-Fi
    WiFiClient client;
    HTTPClient http;

    http.begin(client,  "http://188.23.24.142:12000/api/data/");
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["arduino_id"] = arduino_id;
    doc["air_t"] = random(0, 35);
    doc["air_h"] = random(0, 22);
    doc["earth_h"] = random(0, 12);

    String jsonString;
    serializeJson(doc, jsonString);

    int httpResponse = http.POST(jsonString);
    

  }
}