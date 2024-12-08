#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
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
const String server_url = "https://webhook.site/72961a18-51da-45a2-bd62-2a566fa2bc69";

void setup() {
  Serial.begin(115200);

  preferences.begin("wifi-config", false);

  // Intentar conectarse al WiFi guardado
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");

  if (ssid.length() > 0 && password.length() > 0) {
    Serial.println("Intentando conectar al WiFi guardado...");
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) { // Tiempo de espera 10s
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi conectado");
      Serial.print("Dirección IP: ");
      Serial.println(WiFi.localIP());
      initSensorAndTime(); // Iniciar sensor y sincronizar tiempo
      return; // Salir del setup() porque la conexión fue exitosa
    } else {
      Serial.println("\nNo se pudo conectar al WiFi guardado. Volviendo a modo AP...");
    }
  } else {
    Serial.println("No se encontraron credenciales guardadas.");
  }

  // Iniciar modo Access Point
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("Access Point iniciado. Conéctate a: ");
  Serial.println(ap_ssid);
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.softAPIP());

  // Configurar página web
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // Leer datos del sensor y enviarlos al servidor
    if (mySHTC3.update() == SHTC3_Status_Nominal) {
      temperatureNow = int(mySHTC3.toDegC() * 10); // Temperatura en ºC * 10
      humidity = int(mySHTC3.toPercent() * 10);    // Humedad en % * 10

      Serial.print("Temperatura (ºC * 10): ");
      Serial.println(temperatureNow);
      Serial.print("Humedad (% * 10): ");
      Serial.println(humidity);

      sendDataToServer(); // Enviar datos al servidor
    } else {
      Serial.println("Error al leer el sensor.");
    }

    delay(20000); // Esperar antes de enviar los siguientes datos
  } else {
    // Manejar peticiones HTTP en modo AP
    server.handleClient();
  }
}

// Página web principal
void handleRoot() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Configuración WiFi ESP32</title>
    </head>
    <body>
      <h1>Configura tu red WiFi</h1>
      <form action="/save" method="GET">
        <label for="ssid">SSID:</label><br>
        <input type="text" id="ssid" name="ssid"><br><br>
        <label for="password">Contraseña:</label><br>
        <input type="password" id="password" name="password"><br><br>
        <input type="submit" value="Guardar">
      </form>
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

// Guardar credenciales WiFi
void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  if (ssid.length() > 0 && password.length() > 0) {
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);

    String html = "<html><body><h1>Configuración guardada.</h1>";
    html += "<p>Vuelve a tu red WiFi doméstica para continuar.</p>";
    html += "<p>El dispositivo intentará conectarse a la red WiFi con los siguientes datos:</p>";
    html += "<p>SSID: " + ssid + "</p>";
    html += "</body></html>";

    server.send(200, "text/html", html);

    // Reinicia para intentar conectar a la red configurada
    delay(5000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "<h1>Error: SSID o contraseña no válidos.</h1>");
  }
}

// Iniciar sensor y sincronizar hora
void initSensorAndTime() {
  // Iniciar sensor
  Serial.println("Inicializando sensor SHTC3...");
  Wire.begin();
  if (mySHTC3.begin() == SHTC3_Status_Nominal) {
    Serial.println("Sensor SHTC3 inicializado correctamente.");
  } else {
    Serial.println("Error al inicializar el sensor SHTC3.");
    while (1);
  }

  // Configurar NTP
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
  HTTPClient http;

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


//Sketch uses 1089766 bytes (83%) of program storage space. Maximum is 1310720 bytes.
//Global variables use 36228 bytes (11%) of dynamic memory, leaving 291452 bytes for local variables. Maximum is 327680 bytes.