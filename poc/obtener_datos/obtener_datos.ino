#include <Wire.h>
#include "Adafruit_SHTC3.h"

// Crear una instancia del sensor
Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();

void setup() {
  Serial.begin(9600);
  delay(1000); // Esperar un momento para inicializar

  // Inicializar el sensor
  if (!shtc3.begin()) {
    Serial.println("No se pudo encontrar un sensor SHTC3");
    while (1); // Detener el programa si no se encuentra el sensor
  }

  Serial.println("Sensor SHTC3 inicializado correctamente.");
}

void loop() {
  sensors_event_t humedad, temperatura;

  // Leer los datos del sensor
  if (shtc3.getEvent(&humedad, &temperatura)) {
    Serial.print("Temperatura: ");
    Serial.print(temperatura.temperature);
    Serial.println(" °C");

    Serial.print("Humedad: ");
    Serial.print(humedad.relative_humidity);
    Serial.println(" %");
  } else {
    Serial.println("Error al leer los datos del sensor");
  }

  delay(2000); // Leer cada 2 segundos
}
