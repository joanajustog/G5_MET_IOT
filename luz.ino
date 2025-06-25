#include <Wire.h>
#include <BH1750.h>

#define LED_PIN 2   // GPIO2 (D4 en NodeMCU)
#define SDA_PIN 4   // GPIO4 (D2 en NodeMCU)
#define SCL_PIN 5   // GPIO5 (D1 en NodeMCU)

BH1750 lightMeter;
uint8_t ledPower = 0;
uint16_t brightness;

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

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  scanI2C(); // Escanea dispositivos I2C y muestra su dirección en el Serial Monitor

  Serial.println("Inicializando BH1750...");
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23)) {
    Serial.println("Error: BH1750 no detectado en 0x23. Probando en 0x5C...");
    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C)) {
      Serial.println("Error: No se pudo iniciar el BH1750 en ninguna dirección.");
      while (1); // Detiene el programa si el sensor no responde
    }
  }

  Serial.println("BH1750 iniciado correctamente.");
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  brightness = lightMeter.readLightLevel();
  
  // Ajustar el brillo del LED (más luz -> menos brillo)
  ledPower = constrain(255 - (brightness * 3), 0, 255);

  Serial.print("Luminosidad: ");
  Serial.print(brightness);
  Serial.print(" lx - PWM: ");
  Serial.println(ledPower);

  analogWrite(LED_PIN, ledPower);
  delay(1000);
}
