#include <WiFi.h>
#include <Preferences.h>
#include "config.h"
 
Preferences preferences;
 
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  delay(10);
 
  preferences.begin("wifi-config", false);
 
  // Guarda las credenciales de WiFi en la memoria no volátil
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
 
  // Lee las credenciales de WiFi desde la memoria no volátil
  String stored_ssid = preferences.getString("ssid", "default_ssid");
  String stored_password = preferences.getString("password", "default_password");
 
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(stored_ssid);
 
  WiFi.begin(stored_ssid.c_str(), stored_password.c_str());
 
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