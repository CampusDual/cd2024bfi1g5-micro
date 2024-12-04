#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
 
WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido: ");
  Serial.println(topic);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(9600);

  // Conexión a Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");

  // Configuración de MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.print("Conectando a MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Conectado");
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("Fallo, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void loop() {
  client.loop();
  client.publish(mqtt_topic, "Hola desde ESP32C3 con MQTT.cool");
  delay(5000);
}