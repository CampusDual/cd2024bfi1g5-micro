#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>  // Para mDNS
#include "SparkFun_SHTC3.h"

// Crear un servidor web en el puerto 80
WebServer server(80);
Preferences preferences;

// Configuración del Access Point
const char* ap_ssid = "ESP32_Config";
const char* ap_password = "12345678";

unsigned long lastSendTime = 0;

// Configuración del sensor SHTC3
SHTC3 mySHTC3;
float humidity = 0;
float temperatureNow = 0;

// Configuración del servidor remoto
String server_url = "https://cd2024bfi1g1-dev.dev.campusdual.com/measurements/measurements";

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
      while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {  // Tiempo de espera 10s
        delay(500);
        Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi conectado");
        Serial.print("Dirección IP: ");
        Serial.println(WiFi.localIP());

        // Configurar mDNS
        if (MDNS.begin("esp32")) {  // esp32.local
          Serial.println("mDNS responder iniciado. Accede a: http://esp32.local");
        } else {
          Serial.println("Error al iniciar mDNS responder.");
        }

        initSensor();
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
  server.handleClient();  // Siempre manejar solicitudes HTTP

  if (WiFi.status() == WL_CONNECTED) {
    unsigned long currentTime = millis();
    if (currentTime - lastSendTime >= 900000) {  // 900000 ms = 15 minutos
      if (mySHTC3.update() == SHTC3_Status_Nominal) {
        temperatureNow = mySHTC3.toDegC();
        humidity = mySHTC3.toPercent();

        Serial.print("Temperatura (ºC): ");
        Serial.println(temperatureNow);
        Serial.print("Humedad (%): ");
        Serial.println(humidity);

        sendDataToServer();
        lastSendTime = currentTime;
      } else {
        Serial.println("Error al leer el sensor.");
      }
    }
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
      <style>
        body {
          font-family: Arial, sans-serif;
          margin: 0;
          padding: 0;
          display: flex;
          justify-content: center;
          align-items: center;
          height: 100vh;
          background-color: #f4f4f4;
        }
        .container {
          background-color: #fff;
          padding: 20px;
          border-radius: 8px;
          box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
          max-width: 400px;
          width: 100%;
        }
        h1 {
          text-align: center;
          color: #333;
        }
        form {
          display: flex;
          flex-direction: column;
        }
        label {
          margin-bottom: 5px;
          color: #555;
        }
        input[type="text"],
        input[type="password"] {
          padding: 10px;
          margin-bottom: 15px;
          border: 1px solid #ccc;
          border-radius: 4px;
        }
        input[type="submit"] {
          padding: 10px;
          border: none;
          border-radius: 4px;
          color: #fff;
          cursor: pointer;
        }
        input[type="submit"].save {
          background-color: #007bff;
        }
        input[type="submit"].reset {
          background-color: #dc3545;
          margin-top: 10px;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Configura tu red WiFi y servidor</h1>
        <form action="/save" method="GET">
          <label for="ssid">SSID:</label>
          <input type="text" id="ssid" name="ssid">
          <label for="password">Contraseña:</label>
          <input type="password" id="password" name="password">
          <label for="server_url">URL del servidor:</label>
          <input type="text" id="server_url" name="server_url">
          <input type="submit" value="Guardar" class="save">
        </form>
        <form action="/reset" method="GET">
          <input type="submit" value="Resetear Configuración" class="reset">
        </form>
      </div>
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html; charset=UTF-8", html);
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
    preferences.putBool("configured", true);  // Marcar como configurado

    server_url = newServerURL;  // Actualizar la URL en tiempo real

    String html = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
        <meta charset="UTF-8">
        <title>Configuración Guardada</title>
        <style>
          body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            background-color: #f4f4f4;
          }
          .container {
            background-color: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
            max-width: 400px;
            width: 100%;
            text-align: center;
          }
          h1 {
            color: #333;
          }
          p {
            color: #555;
          }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>Configuración guardada</h1>
          <p>El dispositivo se actualizará con la nueva configuración.</p>
        </div>
      </body>
      </html>
    )rawliteral";
    server.send(200, "text/html; charset=UTF-8", html);

    Serial.println("Nueva URL configurada: " + newServerURL);

    //Intentar conectarse a la Wifi
    Serial.println("Intentando conectar al WiFi guardado...");
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {  // Tiempo de espera 10s
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
    server.send(400, "text/html; charset=UTF-8", "<h1>Error: Todos los campos son obligatorios.</h1>");
  }
}

// Ruta para reset manual
void handleReset() {
  Serial.println("Borrando credenciales WiFi y reiniciando...");

  // Enviar página HTML estática con cuenta atrás
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <title>Reinicio del Dispositivo</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          display: flex;
          justify-content: center;
          align-items: center;
          height: 100vh;
          background-color: #f4f4f4;
          margin: 0;
        }
        .container {
          background-color: #fff;
          padding: 20px;
          border-radius: 8px;
          box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
          text-align: center;
        }
        h1 {
          color: #333;
        }
        p {
          color: #555;
        }
      </style>
      <script>
        let countdown = 5;
        function updateCountdown() {
          document.getElementById('countdown').innerText = countdown;
          if (countdown > 0) {
            countdown--;
            setTimeout(updateCountdown, 1000);
          }
        }
        window.onload = updateCountdown;
      </script>
    </head>
    <body>
      <div class="container">
        <h1>Reiniciando el Dispositivo</h1>
        <p>El dispositivo se pondrá en modo AP.</p>
        <p>Por favor, conéctate a la red WiFi "ESP32_Config".</p>
        <p>Reinicio en <span id="countdown">5</span> segundos...</p>
      </div>
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html; charset=UTF-8", html);

  // Esperar unos segundos antes de reiniciar
  delay(5000);

  // Borrar credenciales y reiniciar
  preferences.clear();
  preferences.putBool("reset_flag", true);  // Configurar flag de reset
  preferences.end();
  ESP.restart();
}

// Iniciar sensor
void initSensor() {
  Serial.println("Inicializando sensor SHTC3...");
  Wire.begin();
  if (mySHTC3.begin() == SHTC3_Status_Nominal) {
    Serial.println("Sensor SHTC3 inicializado correctamente.");
  } else {
    Serial.println("Error al inicializar el sensor SHTC3.");
    while (1)
      ;
  }
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

  String macAddress = WiFi.macAddress();

  String payload = "{\"data\": {";
  payload += "\"DEV_MAC\": \"" + macAddress + "\",";
  payload += "\"ME_TEMP\": " + String(temperatureNow) + ",";
  payload += "\"ME_HUMIDITY\": " + String(humidity);
  payload += "}}";

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
