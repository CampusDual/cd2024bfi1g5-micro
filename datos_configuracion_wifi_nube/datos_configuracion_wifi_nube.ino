#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "config.h"
#include "SparkFun_SHTC3.h"

SHTC3 mySHTC3;  
int16_t humidity = 0;        
int16_t temperatureNow = 0;   

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600; 
const int daylightOffset_sec = 3600; 

void setup() {
  Serial.begin(9600);

   Serial.println("Inicializando sensor SHTC3...");
  Wire.begin();
  if (mySHTC3.begin() == SHTC3_Status_Nominal) {
    Serial.println("Sensor SHTC3 inicializado correctamente.");
  } else {
    Serial.println("Error al inicializar el sensor SHTC3.");
    while (1);
  }

  Serial.print("Conectando al WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Dirección MAC: ");
  Serial.println(WiFi.macAddress());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando hora...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Fallo al obtener la hora. Reintentando...");
    delay(2000);
  }
  Serial.println("Hora sincronizada correctamente.");
}

void loop() {

  if (mySHTC3.update() == SHTC3_Status_Nominal) {

    temperatureNow = int(mySHTC3.toDegC() * 10); 
    humidity = int(mySHTC3.toPercent() * 10);    

    Serial.print("Temperatura (ºC * 10): ");
    Serial.println(temperatureNow);
    Serial.print("Humedad (% * 10): ");
    Serial.println(humidity);
  } else {
    Serial.println("Error al leer el sensor.");
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(server_url.c_str());
    http.addHeader("Content-Type", "application/json");

    String currentTime = getFormattedTime();

    String macAddress = WiFi.macAddress();

    String payload = "{";
    payload += "\"m\": \"" + macAddress + "\",";
    payload += "\"t\": \"" + currentTime + "\",";
    payload += "\"te\": " + String(temperatureNow) + ","; 
    payload += "\"h\": " + String(humidity);              
    payload += "}";

    http.begin(server_url);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Respuesta del servidor: ");
      Serial.println(response);
    } else {
      Serial.print("Error al enviar datos: ");
      Serial.println(http.errorToString(httpResponseCode));
    }

    http.end(); 
  } else {
    Serial.println("WiFi desconectado");
  }

  delay(20000); 
}

String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Error obteniendo hora";
  }
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", &timeinfo); 
  return String(buffer);
}