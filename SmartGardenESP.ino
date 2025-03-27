#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>


#include <DHT.h>
#include <DHT_U.h>

#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include "EEPROM.h"

#define DHTPIN 4

const int relayPin1 = D6;
const int relayPin2 = D7;
const int relayPin3 = D8;

DHT dht(DHTPIN, DHT22);
ESP8266WebServer server(80);
HTTPClient http;
WiFiClient client;


const char* IP = "192.168.0.24:8000";

bool ArduinoIn = false;
bool start = false;


float air_te;
float air_h;
float earth_h;

int arduino_id;



void writeEEPROM(String wifi_password){
  int len = wifi_password.length();
  for(int i=0; i<len; i++){
    EEPROM.write(i, wifi_password[i]);
  }
  EEPROM.write(len, '\0');
  EEPROM.commit();
}

void readEEPROM(){
  Serial.println("Get Wifi data");
  String SSID_and_password = "";
  char breaker;
  int i = 0;

  while (true) {
    breaker = EEPROM.read(i);
    if (breaker == '\0') {
      break;
    }
    SSID_and_password += breaker; 
    i++;
  }
  
  int separatorIndex = SSID_and_password.indexOf('_'); // Находим индекс разделителя
  String WiFiSSID;
  String WiFipassword;
  if (separatorIndex != -1) {
    WiFiSSID = SSID_and_password.substring(0, separatorIndex); // Получаем SSID
    WiFipassword = SSID_and_password.substring(separatorIndex + 1); // Получаем пароль
  }
  
  if (!WiFiSSID.isEmpty()){
    Serial.println("\nWifi: " + WiFiSSID);
    Serial.println("Password: " + WiFipassword);

    WiFi.begin(WiFiSSID, WiFipassword);
    unsigned long starttime = millis();

    while (WiFi.status() != WL_CONNECTED && millis()-starttime <=30000) {
      delay(500);
      Serial.print(".");
      }
    WiFi.softAPdisconnect(true);
  }
}

void startServer(){
  WiFi.softAP("ESP8266", "123456789");
  Serial.println("\nТочка доступа запущена");

  // Устанавливаем обработчик для POST-запроса
  server.on("/newarduino/", HTTP_POST, handlePost);

  // Запускаем сервер
  server.begin();
  Serial.println("HTTP сервер запущен");
  randomSeed(analogRead(0));
}

void getArduino(){
  Serial.println("else");
  
  JsonDocument doc;

  String url = String("http://") + IP + "/api/getformac/?mac_address=" + String(WiFi.macAddress());
  http.begin(client, url);

  int ResponseStatus = http.GET();
  if (ResponseStatus > 0) {
    String response = http.getString();
    Serial.println(response);
    DeserializationError error = deserializeJson(doc, response);
    if (doc["arduino_id"] != -1) {
      arduino_id = doc["arduino_id"];
      ArduinoIn = true;
      start = true;
    } else {
      startServer();
    }
  }
}

void handlePost(){
  if (server.hasArg("plain")) {
    String json = server.arg("plain");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    const char* wifiSSID = doc["wifiSSID"];
    const char* wifiPassword = doc["wifiPassword"];
    const char* arduino_name = doc["arduino_name"];
    const char* arduino_description = doc["arduino_description"];
    int user_id = doc["user_id"];

    // Выводим значения на Serial Monitor
    Serial.println("Полученные данные:");
    Serial.println(wifiSSID);
    Serial.println(wifiPassword);
    Serial.println(arduino_name);
    Serial.println(arduino_description);
    Serial.println(user_id);


    WiFi.begin(wifiSSID, wifiPassword);
    unsigned long starttime = millis();

    while (WiFi.status() != WL_CONNECTED && millis()-starttime <=30000) {
      delay(500);
      Serial.print(".");
    }
    Serial.print("Подключение к Wi-Fi\n");
    // Отправляем ответ клиенту
    server.send(200, "application/json", "{\"status\":\"True\"}");
    delay(1000);
    WiFi.softAPdisconnect(true);

    String WiFiData = String(wifiSSID) + "_" + String(wifiPassword);
    writeEEPROM(WiFiData);
    Serial.println(WiFiData);
    sendArduinoData(arduino_name, arduino_description, user_id);

  } else {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
  }
}

void sendArduinoData(String arduino_name, String arduino_description, int user_id){
  if (WiFi.status() == WL_CONNECTED){
    
    JsonDocument doc;

    String url = String("http://") + IP + "/api/arduino/";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    doc["user_id"] = user_id;
    doc["arduino_name"] = arduino_name;
    doc["arduino_description"] = arduino_description;
    doc["mac_address"] = String(WiFi.macAddress());

    String jsonString;
    serializeJson(doc, jsonString);
    int httpResponseCode = http.POST(jsonString);
    if (httpResponseCode > 0){
      String response = http.getString();
      Serial.println("Ответ от сервера: " + response);
      JsonDocument doc;
      deserializeJson(doc, response);
      if (doc["arduino_id"] != -1){
        arduino_id = doc["arduino_id"];
        ArduinoIn = true;
        start = true;
      }
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115000);
  EEPROM.begin(512);
  dht.begin();
  readEEPROM();

  if (WiFi.status() != WL_CONNECTED){
    startServer();
  } else {
    getArduino();
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
unsigned long check_status = 0;

bool ArduinoWork = false;


void getArduinoStatus(){
  JsonDocument doc;
  
  String url = String("http://") + IP + "/api/checkstatus/?arduino_id=" + arduino_id;
  http.begin(client, url);

  int httpResponse = http.GET();
  if (httpResponse > 0) {
    String response = http.getString();
    deserializeJson(doc, response);
    ArduinoWork = doc["status"];

    if (!ArduinoWork) {
      url += "&second=true";
      http.begin(client, url);
      int httpResponse = http.GET();
      if (httpResponse > 0) {
        String response = http.getString();
        deserializeJson(doc, response);
        ArduinoIn = doc["status"];
        if (!ArduinoIn && start){
          startServer();
          start = false;
        }
      }
    }
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  server.handleClient();

  if(millis() - check_status > 5000 && WiFi.status() == WL_CONNECTED){
    getArduinoStatus();
    check_status = millis();
  }

  if (millis() - start_time > 60000 && WiFi.status() == WL_CONNECTED && ArduinoWork) {
    start_time = millis();
    sendData();
  }

  if (millis() - check_time > 1000 && WiFi.status() == WL_CONNECTED && ArduinoWork){
    check_time = millis();
    getReleStatus();
  }
}


void getReleStatus(){
  if (WiFi.status() == WL_CONNECTED) { // Проверяем, подключены ли мы к Wi-Fi
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

void getData(){
  air_te = dht.readTemperature();
  air_h = dht.readHumidity();
}

void sendData(){
  if (WiFi.status() == WL_CONNECTED) { // Проверяем, подключены ли мы к Wi-Fi
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

