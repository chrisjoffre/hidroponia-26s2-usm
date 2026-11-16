// =====================================================================
//  TORRE HIDROPONICA - Arduino UNO R4 WiFi
//
//  - Servo (rotacion continua): mueve la malla UV
//        'h' = ABRIR  (giro horario)
//        'a' = CERRAR (giro antihorario)
//  - Sensor de temperatura AMBIENTAL (DS18B20) -> controla la malla:
//        temp  > 27 C  -> CIERRA la malla
//        temp <= 22 C  -> ABRE la malla
//        entre 22 y 27 -> mantiene el estado actual (histeresis)
//  - Sensor de temperatura SUMERGIBLE (DS18B20) -> se muestra en la pantalla
//  - Bomba sumergible: ciclo de riego (15 s encendida, 60 s apagada)
//  - Pantalla OLED 0.96" I2C (SSD1306 128x64): temp. agua, temp. aire,
//    estado de la malla y de la bomba
//
//  Monitor serie: 115200 baud, fin de linea "Nueva linea". Escribe ? para ayuda.
//
//  Librerias (Gestor de librerias): OneWire, DallasTemperature,
//  Adafruit SSD1306 y Adafruit GFX Library. Servo y Wire ya vienen incluidas.
// =====================================================================

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================== PINES ==================
const uint8_t PIN_SENSOR_AIRE = 2;   // DS18B20 ambiental (el mismo pin de tu codigo anterior)
const uint8_t PIN_SENSOR_AGUA = 3;   // DS18B20 sumergible
const uint8_t PIN_SERVO       = 9;   // cable amarillo del servo
const uint8_t PIN_BOMBA       = 7;   // al pin IN del modulo rele (NO directo a la bomba)
// Tu modulo (1 canal, 5 V, con transistor y sin optoacoplador) se activa con HIGH.
// Si el rele queda al reves (bomba encendida cuando deberia estar apagada), cambia a false.
const bool BOMBA_ACTIVA_EN_HIGH = true;

// Pantalla OLED 0.96" I2C SSD1306 (SDA = A4, SCK/SCL = A5 en la R4 WiFi).
const uint8_t OLED_DIRECCION = 0x3C;   // si no muestra nada prueba 0x3D
const uint8_t OLED_ANCHO     = 128;
const uint8_t OLED_ALTO      = 64;

// ================== CONTROL DE TEMPERATURA ==================
const float TEMP_CERRAR = 27.0;   // sobre esto (>) se cierra la malla
const float TEMP_ABRIR  = 24.0;   // en este valor o menos (<=) se abre la malla
const uint8_t LECTURAS_CONFIRMACION = 3;   // lecturas seguidas para actuar (evita falsas alarmas)

const unsigned long PERIODO_MEDICION_MS = 2000;   // cada cuanto se miden las temperaturas
const unsigned long TIEMPO_CONVERSION_MS = 800;   // el DS18B20 tarda ~750 ms en medir (12 bits)

// ================== PARAMETROS DEL SERVO (modificables por Serial) ==================
int SERVO_STOP = 1500;                         // detenido (compartido por ambos giros)
int VEL_HORARIO = 1300;                        // pulso para giro horario    (ABRIR)
int VEL_ANTIHORARIO = 1650;                    // pulso para giro antihorario (CERRAR)
unsigned long TIEMPO_HORARIO_MS = 3100;        // duracion del giro horario rapido (ABRIR)
unsigned long TIEMPO_ANTIHORARIO_MS = 3550;    // duracion del giro antihorario (CERRAR). Subelo si queda un hueco, bajalo si se pasa

// Al ABRIR, el final del recorrido se hace despacio ("aterrizaje suave") para que el servo
// no empuje con fuerza contra el tope.
int VEL_HORARIO_SUAVE = 1400;                  // pulso lento horario (entre 1300 y SERVO_STOP; mas cerca de 1500 = mas suave)
unsigned long TIEMPO_SUAVE_MS = 900;           // duracion de la fase lenta final al abrir (0 = sin fase suave)

// ================== PARAMETROS DE LA BOMBA (modificables por Serial) ==================
unsigned long BOMBA_ON_MS  = 60UL * 1000UL;   // 60 s encendida
unsigned long BOMBA_OFF_MS = 15UL * 1000UL;   // 15 s apagada

// ================== OBJETOS ==================
OneWire busAire(PIN_SENSOR_AIRE);
OneWire busAgua(PIN_SENSOR_AGUA);
DallasTemperature sensorAire(&busAire);
DallasTemperature sensorAgua(&busAgua);
Servo servo;
Adafruit_SSD1306 oled(OLED_ANCHO, OLED_ALTO, &Wire, -1);
bool oledOk = false;

// ================== ESTADO ==================
enum EstadoMalla { MALLA_ABIERTA, MALLA_CERRADA };

// Al encender se asume la malla ABIERTA. Si la dejaste cerrada, usa el comando "fija cerrada".
EstadoMalla estadoMalla = MALLA_ABIERTA;
bool servoAuto = true;          // false mientras uses h / a a mano
bool servoMoviendo = false;
EstadoMalla servoDestino = MALLA_ABIERTA;
unsigned long servoInicio = 0;
unsigned long servoDuracion = 0;
bool servoFaseSuave = false;    // true durante el tramo final lento al abrir

bool bombaAuto = true;          // false mientras uses bon / boff a mano
bool bombaEncendida = false;
unsigned long tBomba = 0;       // instante del ultimo cambio de estado de la bomba

float tempAire = 0, tempAgua = 0;
bool aireValida = false, aguaValida = false;
uint8_t contConfirmacion = 0;

unsigned long tUltimaMedicion = 0;
unsigned long tSolicitud = 0;
bool esperandoConversion = false;
unsigned long tUltimaPantalla = 0;

// ================== SERVO ==================
const char* nombreMalla(EstadoMalla e) {
  return (e == MALLA_ABIERTA) ? "ABIERTA" : "CERRADA";
}

// Inicia el giro sin bloquear el programa. Devuelve false si ya se estaba moviendo.
bool iniciarMovimiento(EstadoMalla destino) {
  if (servoMoviendo) return false;
  servoDestino = destino;
  if (destino == MALLA_ABIERTA) {
    servo.writeMicroseconds(VEL_HORARIO);        // horario = abrir
    servoDuracion = TIEMPO_HORARIO_MS;
  } else {
    servo.writeMicroseconds(VEL_ANTIHORARIO);    // antihorario = cerrar
    servoDuracion = TIEMPO_ANTIHORARIO_MS;
  }
  servoFaseSuave = false;
  servoInicio = millis();
  servoMoviendo = true;
  return true;
}

void actualizarServo() {
  if (!servoMoviendo) return;
  if (millis() - servoInicio < servoDuracion) return;

  // Termino el tramo rapido de ABRIR: sigue un tramo lento hasta el tope (poca fuerza)
  if (servoDestino == MALLA_ABIERTA && !servoFaseSuave && TIEMPO_SUAVE_MS > 0) {
    servo.writeMicroseconds(VEL_HORARIO_SUAVE);
    servoFaseSuave = true;
    servoInicio = millis();
    servoDuracion = TIEMPO_SUAVE_MS;
    return;
  }

  servo.writeMicroseconds(SERVO_STOP);
  servoMoviendo = false;
  servoFaseSuave = false;
  estadoMalla = servoDestino;
  Serial.print(">> Malla ");
  Serial.println(nombreMalla(estadoMalla));
}

// ================== BOMBA ==================
void bombaEncender() {
  digitalWrite(PIN_BOMBA, BOMBA_ACTIVA_EN_HIGH ? HIGH : LOW);
  bombaEncendida = true;
  tBomba = millis();
}

void bombaApagar() {
  digitalWrite(PIN_BOMBA, BOMBA_ACTIVA_EN_HIGH ? LOW : HIGH);
  bombaEncendida = false;
  tBomba = millis();
}

void controlBomba() {
  if (!bombaAuto) return;
  unsigned long ahora = millis();
  if (bombaEncendida) {
    if (ahora - tBomba >= BOMBA_ON_MS) {
      bombaApagar();
      Serial.println(">> Bomba OFF (fin del riego)");
    }
  } else {
    if (ahora - tBomba >= BOMBA_OFF_MS) {
      bombaEncender();
      Serial.println(">> Bomba ON (inicio del riego)");
    }
  }
}

// ================== SENSORES ==================
bool lecturaValida(float t) {
  // -127 = sensor desconectado; 85.0 = valor de arranque del DS18B20 (aun sin medir)
  return (t != DEVICE_DISCONNECTED_C) && (t != 85.0);
}

void controlMalla() {
  if (!servoAuto || servoMoviendo) {
    contConfirmacion = 0;
    return;
  }
  if (!aireValida) {
    contConfirmacion = 0;   // sin dato confiable no se mueve la malla
    return;
  }

  bool pedirCerrar = (estadoMalla == MALLA_ABIERTA && tempAire > TEMP_CERRAR);
  bool pedirAbrir  = (estadoMalla == MALLA_CERRADA && tempAire <= TEMP_ABRIR);

  if (pedirCerrar || pedirAbrir) {
    contConfirmacion++;
    if (contConfirmacion >= LECTURAS_CONFIRMACION) {
      contConfirmacion = 0;
      if (pedirCerrar) {
        Serial.println(">> Temperatura alta: CERRANDO malla (antihorario)");
        iniciarMovimiento(MALLA_CERRADA);
      } else {
        Serial.println(">> Temperatura buena: ABRIENDO malla (horario)");
        iniciarMovimiento(MALLA_ABIERTA);
      }
    }
  } else {
    contConfirmacion = 0;
  }
}

void imprimirEstado() {
  Serial.print("Aire: ");
  if (aireValida) { Serial.print(tempAire, 1); Serial.print(" C"); } else Serial.print("ERROR");
  Serial.print(" | Agua: ");
  if (aguaValida) { Serial.print(tempAgua, 1); Serial.print(" C"); } else Serial.print("ERROR");
  Serial.print(" | Malla: ");
  Serial.print(servoMoviendo ? "MOVIENDO" : nombreMalla(estadoMalla));
  Serial.print(" | Bomba: ");
  Serial.println(bombaEncendida ? "ON" : "OFF");
}

// Pide la medicion y, ~800 ms despues, la lee (sin bloquear el loop).
void leerSensores() {
  unsigned long ahora = millis();

  if (!esperandoConversion) {
    if (ahora - tUltimaMedicion >= PERIODO_MEDICION_MS) {
      sensorAire.requestTemperatures();
      sensorAgua.requestTemperatures();
      tSolicitud = ahora;
      esperandoConversion = true;
    }
    return;
  }

  if (ahora - tSolicitud >= TIEMPO_CONVERSION_MS) {
    float a = sensorAire.getTempCByIndex(0);
    float w = sensorAgua.getTempCByIndex(0);
    aireValida = lecturaValida(a);
    aguaValida = lecturaValida(w);
    if (aireValida) tempAire = a;
    if (aguaValida) tempAgua = w;

    esperandoConversion = false;
    tUltimaMedicion = ahora;

    imprimirEstado();
    controlMalla();
  }
}

// ================== PANTALLA OLED ==================
// Escribe la temperatura como "25.3" (o "--.-" si no hay lectura).
void formatearTemp(char* buf, size_t n, float t, bool valida) {
  if (!valida) {
    snprintf(buf, n, "--.-");
    return;
  }
  int v = (int)(t * 10.0 + (t >= 0 ? 0.5 : -0.5));
  bool neg = v < 0;
  if (neg) v = -v;
  snprintf(buf, n, "%s%d.%d", neg ? "-" : "", v / 10, v % 10);
}

void actualizarPantalla() {
  if (!oledOk) return;
  unsigned long ahora = millis();
  if (ahora - tUltimaPantalla < 500) return;
  tUltimaPantalla = ahora;

  char tAguaTxt[8], tAireTxt[8], linea[24];
  formatearTemp(tAguaTxt, sizeof(tAguaTxt), tempAgua, aguaValida);
  formatearTemp(tAireTxt, sizeof(tAireTxt), tempAire, aireValida);

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // Temperaturas en letra grande
  oled.setTextSize(2);
  snprintf(linea, sizeof(linea), "Agua %sC", tAguaTxt);
  oled.setCursor(0, 0);
  oled.print(linea);
  snprintf(linea, sizeof(linea), "Aire %sC", tAireTxt);
  oled.setCursor(0, 20);
  oled.print(linea);

  // Estados en letra chica
  oled.setTextSize(1);
  oled.setCursor(0, 44);
  oled.print("Malla: ");
  oled.print(servoMoviendo ? "MOVIENDO" : nombreMalla(estadoMalla));
  oled.setCursor(0, 56);
  oled.print("Bomba: ");
  oled.print(bombaEncendida ? "OFF" : "ON");

  oled.display();
}

// ================== COMANDOS POR SERIAL ==================
void mostrarAyuda() {
  Serial.println();
  Serial.println("===== TORRE HIDROPONICA =====");
  Serial.println("(Monitor serie a 115200, fin de linea: 'Nueva linea')");
  Serial.println("--- Malla (servo) ---");
  Serial.println("h            -> ABRIR malla (horario)   [pasa a modo manual]");
  Serial.println("a            -> CERRAR malla (antihorario) [pasa a modo manual]");
  Serial.println("fija abierta -> dice que la malla YA esta abierta (sin mover)");
  Serial.println("fija cerrada -> dice que la malla YA esta cerrada (sin mover)");
  Serial.println("--- Calibracion servo ---");
  Serial.println("stop N       -> fija el pulso de STOP (ej: stop 1495)");
  Serial.println("+  /  -      -> STOP +1 / -1 us (para encontrar el punto muerto)");
  Serial.println("vh N         -> pulso horario (ej: vh 1300)");
  Serial.println("va N         -> pulso antihorario (ej: va 1650)");
  Serial.println("th N         -> tiempo horario en ms (ej: th 3100)");
  Serial.println("ta N         -> tiempo antihorario (CERRAR) en ms (ej: ta 3300)");
  Serial.println("vs N         -> pulso lento al final de ABRIR (ej: vs 1400, menor que STOP)");
  Serial.println("ts N         -> ms de la fase lenta al ABRIR (ej: ts 900, 0 = sin fase lenta)");
  Serial.println("--- Bomba ---");
  Serial.println("bon / boff   -> enciende / apaga la bomba a mano [pasa a modo manual]");
  Serial.println("pon N        -> segundos ENCENDIDA en cada riego (ej: pon 15)");
  Serial.println("poff N       -> segundos APAGADA entre riegos (ej: poff 60)");
  Serial.println("--- General ---");
  Serial.println("auto         -> vuelve malla y bomba a modo automatico");
  Serial.println("p            -> muestra los valores actuales");
  Serial.println("?            -> esta ayuda");
  Serial.println("=============================");
}

void mostrarValores() {
  Serial.println("--- Valores actuales ---");
  Serial.print("const float TEMP_CERRAR = "); Serial.print(TEMP_CERRAR, 1); Serial.println(";");
  Serial.print("const float TEMP_ABRIR  = "); Serial.print(TEMP_ABRIR, 1); Serial.println(";");
  Serial.print("int SERVO_STOP      = "); Serial.print(SERVO_STOP); Serial.println(";");
  Serial.print("int VEL_HORARIO     = "); Serial.print(VEL_HORARIO); Serial.println(";");
  Serial.print("int VEL_ANTIHORARIO = "); Serial.print(VEL_ANTIHORARIO); Serial.println(";");
  Serial.print("unsigned long TIEMPO_HORARIO_MS     = "); Serial.print(TIEMPO_HORARIO_MS); Serial.println(";");
  Serial.print("unsigned long TIEMPO_ANTIHORARIO_MS = "); Serial.print(TIEMPO_ANTIHORARIO_MS); Serial.println(";");
  Serial.print("int VEL_HORARIO_SUAVE = "); Serial.print(VEL_HORARIO_SUAVE); Serial.println(";");
  Serial.print("unsigned long TIEMPO_SUAVE_MS = "); Serial.print(TIEMPO_SUAVE_MS); Serial.println(";");
  Serial.print("Bomba: ON "); Serial.print(BOMBA_ON_MS / 1000UL); Serial.print(" s / OFF ");
  Serial.print(BOMBA_OFF_MS / 1000UL); Serial.println(" s");
  Serial.print("Malla: "); Serial.print(nombreMalla(estadoMalla));
  Serial.print(" | modo malla: "); Serial.print(servoAuto ? "AUTO" : "MANUAL");
  Serial.print(" | modo bomba: "); Serial.println(bombaAuto ? "AUTO" : "MANUAL");
  Serial.println("------------------------");
}

void aplicarStop() {
  servo.writeMicroseconds(SERVO_STOP);
  Serial.print("STOP = ");
  Serial.println(SERVO_STOP);
}

void procesarComando(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  if (cmd == "h") {
    servoAuto = false;
    if (iniciarMovimiento(MALLA_ABIERTA)) Serial.println(">> Abriendo malla (horario)...");
    else Serial.println("El servo todavia se esta moviendo");
  } else if (cmd == "a") {
    servoAuto = false;
    if (iniciarMovimiento(MALLA_CERRADA)) Serial.println(">> Cerrando malla (antihorario)...");
    else Serial.println("El servo todavia se esta moviendo");
  } else if (cmd == "fija abierta") {
    estadoMalla = MALLA_ABIERTA;
    Serial.println("Estado de malla fijado: ABIERTA");
  } else if (cmd == "fija cerrada") {
    estadoMalla = MALLA_CERRADA;
    Serial.println("Estado de malla fijado: CERRADA");
  } else if (cmd == "auto") {
    servoAuto = true;
    bombaAuto = true;
    contConfirmacion = 0;
    if (!bombaEncendida) tBomba = millis();   // reinicia la espera del ciclo
    Serial.println("Modo AUTOMATICO (malla y bomba)");
  } else if (cmd == "bon") {
    bombaAuto = false;
    bombaEncender();
    Serial.println(">> Bomba ON (manual)");
  } else if (cmd == "boff") {
    bombaAuto = false;
    bombaApagar();
    Serial.println(">> Bomba OFF (manual)");
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
  } else if (cmd.startsWith("vs ")) {
    int v = constrain(cmd.substring(3).toInt(), 1000, 2000);
    if (v >= SERVO_STOP) {
      Serial.println("Valor invalido: debe ser menor que SERVO_STOP (giro horario).");
    } else {
      VEL_HORARIO_SUAVE = v;
      Serial.print("VEL_HORARIO_SUAVE = "); Serial.println(VEL_HORARIO_SUAVE);
    }
  } else if (cmd.startsWith("ts ")) {
    TIEMPO_SUAVE_MS = max(0L, cmd.substring(3).toInt());
    Serial.print("TIEMPO_SUAVE_MS = "); Serial.println(TIEMPO_SUAVE_MS);
  } else if (cmd.startsWith("pon ")) {
    BOMBA_ON_MS = (unsigned long)max(1L, cmd.substring(4).toInt()) * 1000UL;
    Serial.print("Bomba encendida (s) = "); Serial.println(BOMBA_ON_MS / 1000UL);
  } else if (cmd.startsWith("poff ")) {
    BOMBA_OFF_MS = (unsigned long)max(1L, cmd.substring(5).toInt()) * 1000UL;
    Serial.print("Bomba apagada (s) = "); Serial.println(BOMBA_OFF_MS / 1000UL);
  } else {
    Serial.println("Comando no reconocido. Escribe ? para ver la ayuda.");
  }
}

void leerSerial() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    procesarComando(cmd);
  }
}

// ================== SETUP / LOOP ==================
void setup() {
  // Bomba apagada ANTES que cualquier otra cosa
  pinMode(PIN_BOMBA, OUTPUT);
  digitalWrite(PIN_BOMBA, BOMBA_ACTIVA_EN_HIGH ? LOW : HIGH);

  Serial.begin(115200);
  Serial.setTimeout(100);

  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_DIRECCION);
  if (oledOk) {
    Wire.setClock(400000);   // I2C rapido: refrescar la pantalla tarda menos
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println("Torre hidroponica");
    oled.println("Iniciando...");
    oled.display();
  } else {
    Serial.println("AVISO: no se encontro la pantalla OLED (revisa SDA/SCK y la direccion 0x3C). El sistema sigue funcionando sin pantalla.");
  }

  sensorAire.begin();
  sensorAgua.begin();
  sensorAire.setWaitForConversion(false);   // lectura sin bloquear
  sensorAgua.setWaitForConversion(false);

  servo.attach(PIN_SERVO);
  servo.writeMicroseconds(SERVO_STOP);

  tBomba = millis();   // la primera bomba parte despues de BOMBA_OFF_MS
  mostrarAyuda();
  mostrarValores();
}

void loop() {
  leerSensores();     // mide aire y agua, y decide si mover la malla
  actualizarServo();  // detiene el servo cuando termina el giro
  controlBomba();     // ciclo de riego
  actualizarPantalla();  // refresca la pantalla OLED
  leerSerial();       // comandos por el monitor serie
}
