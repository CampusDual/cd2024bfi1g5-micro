#include <WiFi.h>

// Credenciales de WiFi
const char* ssid = "nombre";
const char* password = "contraseña";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  delay(10);

  // Conectar a la red WiFi
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // put your main code here, to run repeatedly:

}
