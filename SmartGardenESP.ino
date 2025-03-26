#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <EEPROM.h>

#define DHTPIN 4

DHT dht(DHTPIN, DHT22);

const int relayPin1 = D6;
const int relayPin2 = D7;
const int relayPin3 = D8;

// Настройки точки доступа
String WiFiSSID = "";
String WiFipassword = "";

const char* IP = "192.168.0.24:8000";

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
    String WiFiData = String(wifiSSID) + "_" + String(wifiPassword);
    int len = WiFiData.length();
    for (int i=0; i<len; i++){
      EEPROM.write(i, char(WiFiData[i]));
    }
    EEPROM.write(len, '\0');
    EEPROM.commit();
    Serial.println(WiFiData);
    sendArduinoData();

  } else {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
  }
}

void sendArduinoData(){
  if (WiFi.status() == WL_CONNECTED) { // Проверяем, подключены ли мы к Wi-Fi
    
    WiFi.softAPdisconnect(true);
    
    String WiFiData = "";
    

     WiFiClient client;
        HTTPClient http;
    http.begin(client, String("http://") + IP + "/api/arduino/");
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
      } else {
        Serial.print("Ошибка при отправке запроса: ");
        Serial.println(httpResponseCode);
        }
    http.end();

  }
}


void getWiFiData(){

  Serial.println("Get Wifi data");
  String SSID_and_password = "";
  char breaker;
  int i = 0;

  // Чтение данных из EEPROM
  while (true) {
    Serial.println(char(EEPROM.read(i)));
    breaker = EEPROM.read(i);
    
    // Проверка на завершение строки
    if (breaker == '\0') {
      break; // Выход из цикла, если достигнут завершающий нуль
    }
    
    SSID_and_password += breaker; 
    
    i++;
  }
  String ssid = "";
  String pass = "";
  int separatorIndex = SSID_and_password.indexOf('_'); // Находим индекс разделителя

  Serial.println(SSID_and_password);
  if (separatorIndex != -1) {
    WiFiSSID = SSID_and_password.substring(0, separatorIndex); // Получаем SSID
    WiFipassword = SSID_and_password.substring(separatorIndex + 1); // Получаем пароль
  }
  
  if (!WiFiSSID.isEmpty()){
    Serial.println("Wifi: " + WiFiSSID);
    Serial.println("Password: " + WiFipassword);

    WiFi.begin(WiFiSSID, WiFipassword);
    unsigned long starttime = millis();
    unsigned long endtime = millis();

    while (WiFi.status() != WL_CONNECTED && endtime-starttime <=30000) {
      delay(500);
      Serial.print(".");
      endtime = millis();
      }
  }


}

void setup() {
  Serial.begin(115200);
  dht.begin();
  EEPROM.begin(512);
  Serial.println("Start");
  getWiFiData();
  if(WiFi.status() != WL_CONNECTED){
  // Настраиваем точку доступа
    WiFi.softAP(ssid, password);
    Serial.println("\nТочка доступа запущена");

    // Устанавливаем обработчик для POST-запроса
    server.on("/newarduino/", HTTP_POST, handlePost);

    // Запускаем сервер
    server.begin();
    Serial.println("HTTP сервер запущен");
    randomSeed(analogRead(0));
  } else {
    Serial.println("else");
    WiFiClient client;
    HTTPClient http;
    String url = String("http://") + IP + "/api/getformac/?mac_address=" + String(WiFi.macAddress());
    
    http.begin(client, url);

    JsonDocument doc;

    int ResponseStatus = http.GET();
    if (ResponseStatus > 0) {
    String response = http.getString();
    Serial.println(response);
    DeserializationError error = deserializeJson(doc, response);
    arduino_id = doc["arduino_id"];

    }
  }

  pinMode(relayPin1, OUTPUT);
  digitalWrite(relayPin1, HIGH);
  pinMode(relayPin2, OUTPUT);
  digitalWrite(relayPin2, HIGH);
  pinMode(relayPin3, OUTPUT);
  digitalWrite(relayPin3, HIGH);

}

unsigned long start_time = 0;
unsigned long check_time = 0;

void loop() {
    server.handleClient();
    if (millis() - start_time > 60000 && WiFi.status() == WL_CONNECTED) {
      start_time = millis();
      sendData();
    }
    if (millis() - check_time > 1000 && WiFi.status() == WL_CONNECTED){
      check_time = millis();
      getReleStatus();
    }

}

void getReleStatus(){
  if (WiFi.status() == WL_CONNECTED) { // Проверяем, подключены ли мы к Wi-Fi
    WiFiClient client;
    HTTPClient http;
    String url = String("http://") + IP + "/api/relestatus/?arduino_id=" + String(arduino_id);
    http.begin(client,  url);
    
    JsonDocument doc;
    
    int httpResponse = http.GET();
    if (httpResponse > 0) {
      String response = http.getString();
      // Serial.println("RELE ANSWER: " + response);
      DeserializationError error = deserializeJson(doc, response);

      bool value1 = doc["1"];
      bool value2 = doc["2"];
      bool value3 = doc["3"];

      if (value1){
        digitalWrite(relayPin1, LOW);
      } else {digitalWrite(relayPin1, HIGH);}

      if (value2){
        digitalWrite(relayPin2, HIGH);
      } else {digitalWrite(relayPin2, LOW);}

      if (value3){
        digitalWrite(relayPin3, HIGH);
      } else {digitalWrite(relayPin3, LOW);}


    } else {
      String response = http.getString();
      Serial.print("RELE Ошибка от сервера: ");
      Serial.println(response);
    }
  }
}

float air_te;
float air_h;
float earth_h;

void getData(){
  air_te = dht.readTemperature();
  air_h = dht.readHumidity();


}

void sendData(){
  if (WiFi.status() == WL_CONNECTED) { // Проверяем, подключены ли мы к Wi-Fi
    WiFiClient client;
    HTTPClient http;
    getData();
    http.begin(client,  String("http://") + IP + "/api/data/");
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["arduino_id"] = arduino_id;
    doc["air_t"] = air_te;
    doc["air_h"] = air_h;
    doc["earth_h"] = random(0, 12);

    String jsonString;
    serializeJson(doc, jsonString);

    int httpResponse = http.POST(jsonString);
    if (httpResponse > 0) {
      String response = http.getString();
    } else {
      String response = http.getString();
      Serial.print("Ошибка от сервера: ");
      Serial.println(response);
    }
    

  }
}
