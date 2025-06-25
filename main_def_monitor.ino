#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <Wire.h>
#include <BH1750.h>
#include <Servo.h>

// ------------------ Variables Firebase --------------------
UserAuth user_auth("AIzaSyD06Vu2D_x98UJMrXcAFR_4aQl0TLWL858", "esp@salle.com", "123456789");
FirebaseApp app;
WiFiClientSecure sslClient;
using AsyncClient = AsyncClientClass;
AsyncClient asyncClient(sslClient);
RealtimeDatabase db;
AsyncResult asyncResult;

// ------------------ Pines -------------------------
const int sensorTop = 12;      // D6
const int sensorBottom = 13;   // D7
const int SDA_PIN = 4;         // D2 (I2C SDA)
const int SCL_PIN = 5;         // D1 (I2C SCL)
const int pinPulsador1 = 14;   // D5
const int pinPulsador2 = 0;    // D0
const int pinServo = 2;        // D4

// ------------------ Estados y sensores ----------------
BH1750 lightMeter;
Servo miServo;

int posiciones[] = {0, 90, 180};
int posicionActual = 0;
int posicionAnterior = 0;

bool estadoAnterior1 = HIGH;
bool estadoAnterior2 = HIGH;

// ------------------ Control de modo -------------------
enum Modo { NONE, FALL_SENSOR, LIGHT_SENSOR, SERVO_CONTROL };
Modo modoActivo = NONE;

enum StateMachine { WAIT_TOP, WAIT_BOTTOM, DETECTION, DISCARD, CLEAN };
StateMachine Actual_State = WAIT_TOP;
unsigned long momentoDeteccionSuperior = 0;
const unsigned long time_out = 3000;
const unsigned long fall_time = 1000;

// ------------------ Funciones Firebase -----------------
void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;
  if (aResult.available()) {
    Serial.printf("Datos recibidos: %s\n", aResult.c_str());
  }
  if (aResult.isError()) {
    Serial.printf("Error: %s, Código: %d\n", aResult.error().message().c_str(), aResult.error().code());
  }
}

void setFallDetected(bool value) {
  Serial.printf("Seteando fall_detected a: %s\n", value ? "true" : "false");
  db.set<bool>(asyncClient, "/fall_detected", value);
}

void setLightLevel(uint16_t level) {
  Serial.printf("Seteando light_level a: %d\n", level);
  db.set<uint16_t>(asyncClient, "/light", level);
}

void setServoPosition(int degrees) {
  Serial.printf("Seteando servo_position a: %d\n", degrees);
  db.set<int>(asyncClient, "/rotation", degrees);
}

void scanI2C() {
  Serial.println("Escaneando dispositivos I2C...");
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("Dispositivo encontrado en 0x");
      Serial.println(address, HEX);
      delay(10);
    }
  }
}

// ------------------ Internet + Firebase -----------------
void setup_internet() {
  const char* ssid = "iPhone de Carles";
  const char* password = "123456789";

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a WiFi");
  Serial.println(WiFi.localIP());

  sslClient.setInsecure(); // No SSL verify
  initializeApp(asyncClient, app, getAuth(user_auth), processData, "authTask");

  while (!app.ready()) {
    Serial.println("Esperando autenticación de Firebase...");
    delay(500);
  }

  Serial.println("Firebase autenticado correctamente.");

  app.getApp<RealtimeDatabase>(db);
  db.url("https://sensapp-8327f-default-rtdb.europe-west1.firebasedatabase.app/");
}

// ------------------ Setup inicial ----------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  setup_internet();

  Serial.println("\nSelecciona modo:");
  Serial.println("1 - Sensor de caídas");
  Serial.println("2 - Sensor de luz");
  Serial.println("3 - Control de servo");

  pinMode(sensorTop, INPUT);
  pinMode(sensorBottom, INPUT);
  pinMode(pinPulsador1, INPUT_PULLUP);
  pinMode(pinPulsador2, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  scanI2C();

  Serial.println("Inicializando BH1750...");
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23)) {
    Serial.println("Error: BH1750 no detectado en 0x23. Probando en 0x5C...");
    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C)) {
      Serial.println("Error: No se pudo iniciar el BH1750 en ninguna dirección.");
      while (true) delay(1000);
    }
  }
  Serial.println("BH1750 iniciado correctamente.");

  miServo.attach(pinServo);
}

// ------------------ Bucle principal --------------------
void loop() {
  app.loop();

  if (modoActivo == NONE && Serial.available()) {
    char seleccion = Serial.read();
    if (seleccion == '1') modoActivo = FALL_SENSOR;
    if (seleccion == '2') modoActivo = LIGHT_SENSOR;
    if (seleccion == '3') modoActivo = SERVO_CONTROL;
    Serial.print("Modo seleccionado: ");
    Serial.println((int)modoActivo);
    delay(500);
  }

  if (modoActivo == FALL_SENSOR) {
    int lecturaTop = digitalRead(sensorTop);
    int lecturaBottom = digitalRead(sensorBottom);

    switch (Actual_State) {
      case WAIT_TOP:
        if (lecturaBottom == HIGH) {
          Serial.println("Sensor BOTTOM detectado primero - ignorando");
          Actual_State = DISCARD;
        } else if (lecturaTop == HIGH) {
          Serial.println("Sensor TOP detectado");
          momentoDeteccionSuperior = millis();
          Actual_State = WAIT_BOTTOM;
        }
        break;

      case WAIT_BOTTOM:
        if (lecturaBottom == HIGH &&  millis() - momentoDeteccionSuperior < fall_time) {
          Serial.println("** CAÍDA DETECTADA **");
          setFallDetected(true);
          Actual_State = DETECTION;
        } else if (millis() - momentoDeteccionSuperior > time_out) {
          Serial.println("TIME OUT esperando sensor BOTTOM");
          Actual_State = DISCARD;
        }
        break;

      case DETECTION:
        delay(5000);
        Actual_State = CLEAN;
        break;

      case DISCARD:
        delay(1000);
        Actual_State = CLEAN;
        break;

      case CLEAN:
        momentoDeteccionSuperior = 0;
        Serial.println("Sistema limpio");
        Actual_State = WAIT_TOP;
        setFallDetected(false);
        break;
    }
  }

  if (modoActivo == LIGHT_SENSOR) {
    uint16_t brightness = lightMeter.readLightLevel();
    uint8_t ledPower = constrain(255 - (brightness * 3), 0, 255);
    Serial.printf("Luminosidad: %d lx - PWM: %d\n", brightness, ledPower);
    setLightLevel(brightness);
    delay(1000);
  }

  if (modoActivo == SERVO_CONTROL) {
    bool estadoActual1 = digitalRead(pinPulsador1);
    bool estadoActual2 = digitalRead(pinPulsador2);

    if (estadoActual1 == LOW && estadoAnterior1 == HIGH) {
      posicionActual = (posicionActual + 1) % 3;
      int nuevaPos = posiciones[posicionActual];
      Serial.printf("Botón 1 pulsado. Escribiendo en Firebase: %d°\n", nuevaPos);
      setServoPosition(nuevaPos);
    }

    if (estadoActual2 == LOW && estadoAnterior2 == HIGH) {
      Serial.println("Botón 2 pulsado. Escribiendo en Firebase: 0°");
      setServoPosition(0);
      posicionActual = 0;
    }

    estadoAnterior1 = estadoActual1;
    estadoAnterior2 = estadoActual2;

    static unsigned long lastRead = 0;
    static int ultimaPosicionFirebase = -1;

    if (millis() - lastRead > 500) {
    lastRead = millis();

    int posRemota = db.get<int>(asyncClient, "/rotation");
    // posRemota será 0 si no hay valor o error (depende de la librería)

    if (posRemota != ultimaPosicionFirebase) {
      Serial.printf("Cambio remoto en Firebase detectado: %d°\n", posRemota);
      miServo.write(posRemota);
      ultimaPosicionFirebase = posRemota;

      for (int i = 0; i < 3; i++) {
        if (posiciones[i] == posRemota) {
          posicionActual = i;
          break;
        }
      }
    }
  }
}
}