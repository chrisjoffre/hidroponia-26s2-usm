//Sensor de temperatura sumergible
#include <OneWire.h>
#include <DallasTemperature.h>

const int oneWireBus = 2; 
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(9600);
  sensors.begin();
  Serial.println("Lector de temperatura iniciado.");
}

void loop() {
  sensors.requestTemperatures(); 

  // Lee la temperatura en grados Celsius del primer sensor encontrado
  float temperaturaC = sensors.getTempCByIndex(0);

  // Verifica si el sensor está bien conectado
  if (temperaturaC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: No se pudo leer del sensor DS18B20. Revisa las conexiones.");
  } else {
    Serial.print("Temperatura del agua: ");
    Serial.print(temperaturaC);
    Serial.println(" °C");
  }
  delay(1000);
}
