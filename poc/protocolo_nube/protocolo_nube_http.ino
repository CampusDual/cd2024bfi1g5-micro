#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Crear un servidor web en el puerto 80
WebServer server(80);
Preferences preferences;

const char* ap_ssid = "ESP32_Config";
const char* ap_password = "12345678";

void setup() {
  Serial.begin(9600);

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
      return; // Salir de setup() porque la conexión fue exitosa
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
  server.on("/", handleRoot);         // Página principal
  server.on("/save", handleSave);     // Ruta para guardar datos
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  server.handleClient();
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