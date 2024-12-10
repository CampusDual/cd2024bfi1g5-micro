#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ESPmDNS.h> // Para mDNS
#include <time.h>
#include "SparkFun_SHTC3.h"

// Crear un servidor web en el puerto 80
WebServer server(80);
Preferences preferences;

// Configuración del Access Point
const char* ap_ssid = "ESP32_Config";
const char* ap_password = "12345678";

// Configuración del servidor HTTP y NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// Configuración del sensor SHTC3
SHTC3 mySHTC3;
int16_t humidity = 0;
int16_t temperatureNow = 0;

// Configuración del servidor remoto
String server_url = "https://webhook.site/#!/view/1f8d7fde-961c-471a-b731-1f2ce41b78ec";



void setup() {
  Serial.begin(9600);

  preferences.begin("wifi-config", false);

  // Verificar si el flag de configuración está presente
  bool configured = preferences.getBool("configured", false);

  if (!configured) {
    // No hay configuración guardada, iniciar en modo AP
    Serial.println("No se encontró configuración. Iniciando modo AP...");
    startAccessPoint();
  } else {
    // Intentar conectarse a WiFi con las credenciales guardadas
    String wifiSSID = preferences.getString("ssid", "");
    String wifiPassword = preferences.getString("password", "");
    server_url = preferences.getString("server_url", "");

    if (wifiSSID.length() > 0 && wifiPassword.length() > 0 && server_url.length() > 0) {
      Serial.println("Intentando conectar al WiFi guardado...");
      WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

      unsigned long startTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) { // Tiempo de espera 10s
        delay(500);
        Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi conectado");
        Serial.print("Dirección IP: ");
        Serial.println(WiFi.localIP());
        
        // Configurar mDNS
        if (MDNS.begin("esp32")) { // esp32.local
          Serial.println("mDNS responder iniciado. Accede a: http://esp32.local");
        } else {
          Serial.println("Error al iniciar mDNS responder.");
        }
        
        initSensorAndTime();
      } else {
        Serial.println("\nNo se pudo conectar al WiFi guardado. Iniciando modo AP...");
        startAccessPoint();
      }
    } else {
      Serial.println("Datos incompletos. Iniciando modo AP...");
      startAccessPoint();
    }
  }

  // Configurar rutas del servidor HTTP
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/reset", handleReset);
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  server.handleClient(); // Siempre manejar solicitudes HTTP

  if (WiFi.status() == WL_CONNECTED) {
    if (mySHTC3.update() == SHTC3_Status_Nominal) {
      temperatureNow = int(mySHTC3.toDegC() * 10);
      humidity = int(mySHTC3.toPercent() * 10);

      Serial.print("Temperatura (ºC * 10): ");
      Serial.println(temperatureNow);
      Serial.print("Humedad (% * 10): ");
      Serial.println(humidity);

      sendDataToServer();
    } else {
      Serial.println("Error al leer el sensor.");
    }
    delay(20000); // Intervalo entre envíos de datos
  }
}

// Iniciar Access Point
void startAccessPoint() {
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("Access Point iniciado. Conéctate a: ");
  Serial.println(ap_ssid);
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.softAPIP());
}

// Página web principal
void handleRoot() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <title>Configuración ESP32</title>
    </head>
    <body>
      <h1>Configura tu red WiFi y servidor</h1>
      <form action="/save" method="GET">
        <label for="ssid">SSID:</label><br>
        <input type="text" id="ssid" name="ssid"><br><br>
        <label for="password">Contraseña:</label><br>
        <input type="password" id="password" name="password"><br><br>
        <label for="server_url">URL del servidor:</label><br>
        <input type="text" id="server_url" name="server_url"><br><br>
        <input type="submit" value="Guardar">
      </form>
      <form action="/reset" method="GET" style="margin-top: 20px;">
        <input type="submit" value="Resetear Configuración">
      </form>
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

// Guardar credenciales y servidor
void handleSave() {
  String wifiSSID = server.arg("ssid");
  String wifiPassword = server.arg("password");
  String newServerURL = server.arg("server_url");

  if (wifiSSID.length() > 0 && wifiPassword.length() > 0 && newServerURL.length() > 0) {
    preferences.putString("ssid", wifiSSID);
    preferences.putString("password", wifiPassword);
    preferences.putString("server_url", newServerURL);
    preferences.putBool("configured", true); // Marcar como configurado

    server_url = newServerURL; // Actualizar la URL en tiempo real

    String html = "<html><body><h1>Configuración guardada.</h1>";
    html += "<p>El dispositivo se actualizará con la nueva configuración.</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);

    Serial.println("Nueva URL configurada: " + newServerURL);

    //Intentar conectarse a la Wifi
    Serial.println("Intentando conectar al WiFi guardado...");
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) { // Tiempo de espera 10s
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi conectado");
      Serial.print("Dirección IP: ");
      Serial.println(WiFi.localIP());
      Serial.println("Desactivando Access Point...");
      WiFi.softAPdisconnect(true);
    } else {
      Serial.println("\nError al conectarse a la Wifi");
      startAccessPoint();
    }

  } else {
    server.send(400, "text/html", "<h1>Error: Todos los campos son obligatorios.</h1>");
  }
}

// Ruta para reset manual
void handleReset() {
  Serial.println("Borrando credenciales WiFi y reiniciando...");
  preferences.clear();
  preferences.putBool("reset_flag", true); // Configurar flag de reset
  preferences.end();
  ESP.restart();
}

// Iniciar sensor y sincronizar hora
void initSensorAndTime() {
  Serial.println("Inicializando sensor SHTC3...");
  Wire.begin();
  if (mySHTC3.begin() == SHTC3_Status_Nominal) {
    Serial.println("Sensor SHTC3 inicializado correctamente.");
  } else {
    Serial.println("Error al inicializar el sensor SHTC3.");
    while (1);
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando hora...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Fallo al obtener la hora. Reintentando...");
    delay(2000);
  }
  Serial.println("Hora sincronizada correctamente.");
}

// Enviar datos al servidor
void sendDataToServer() {
  if (server_url.length() == 0) {
    Serial.println("Error: La URL del servidor está vacía.");
    return;
  }

  HTTPClient http;

  Serial.print("Enviando datos al servidor: ");
  Serial.println(server_url);

  http.begin(server_url);
  http.addHeader("Content-Type", "application/json");

  String currentTime = getFormattedTime();
  String macAddress = WiFi.macAddress();

  String payload = "{";
  payload += "\"m\": \"" + macAddress + "\",";
  payload += "\"t\": \"" + currentTime + "\",";
  payload += "\"te\": " + String(temperatureNow) + ",";
  payload += "\"h\": " + String(humidity);
  payload += "}";

  Serial.print("Payload enviado: ");
  Serial.println(payload);

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
}

// Obtener la hora formateada
String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Error obteniendo hora";
  }
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", &timeinfo);
  return String(buffer);
}