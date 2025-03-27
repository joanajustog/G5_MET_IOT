#include <Servo.h>

Servo miServo;

// Pines de los pulsadores
const int pinPulsador1 = 14;      // Cambiar posición
const int pinPulsador2 = 12;      // Volver a 0°

const int pinServo = 2;           // Pin de control del servo SG90

// Posiciones en grados
int posiciones[] = {0, 90, 180};  // 0°, 90°, 180°
int posicionActual = 0;           // Índice de la posición actual
int posicionAnterior = 0;         // Para saber desde dónde se vuelve a 0°

// Variables para antirrebote
bool estadoAnterior1 = HIGH;
bool estadoAnterior2 = HIGH;

void setup() {
  miServo.attach(pinServo);
  pinMode(pinPulsador1, INPUT_PULLUP);
  pinMode(pinPulsador2, INPUT_PULLUP);
  miServo.write(posiciones[posicionActual]);          // Iniciar en 0°
}

void loop() {
  // Lectura de los pulsadores
  bool estadoActual1 = digitalRead(pinPulsador1);
  bool estadoActual2 = digitalRead(pinPulsador2);

  // Pulsador 1: Cambiar posición secuencial
  if (estadoActual1 == LOW && estadoAnterior1 == HIGH) {
    avanzarPosicion();
    delay(200);           // Antirrebote
  }

  // Pulsador 2: Volver a 0° desde cualquier posición
  if (estadoActual2 == LOW && estadoAnterior2 == HIGH) {
    volverA0();
    delay(200);           // Antirrebote
  }

  // Actualizar estados
  estadoAnterior1 = estadoActual1;
  estadoAnterior2 = estadoActual2;
}

void avanzarPosicion() {
  // Guardar la posición anterior antes de cambiar
  posicionAnterior = posicionActual;

  // Avanzar a la siguiente posición
  posicionActual = (posicionActual + 1) % 3;
  miServo.write(posiciones[posicionActual]);
  delay(500);           // Tiempo para que el servo complete el movimiento
}

void volverA0() {
  if (posicionActual != 0) {
    miServo.write(0);   // Mover a 0°
   
    // Ajustar el tiempo de delay según la posición anterior
    if (posicionAnterior == 1) {
      delay(500);       // Desde 90°
    } else if (posicionAnterior == 2) {
      delay(1000);      // Desde 180°
    }

    posicionActual = 0; // Resetear la posición
  }
}