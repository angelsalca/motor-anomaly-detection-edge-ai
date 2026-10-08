#include <WiFiC3.h>
#include <ArduinoMqttClient.h>
#include <Servo.h>
#include <Wire.h>
#include "NanoEdgeAI_Accel.h"
#include "NanoEdgeAI_Mic.h"

// -----------------------------------------------------------------------------
// CONFIGURAZIONE RETE
// -----------------------------------------------------------------------------
// Copiare secrets.example.h in secrets.h e inserire i propri valori.
#include "secrets.h"

// -----------------------------------------------------------------------------
// PIN
// -----------------------------------------------------------------------------
const int BTN_START  = 2;   // D2
const int BTN_FAULT  = 3;   // D3
const int BTN_STOP   = 18;  // A3

const int SERVO_PIN  = 5;   // D5

const int MIC_BRUSHED = A2; // pin 17 — MAX9814

// -----------------------------------------------------------------------------
// MQTT TOPICS (solo motore brushed)
// -----------------------------------------------------------------------------
const char* TOPIC_BRUSHED_DATI = "banco/motore_brushed/dati";
const char* TOPIC_BRUSHED_ANOM = "banco/motore_brushed/anomalia";
const char* TOPIC_STATO        = "banco/stato";

// -----------------------------------------------------------------------------
// PARAMETRI SERVO
// -----------------------------------------------------------------------------
const int SERVO_HOME       = 90;
const int SERVO_FAULT_POS  = 40;
const int SERVO_STEP_DELAY = 100; // ms per grado

// -----------------------------------------------------------------------------
// PARAMETRI MICROFONO (lettura raw per NanoEdge AI)
// -----------------------------------------------------------------------------
const int ADC_RESOLUTION = 12;

// -----------------------------------------------------------------------------
// NanoEdge AI — Accelerometro LIS2DW12
// -----------------------------------------------------------------------------
#define LIS2_ADDR       0x19
#define LIS2_WHO_AM_I   0x0F
#define LIS2_CTRL1      0x20
#define LIS2_CTRL2      0x21
#define LIS2_CTRL6      0x25
#define LIS2_STATUS     0x27
#define LIS2_OUT_X_L    0x28

#define LIS2_CTRL1_VAL  0x90   // ODR=800 Hz, High-Performance
#define LIS2_CTRL2_VAL  0x08   // BDU=1, IF_ADD_INC=1
#define LIS2_CTRL6_VAL  0x10   // FS=±4g

#define LIS2_SENS       0.000488f  // mg/LSB a 14-bit ±4g
#define LIS2_SHIFT      2

#define ACCEL_ODR_HZ    800
#define ACCEL_PERIOD_US (1000000UL / ACCEL_ODR_HZ)  // 1250 µs

// NanoEdge AI — parametri
#define LEARNING_ITERATIONS  200   // frame di learning (motore in condizioni normali)
#define ANOMALY_THRESHOLD     90   // soglia similarity [%]: sotto = anomalia

// Buffer dati sensori
float accel_buffer[NEAI_ACCEL_BUFFER_SIZE];  // 256*3 float, interleaved x,y,z
float mic_buffer[NEAI_MIC_BUFFER_SIZE];      // 256 float

// Risultati NanoEdge AI
uint8_t accel_similarity = 0;
uint8_t mic_similarity   = 0;
enum neai_accel_state accel_state;
enum neai_mic_state   mic_state;

// Stato learning
bool     learning_done = false;
uint16_t learn_count   = 0;

// -----------------------------------------------------------------------------
// STATO BANCO
// -----------------------------------------------------------------------------
enum StatoBanco { IDLE, RUNNING, IN_FAULT, STOPPED };
StatoBanco statoBanco = IDLE;

// Valori similarity usati nel payload MQTT durante fault simulato
const int SIM_NORMALE = 95;
const int SIM_FAULT   = 35;

// -----------------------------------------------------------------------------
// STRUTTURA DATI MOTORE BRUSHED
// -----------------------------------------------------------------------------
struct DatiMacchina {
  const char* nome;
  const char* topicDati;
  const char* topicAnomalia;
  int         microfono;    // RMS microfono (raw)
  int         micPeak;
  int         similarity;   // usato nel payload MQTT (in detect = mic_similarity)
  bool        anomalia;
  const char* stato;
};

DatiMacchina motore_brushed = {
  "motore_brushed",
  TOPIC_BRUSHED_DATI,
  TOPIC_BRUSHED_ANOM,
  0, 0, SIM_NORMALE, false, "IDLE"
};

// -----------------------------------------------------------------------------
// OGGETTI GLOBALI
// -----------------------------------------------------------------------------
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
Servo      servoMotore;

// -----------------------------------------------------------------------------
// TIMING
// -----------------------------------------------------------------------------
unsigned long ultimaPublicazione = 0;
const unsigned long INTERVALLO_PUB = 1000; // ms

// -----------------------------------------------------------------------------
// ANTI-RIMBALZO PULSANTI
// -----------------------------------------------------------------------------
int statoPrecFault = HIGH;
int statoPrecStart = HIGH;
int statoPrecStop  = HIGH;

unsigned long tempoUltimaLettura = 0;
const unsigned long DEBOUNCE_MS  = 50;

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println(F("=============================================="));
  Serial.println(F(" BANCO PROVA - Portenta C33 - avvio..."));
  Serial.println(F("=============================================="));

  // ADC 12 bit
  analogReadResolution(ADC_RESOLUTION);
  delay(200); // warm-up AGC MAX9814

  // Pulsanti pull-up interno
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_FAULT, INPUT_PULLUP);
  pinMode(BTN_STOP,  INPUT_PULLUP);

  // I2C + LIS2DW12
  Wire.begin();
  Wire.setClock(400000);
  delay(10);

  uint8_t whoami = lis2_read(LIS2_WHO_AM_I);
  if (whoami != 0x44) {
    Serial.print(F("[ERRORE] LIS2DW12 WHO_AM_I: 0x"));
    Serial.print(whoami, HEX);
    Serial.println(F(" (atteso 0x44) — controllare il cablaggio"));
    while (1);
  }
  Serial.println(F("[OK] LIS2DW12 rilevato"));
  lis2_init();

  // NanoEdge AI init
  accel_state = neai_accel_init(false);
  if (accel_state != NEAI_ACCEL_OK) {
    Serial.print(F("[ERRORE] neai_accel_init: "));
    Serial.println((int)accel_state);
    while (1);
  }
  Serial.println(F("[OK] NanoEdge AI Accel inizializzato"));

  mic_state = neai_mic_init(false);
  if (mic_state != NEAI_MIC_OK) {
    Serial.print(F("[ERRORE] neai_mic_init: "));
    Serial.println((int)mic_state);
    while (1);
  }
  Serial.println(F("[OK] NanoEdge AI Mic inizializzato"));

  // Servo home
  inizializzaServo();

  // WiFi e MQTT (bloccanti in setup)
  connectWiFi();
  connectMQTT();

  // Stato iniziale
  statoBanco = RUNNING;
  motore_brushed.stato      = "RUNNING";
  motore_brushed.anomalia   = false;
  motore_brushed.similarity = SIM_NORMALE;
  publishStatoBanco("RUNNING");
  Serial.println(F(">> Sistema in RUNNING"));

  Serial.print(F("\n>>> FASE LEARNING: "));
  Serial.print(LEARNING_ITERATIONS);
  Serial.println(F(" frame — avviare il motore in condizioni NORMALI <<<\n"));
}

// =============================================================================
// LOOP PRINCIPALE
// =============================================================================
void loop() {
  // 1. Pulsanti — priorità massima
  leggiPulsanti();

  // 2. Connessioni attive (non bloccante)
  checkConnections();

  // 3. Polling MQTT
  mqttClient.poll();

  // 4. Acquisizione NanoEdge AI + pubblicazione ogni secondo
  if (millis() - ultimaPublicazione >= INTERVALLO_PUB) {
    ultimaPublicazione = millis();
    eseguiCicloNanoEdge();
    pubblicaMacchina(motore_brushed);
  }
}

// =============================================================================
// CICLO NANEDGE AI: learning o detect
// =============================================================================
void eseguiCicloNanoEdge() {
  // Acquisisce un frame da entrambi i sensori
  capture_accel_frame();
  capture_mic_frame();

  if (!learning_done) {
    // ------ FASE LEARNING ------
    neai_accel_learn(accel_buffer);
    neai_mic_learn(mic_buffer);
    learn_count++;

    if (learn_count % 10 == 0) {
      Serial.print(F("  Learning: "));
      Serial.print(learn_count);
      Serial.print(F(" / "));
      Serial.println(LEARNING_ITERATIONS);
    }

    if (learn_count >= LEARNING_ITERATIONS) {
      learning_done = true;
      Serial.println(F("\n>>> LEARNING COMPLETATO — avvio rilevamento anomalie <<<\n"));
    }

  } else {
    // ------ FASE DETECT ------
    accel_state = neai_accel_detect(accel_buffer, &accel_similarity);
    mic_state   = neai_mic_detect(mic_buffer,     &mic_similarity);

    // Aggiorna il payload MQTT con la similarity del mic
    motore_brushed.similarity = mic_similarity;
    motore_brushed.anomalia   = (mic_similarity < ANOMALY_THRESHOLD);

    // Stampa seriale
    stampaSimilarity();
  }
}

// =============================================================================
// STAMPA SERIALE
// =============================================================================
void stampaSimilarity() {
  // Stampa sempre la similarity del microfono
  Serial.print(F("Similarity: "));
  Serial.print(mic_similarity);
  Serial.println(F("%"));

  // Se sotto soglia: stampa errore + entrambe le similarity
  if (mic_similarity < ANOMALY_THRESHOLD) {
    Serial.print(F("ERRORE - Similarity Mic: "));
    Serial.print(mic_similarity);
    Serial.print(F("% | Similarity Accel: "));
    Serial.print(accel_similarity);
    Serial.println(F("%"));
  }
}

// =============================================================================
// ACQUISIZIONE FRAME ACCELEROMETRO
// Buffer interleaved: [x0,y0,z0, x1,y1,z1, ..., x255,y255,z255]
// =============================================================================
void capture_accel_frame() {
  int16_t rx, ry, rz;

  for (int i = 0; i < NEAI_ACCEL_INPUT_SIGNAL_LENGTH; i++) {
    uint32_t t0      = micros();
    uint32_t timeout = t0 + ACCEL_PERIOD_US * 2;

    while (!lis2_data_ready()) {
      if (micros() > timeout) break;
    }

    lis2_read_xyz(rx, ry, rz);

    int base = i * 3;
    accel_buffer[base + 0] = (float)(rx >> LIS2_SHIFT) * LIS2_SENS;
    accel_buffer[base + 1] = (float)(ry >> LIS2_SHIFT) * LIS2_SENS;
    accel_buffer[base + 2] = (float)(rz >> LIS2_SHIFT) * LIS2_SENS;

    while ((micros() - t0) < ACCEL_PERIOD_US);
  }
}

// =============================================================================
// ACQUISIZIONE FRAME MICROFONO
// Buffer: [s0, s1, ..., s255] — campioni ADC raw come float
// NOTA: il formato deve corrispondere esattamente ai dati di training
//       (raw ADC float, NON normalizzati — vedi NanoEdgeAI_Mic.h)
// =============================================================================
void capture_mic_frame() {
  uint32_t t_next = micros();
  const uint32_t MIC_PERIOD_US = 1000000UL / 8000; // 8 kHz

  for (int i = 0; i < NEAI_MIC_INPUT_SIGNAL_LENGTH; i++) {
    while ((int32_t)(micros() - t_next) < 0);
    mic_buffer[i] = (float)analogRead(MIC_BRUSHED);
    t_next += MIC_PERIOD_US;
  }
}

// =============================================================================
// DRIVER LIS2DW12 via I2C
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
  Wire.endTransmission(false);
  Wire.requestFrom((int)LIS2_ADDR, 1);
  return Wire.available() ? Wire.read() : 0xFF;
}

void lis2_read_xyz(int16_t &x, int16_t &y, int16_t &z) {
  Wire.beginTransmission(LIS2_ADDR);
  Wire.write(LIS2_OUT_X_L);
  Wire.endTransmission(false);
  Wire.requestFrom((int)LIS2_ADDR, 6);

  uint8_t xl = Wire.read(), xh = Wire.read();
  uint8_t yl = Wire.read(), yh = Wire.read();
  uint8_t zl = Wire.read(), zh = Wire.read();

  x = (int16_t)((xh << 8) | xl);
  y = (int16_t)((yh << 8) | yl);
  z = (int16_t)((zh << 8) | zl);
}

bool lis2_data_ready() {
  return (lis2_read(LIS2_STATUS) & 0x01) != 0;
}

// =============================================================================
// LETTURA PULSANTI con anti-rimbalzo
// =============================================================================
void leggiPulsanti() {
  if (millis() - tempoUltimaLettura < DEBOUNCE_MS) return;
  tempoUltimaLettura = millis();

  // --- FAULT D3 (priorità massima) ---
  int statoFaultOra = digitalRead(BTN_FAULT);
  if (statoFaultOra != statoPrecFault) {
    statoPrecFault = statoFaultOra;
    if (statoFaultOra == LOW) {
      Serial.println(F("FAULT -> pulsante D3 premuto"));
      gestioneFault();
    }
  }

  // --- START D2 ---
  int statoStartOra = digitalRead(BTN_START);
  if (statoStartOra != statoPrecStart) {
    statoPrecStart = statoStartOra;
    if (statoStartOra == LOW) {
      Serial.println(F("START -> pulsante D2 premuto"));
      statoBanco = RUNNING;
      motore_brushed.stato    = "RUNNING";
      motore_brushed.anomalia = false;
      publishStatoBanco("RUNNING");
    }
  }

  // --- STOP A3 ---
  int statoStopOra = digitalRead(BTN_STOP);
  if (statoStopOra != statoPrecStop) {
    statoPrecStop = statoStopOra;
    if (statoStopOra == LOW) {
      Serial.println(F("STOP -> pulsante premuto"));
      statoBanco = STOPPED;
      motore_brushed.stato    = "STOPPED";
      motore_brushed.anomalia = false;
      publishStatoBanco("STOPPED");
    }
  }
}

// =============================================================================
// GESTIONE EVENTO FAULT
// =============================================================================
void gestioneFault() {
  statoBanco = IN_FAULT;
  publishStatoBanco("FAULT_TEST");

  motore_brushed.stato      = "FAULT_TEST";
  motore_brushed.anomalia   = true;
  motore_brushed.similarity = SIM_FAULT;

  pubblicaMacchina(motore_brushed);
  pubblicaAnomalia(motore_brushed);

  eseguiSequenzaServo();

  // Ripristino
  statoBanco = RUNNING;
  motore_brushed.stato      = "RUNNING";
  motore_brushed.anomalia   = false;
  motore_brushed.similarity = SIM_NORMALE;
  publishStatoBanco("RUNNING");
  Serial.println(F(">> Sistema tornato in RUNNING dopo FAULT"));
}

// =============================================================================
// PUBBLICAZIONE MQTT
// =============================================================================
void pubblicaMacchina(DatiMacchina &m) {
  char payload[256];
  snprintf(payload, sizeof(payload),
    "{\"macchina\":\"%s\","
    "\"similarity\":%d,"
    "\"microfono\":%d,"
    "\"vibrazione\":0,"
    "\"mic_peak\":%d,"
    "\"vib_peak\":0,"
    "\"rpm\":0,"
    "\"anomalia\":%s,"
    "\"stato\":\"%s\"}",
    m.nome,
    m.similarity,
    m.microfono,
    m.micPeak,
    m.anomalia ? "true" : "false",
    m.stato
  );

  mqttClient.beginMessage(m.topicDati);
  mqttClient.print(payload);
  mqttClient.endMessage();
}

void pubblicaAnomalia(DatiMacchina &m) {
  char payload[128];
  snprintf(payload, sizeof(payload),
    "{\"macchina\":\"%s\","
    "\"similarity\":%d,"
    "\"anomalia\":true,"
    "\"stato\":\"FAULT_TEST\"}",
    m.nome,
    m.similarity
  );

  mqttClient.beginMessage(m.topicAnomalia);
  mqttClient.print(payload);
  mqttClient.endMessage();

  Serial.print(F(">> Anomalia pubblicata: "));
  Serial.println(m.topicAnomalia);
}

void publishStatoBanco(const char* stato) {
  char payload[64];
  snprintf(payload, sizeof(payload), "{\"stato\":\"%s\"}", stato);
  mqttClient.beginMessage(TOPIC_STATO);
  mqttClient.print(payload);
  mqttClient.endMessage();
  Serial.print(F(">> Stato banco: "));
  Serial.println(stato);
}

// =============================================================================
// SERVO
// =============================================================================
void inizializzaServo() {
  servoMotore.attach(SERVO_PIN, 500, 2500);
  servoMotore.write(SERVO_HOME);
  delay(2000);
  Serial.println(F(">> Servo in HOME (90 gradi)"));
}

void muoviLento(int da, int a) {
  if (da <= a) {
    for (int pos = da; pos <= a; pos++) {
      servoMotore.write(pos);
      delay(SERVO_STEP_DELAY);
    }
  } else {
    for (int pos = da; pos >= a; pos--) {
      servoMotore.write(pos);
      delay(SERVO_STEP_DELAY);
    }
  }
}

void eseguiSequenzaServo() {
  Serial.println(F(">> Servo -> vado lentamente in FAULT (40 gradi)"));
  muoviLento(SERVO_HOME, SERVO_FAULT_POS);
  delay(500);
  Serial.println(F(">> Servo -> torno lentamente in HOME (90 gradi)"));
  muoviLento(SERVO_FAULT_POS, SERVO_HOME);
  Serial.println(F(">> Movimento servo completato"));
}

// =============================================================================
// CONNESSIONI
// =============================================================================
void connectWiFi() {
  Serial.print(F("Connessione WiFi a: "));
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tentativi = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
    if (++tentativi > 40) {
      Serial.println(F("\nERRORE: WiFi non raggiungibile. Blocco."));
      while (1) delay(1000);
    }
  }
  Serial.println();
  Serial.print(F("WiFi connesso. IP: "));
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  Serial.print(F("Connessione MQTT a "));
  Serial.print(MQTT_BROKER);
  Serial.print(F(":"));
  Serial.println(MQTT_PORT);

  int tentativi = 0;
  while (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
    Serial.print(F("Errore MQTT (cod. "));
    Serial.print(mqttClient.connectError());
    Serial.println(F(") - riprovo..."));
    delay(1000);
    if (++tentativi > 10) {
      Serial.println(F("MQTT non raggiungibile - continuo senza"));
      return;
    }
  }
  Serial.println(F("MQTT connesso."));
}

void checkConnections() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi perso - riconnessione..."));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 8000) {
      delay(500);
    }
    if (WiFi.status() == WL_CONNECTED) Serial.println(F("WiFi riconnesso."));
  }

  if (!mqttClient.connected()) {
    Serial.println(F("MQTT disconnesso - tentativo riconnessione..."));
    mqttClient.connect(MQTT_BROKER, MQTT_PORT);
  }
}
