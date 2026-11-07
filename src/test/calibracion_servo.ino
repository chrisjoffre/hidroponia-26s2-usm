//h: abrir (horario).    a: cerrar (antihorario)

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Servo.h>

// ================== PINES ==================
const uint8_t PIN_SENSOR = 2;   // OUT del sensor
const uint8_t PIN_SERVO  = 9;   // cable amarillo del servo

// ================== TEMPERATURA (solo para controlTemperatura) ==================
const float TEMP_ALTA = 30.0;   // sobre esto, gira horario
const float TEMP_BAJA = 23.0;   // en este valor o menos, gira antihorario

// ================== PARAMETROS DEL SERVO (modificables por Serial) ==================
int SERVO_STOP = 1500;            // detenido (compartido por ambos giros)
int VEL_HORARIO = 1300;           // pulso para giro horario
int VEL_ANTIHORARIO = 1650;       // pulso para giro antihorario
unsigned long TIEMPO_HORARIO_MS = 3100;      // duración del giro de "180" horario
unsigned long TIEMPO_ANTIHORARIO_MS = 3000;  // duración del giro de "180" antihorario

OneWire oneWire(PIN_SENSOR);
DallasTemperature sensor(&oneWire);
Servo servo;

bool girado = false;

// ================== FUNCIONES DE GIRO ==================
void girar180Horario() {
  servo.writeMicroseconds(VEL_HORARIO);
  delay(TIEMPO_HORARIO_MS);
  servo.writeMicroseconds(SERVO_STOP);
}

void girar180Antihorario() {
  servo.writeMicroseconds(VEL_ANTIHORARIO);
  delay(TIEMPO_ANTIHORARIO_MS);
  servo.writeMicroseconds(SERVO_STOP);
}

// ================== CODIGO ANTERIOR (NO SE LLAMA) ==================
// Para usarlo: llama controlTemperatura() desde loop() en vez de calibracion().
void controlTemperatura() {
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
    Serial.println(">> Girando 180 grados horario");
    girar180Horario();
    girado = true;
  } else if (girado && t <= TEMP_BAJA) {
    Serial.println(">> Girando 180 grados antihorario");
    girar180Antihorario();
    girado = false;
  }

  delay(1000); // Tiempo de medicion
}

// ================== CALIBRACION POR SERIAL ==================
void mostrarAyuda() {
  Serial.println();
  Serial.println("===== CALIBRACION SERVO =====");
  Serial.println("(Monitor serie a 115200, fin de linea: 'Nueva linea')");
  Serial.println("h          -> gira 180 horario");
  Serial.println("a          -> gira 180 antihorario");
  Serial.println("stop N     -> fija el pulso de STOP (ej: stop 1495)");
  Serial.println("+  /  -    -> STOP +1 / -1 us (para encontrar el punto muerto)");
  Serial.println("vh N       -> pulso horario (ej: vh 1350)");
  Serial.println("va N       -> pulso antihorario (ej: va 1650)");
  Serial.println("th N       -> tiempo horario en ms (ej: th 1200)");
  Serial.println("ta N       -> tiempo antihorario en ms (ej: ta 1100)");
  Serial.println("p          -> muestra los valores actuales");
  Serial.println("?          -> esta ayuda");
  Serial.println("=============================");
}

void mostrarValores() {
  Serial.println("--- Valores actuales ---");
  Serial.print("const int SERVO_STOP      = "); Serial.print(SERVO_STOP); Serial.println(";");
  Serial.print("const int VEL_HORARIO     = "); Serial.print(VEL_HORARIO); Serial.println(";");
  Serial.print("const int VEL_ANTIHORARIO = "); Serial.print(VEL_ANTIHORARIO); Serial.println(";");
  Serial.print("const unsigned long TIEMPO_HORARIO_MS     = "); Serial.print(TIEMPO_HORARIO_MS); Serial.println(";");
  Serial.print("const unsigned long TIEMPO_ANTIHORARIO_MS = "); Serial.print(TIEMPO_ANTIHORARIO_MS); Serial.println(";");
  Serial.println("------------------------");
}

void aplicarStop() {
  servo.writeMicroseconds(SERVO_STOP);
  Serial.print("STOP = ");
  Serial.println(SERVO_STOP);
}

void procesarComando(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "h") {
    Serial.println(">> Girando 180 horario");
    girar180Horario();
    Serial.println("Listo");
  } else if (cmd == "a") {
    Serial.println(">> Girando 180 antihorario");
    girar180Antihorario();
    Serial.println("Listo");
  } else if (cmd == "+") {
    SERVO_STOP++;
    aplicarStop();
  } else if (cmd == "-") {
    SERVO_STOP--;
    aplicarStop();
  } else if (cmd == "p") {
    mostrarValores();
  } else if (cmd == "?") {
    mostrarAyuda();
  } else if (cmd.startsWith("stop ")) {
    SERVO_STOP = constrain(cmd.substring(5).toInt(), 1000, 2000);
    aplicarStop();
  } else if (cmd.startsWith("vh ")) {
    VEL_HORARIO = constrain(cmd.substring(3).toInt(), 1000, 2000);
    Serial.print("VEL_HORARIO = "); Serial.println(VEL_HORARIO);
  } else if (cmd.startsWith("va ")) {
    VEL_ANTIHORARIO = constrain(cmd.substring(3).toInt(), 1000, 2000);
    Serial.print("VEL_ANTIHORARIO = "); Serial.println(VEL_ANTIHORARIO);
  } else if (cmd.startsWith("th ")) {
    TIEMPO_HORARIO_MS = max(0L, cmd.substring(3).toInt());
    Serial.print("TIEMPO_HORARIO_MS = "); Serial.println(TIEMPO_HORARIO_MS);
  } else if (cmd.startsWith("ta ")) {
    TIEMPO_ANTIHORARIO_MS = max(0L, cmd.substring(3).toInt());
    Serial.print("TIEMPO_ANTIHORARIO_MS = "); Serial.println(TIEMPO_ANTIHORARIO_MS);
  } else {
    Serial.println("Comando no reconocido. Escribe ? para ver la ayuda.");
  }
}

void calibracion() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    procesarComando(cmd);
  }
}

// ================== SETUP / LOOP ==================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  sensor.begin();
  servo.attach(PIN_SERVO);
  servo.writeMicroseconds(SERVO_STOP);
  mostrarAyuda();
  mostrarValores();
}

void loop() {
  calibracion();
  // controlTemperatura();   // <- descomenta esto (y comenta calibracion()) cuando termines de calibrar
}
