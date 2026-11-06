#include <OneWire.h>
#include <DallasTemperature.h>
#include <Servo.h>

const uint8_t PIN_SENSOR = 2;   // OUT del sensor
const uint8_t PIN_SERVO  = 9;   // cable amarillo del servo

const float TEMP_ALTA = 30.0;   // sobre esto, gira a la derecha
const float TEMP_BAJA = 23.0;   // en este valor o menos, gira a la izquierda

// Tiempo que tarda tu servo en girar 180 grados (ajústalo probando)
const unsigned long TIEMPO_180_MS = 1000;

const int SERVO_STOP      = 1500;  // detenido (ajusta 1490-1510 si se mueve solo)
const int SERVO_DERECHA   = 1350;  // si gira al revés, intercambia los dos valores
const int SERVO_IZQUIERDA = 1650;

OneWire oneWire(PIN_SENSOR);
DallasTemperature sensor(&oneWire);
Servo servo;

bool girado = false;

void girar(int velocidad) {
  servo.writeMicroseconds(velocidad);
  delay(TIEMPO_180_MS);
  servo.writeMicroseconds(SERVO_STOP);
}

void setup() {
  Serial.begin(115200);
  sensor.begin();
  servo.attach(PIN_SERVO);
  servo.writeMicroseconds(SERVO_STOP);
  Serial.println("Prueba iniciada");
}

void loop() {
  sensor.requestTemperatures();
  float t = sensor.getTempCByIndex(0);

  if (t == DEVICE_DISCONNECTED_C) {
    Serial.println("ERROR: sensor no detectado, revisa las conexiones");
    delay(1000);
    return;
  }

  Serial.print("Temperatura: ");
  Serial.print(t, 2);
  Serial.println(" C");

  if (!girado && t > TEMP_ALTA) {
    Serial.println(">> Girando 180 grados a la derecha");
    girar(SERVO_DERECHA);
    girado = true;
  } else if (girado && t <= TEMP_BAJA) {
    Serial.println(">> Girando 180 grados a la izquierda");
    girar(SERVO_IZQUIERDA);
    girado = false;
  }

  delay(1000); //Tiempo de medicion
}
