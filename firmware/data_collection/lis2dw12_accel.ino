#include <Wire.h>
#define LIS2_ADDR       0x19

// ---------------------------------------------------------------------------
// Parametri campionamento
// ---------------------------------------------------------------------------
#define ACCEL_ODR_HZ    800
#define ACCEL_SAMPLES   256
#define ACCEL_PERIOD_US (1000000UL / ACCEL_ODR_HZ)  // 1250 µs

// ---------------------------------------------------------------------------
// Registri LIS2DW12
// ---------------------------------------------------------------------------
#define LIS2_WHO_AM_I   0x0F
#define LIS2_CTRL1      0x20
#define LIS2_CTRL2      0x21
#define LIS2_CTRL6      0x25
#define LIS2_STATUS     0x27
#define LIS2_OUT_X_L    0x28

// CTRL1: ODR=800Hz (1001b), High-Performance mode (MODE=00)
#define LIS2_CTRL1_VAL  0x90
// CTRL2: BDU=1, IF_ADD_INC=1
#define LIS2_CTRL2_VAL  0x08
// CTRL6: FS=±4g
#define LIS2_CTRL6_VAL  0x10

// Sensibilità 14-bit a ±4g: 0.488 mg/LSB
#define LIS2_SENS       0.000488f
#define LIS2_SHIFT      2

// ---------------------------------------------------------------------------
// Pin di debug per oscilloscopio
// HIGH durante la lettura del campione → misura frequenza e duty I²C
// ---------------------------------------------------------------------------
#define DEBUG_PIN       D0

// ---------------------------------------------------------------------------
// Buffer
// ---------------------------------------------------------------------------
float ax[ACCEL_SAMPLES], ay[ACCEL_SAMPLES], az[ACCEL_SAMPLES];

// ---------------------------------------------------------------------------
// Prototipi
// ---------------------------------------------------------------------------
void    lis2_init();
void    lis2_write(uint8_t reg, uint8_t val);
uint8_t lis2_read(uint8_t reg);
void    lis2_read_xyz(int16_t &x, int16_t &y, int16_t &z);
bool    lis2_data_ready();

// =============================================================================
void setup() {
  Serial.begin(921600);
  while (!Serial);

  Wire.begin();
  Wire.setClock(400000);  // I2C fast mode 400 kHz
  delay(10);

  pinMode(DEBUG_PIN, OUTPUT);
  digitalWrite(DEBUG_PIN, LOW);

  uint8_t whoami = lis2_read(LIS2_WHO_AM_I);
  if (whoami != 0x44) {
    Serial.print("# ERRORE WHO_AM_I: 0x");
    Serial.print(whoami, HEX);
    Serial.println(" (atteso 0x44)");
    while (1);
  }
  Serial.println("# LIS2DW12 OK");

  lis2_init();
  Serial.print("# ODR=800Hz | FS=±4g | campioni per burst=");
  Serial.println(ACCEL_SAMPLES);
}

// =============================================================================
void loop() {
  int16_t rx, ry, rz;

  for (int i = 0; i < ACCEL_SAMPLES; i++) {
    uint32_t t0 = micros();

    uint32_t timeout = t0 + ACCEL_PERIOD_US * 2;
    while (!lis2_data_ready()) {
      if (micros() > timeout) break;
    }

    lis2_read_xyz(rx, ry, rz);

    digitalWrite(DEBUG_PIN, HIGH);   // ← fronte di salita: campione acquisito
    ax[i] = (float)(rx >> LIS2_SHIFT) * LIS2_SENS;
    ay[i] = (float)(ry >> LIS2_SHIFT) * LIS2_SENS;
    az[i] = (float)(rz >> LIS2_SHIFT) * LIS2_SENS;
    digitalWrite(DEBUG_PIN, LOW);    // ← fronte di discesa: fine elaborazione

    while ((micros() - t0) < ACCEL_PERIOD_US);
  }

  // Output NanoEdge AI Studio: una riga per burst
  for (int i = 0; i < ACCEL_SAMPLES; i++) {
    Serial.print(ax[i], 4); Serial.print(' ');
    Serial.print(ay[i], 4); Serial.print(' ');
    Serial.print(az[i], 4);
    if (i < ACCEL_SAMPLES - 1) Serial.print(' ');
  }
  Serial.println();
}

// =============================================================================
// Driver LIS2DW12 via I2C
// =============================================================================
void lis2_init() {
  lis2_write(LIS2_CTRL2, 0x40);  // soft reset
  delay(5);
  lis2_write(LIS2_CTRL6, LIS2_CTRL6_VAL);
  lis2_write(LIS2_CTRL2, LIS2_CTRL2_VAL);
  lis2_write(LIS2_CTRL1, LIS2_CTRL1_VAL);
  delay(5);
}

void lis2_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(LIS2_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t lis2_read(uint8_t reg) {
  Wire.beginTransmission(LIS2_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);  // repeated start
  Wire.requestFrom((int)LIS2_ADDR, 1);
  return Wire.available() ? Wire.read() : 0xFF;
}

void lis2_read_xyz(int16_t &x, int16_t &y, int16_t &z) {
  Wire.beginTransmission(LIS2_ADDR);
  Wire.write(LIS2_OUT_X_L);
  Wire.endTransmission(false);
  Wire.requestFrom((int)LIS2_ADDR, 6);

  uint8_t xl = Wire.read(); uint8_t xh = Wire.read();
  uint8_t yl = Wire.read(); uint8_t yh = Wire.read();
  uint8_t zl = Wire.read(); uint8_t zh = Wire.read();

  x = (int16_t)((xh << 8) | xl);
  y = (int16_t)((yh << 8) | yl);
  z = (int16_t)((zh << 8) | zl);
}

bool lis2_data_ready() {
  return (lis2_read(LIS2_STATUS) & 0x01) != 0;
}
