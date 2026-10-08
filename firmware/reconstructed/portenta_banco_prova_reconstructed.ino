/*
  PROGETTO BANCO PROVA - ARDUINO PORTENTA C33
  Versione finale ricostruita dalla chat

  Funzioni:
  - Connessione WiFi
  - Connessione MQTT a Raspberry/Mosquitto
  - Lettura microfoni analogici A0/A1/A2
  - Lettura vibrazione placeholder per ADXL345 SPI
  - Gestione START / STOP / FAULT
  - Servo su D5 per anomalia meccanica controllata
  - Pubblicazione dati live su MQTT
  - Pubblicazione /anomalia solo se similarity < soglia

  NOTE IMPORTANTI:
  - ADXL345 SPI è ancora placeholder: sostituire leggiAccelerometroSPI()
  - NanoEdge AI è ancora placeholder: sostituire calcolaSimilarityNanoEdge()
*/

#include <WiFiC3.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <SPI.h>

// ===================== CONFIG RETE =====================
// Copiare secrets.example.h in secrets.h e inserire i propri valori.
#include "secrets.h"

// ===================== PIN =====================
const int BTN_START = 2;    // D2
const int BTN_FAULT = 0;    // D0, pulsante verso GND con INPUT_PULLUP
const int BTN_STOP  = 18;   // A3
const int SERVO_PIN = 5;    // D5

// Microfoni analogici
const int MIC_POMPA          = A0;
const int MIC_MOTORIDUTTORE  = A1;
const int MIC_BRUSHED        = A2;

// CS accelerometri ADXL345 SPI - placeholder
const int CS_POMPA          = 6;  // D6
const int CS_MOTORIDUTTORE  = 7;  // D7
const int CS_BRUSHED        = 4;  // D4

// ===================== SERVO =====================
Servo servoMotore;
const int SERVO_HOME       = 90;
const int SERVO_FAULT_POS  = 40;
const int SERVO_STEP_DELAY = 100;

// ===================== LOGICA =====================
const int SOGLIA_ANOMALIA = 75;
const unsigned long INTERVALLO_PUBBLICAZIONE = 1000;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long ultimoInvio = 0;

bool sistemaRunning = true;
bool lastFaultState = HIGH;

struct DatiMacchina {
  const char* nome;
  int micPin;
  int csPin;
  int microfono;
  int micPeak;
  int vibrazione;
  int vibPeak;
  int rpm;
  int similarity;
  bool anomalia;
  const char* stato;
};

DatiMacchina pompa = {
  "pompa", MIC_POMPA, CS_POMPA, 0, 0, 0, 0, 0, 100, false, "RUNNING"
};

DatiMacchina motoriduttore = {
  "motoriduttore", MIC_MOTORIDUTTORE, CS_MOTORIDUTTORE, 0, 0, 0, 0, 0, 100, false, "RUNNING"
};

DatiMacchina brushed = {
  "motore_brushed", MIC_BRUSHED, CS_BRUSHED, 0, 0, 0, 0, 0, 100, false, "RUNNING"
};

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("\n=== BANCO PROVA PORTENTA C33 ===");

  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_FAULT, INPUT_PULLUP);
  pinMode(BTN_STOP, INPUT_PULLUP);

  pinMode(CS_POMPA, OUTPUT);
  pinMode(CS_MOTORIDUTTORE, OUTPUT);
  pinMode(CS_BRUSHED, OUTPUT);
  digitalWrite(CS_POMPA, HIGH);
  digitalWrite(CS_MOTORIDUTTORE, HIGH);
  digitalWrite(CS_BRUSHED, HIGH);

  SPI.begin();

  randomSeed(analogRead(A5));

  connettiWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  connettiMQTT();

  publishStatoBanco("RUNNING");
  Serial.println("Sistema in RUNNING");
}

// ===================== LOOP =====================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connettiWiFi();
  }

  if (!mqttClient.connected()) {
    connettiMQTT();
  }

  mqttClient.loop();

  gestisciPulsanti();

  if (sistemaRunning && millis() - ultimoInvio >= INTERVALLO_PUBBLICAZIONE) {
    ultimoInvio = millis();
    acquisisciTutto();
    aggiornaSimilarityConNanoEdge();
    pubblicaTutto();
    pubblicaAnomalieSoloSeRilevate();
  }
}

// ===================== WIFI =====================
void connettiWiFi() {
  Serial.print("Connessione WiFi a ");
  Serial.println(WIFI_SSID);

  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(3000);
    Serial.print(".");
  }

  Serial.println("\nWiFi connesso");
  Serial.print("IP Arduino: ");
  Serial.println(WiFi.localIP());
}

// ===================== MQTT =====================
void connettiMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connessione MQTT a ");
    Serial.print(MQTT_BROKER);
    Serial.print(":");
    Serial.println(MQTT_PORT);

    String clientId = "PortentaBanco-" + String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("MQTT connesso");
      publishStatoBanco(sistemaRunning ? "RUNNING" : "STOPPED");
    } else {
      Serial.print("Errore MQTT, rc=");
      Serial.println(mqttClient.state());
      delay(3000);
    }
  }
}

void publishStatoBanco(const char* stato) {
  String payload = "{\"stato\":\"" + String(stato) + "\"}";
  mqttClient.publish("banco/stato", payload.c_str());
}

// ===================== PULSANTI =====================
void gestisciPulsanti() {
  if (digitalRead(BTN_START) == LOW) {
    sistemaRunning = true;
    impostaStatoTutte("RUNNING");
    publishStatoBanco("RUNNING");
    Serial.println("START premuto -> RUNNING");
    delay(300);
  }

  if (digitalRead(BTN_STOP) == LOW) {
    sistemaRunning = false;
    impostaStatoTutte("STOPPED");
    publishStatoBanco("STOPPED");
    Serial.println("STOP premuto -> STOPPED");
    delay(300);
  }

  bool faultState = digitalRead(BTN_FAULT);
  if (faultState != lastFaultState) {
    Serial.print("D0 FAULT cambiato: ");
    Serial.print(lastFaultState == HIGH ? "HIGH" : "LOW");
    Serial.print(" -> ");
    Serial.print(faultState == HIGH ? "HIGH" : "LOW");
    Serial.println(faultState == LOW ? " | PREMUTO" : " | RILASCIATO");
    lastFaultState = faultState;
  }

  if (faultState == LOW && sistemaRunning) {
    gestioneFault();
    delay(500);
  }
}

void impostaStatoTutte(const char* stato) {
  pompa.stato = stato;
  motoriduttore.stato = stato;
  brushed.stato = stato;
}

// ===================== FAULT CORRETTO =====================
void gestioneFault() {
  Serial.println("FAULT -> attivo SOLO anomalia meccanica controllata");

  impostaStatoTutte("FAULT_TEST");
  publishStatoBanco("FAULT_TEST");

  // 1. Creo fisicamente l'anomalia
  eseguiSequenzaServo();

  // 2. Leggo i sensori reali dopo il movimento
  acquisisciTutto();

  // 3. NanoEdge AI calcola similarity
  aggiornaSimilarityConNanoEdge();

  // 4. Pubblico dati live per dashboard
  pubblicaTutto();

  // 5. Pubblico /anomalia solo se rilevata davvero
  pubblicaAnomalieSoloSeRilevate();

  impostaStatoTutte("RUNNING");
  publishStatoBanco("RUNNING");

  Serial.println("FAULT meccanico completato -> sistema tornato RUNNING");
}

// ===================== SERVO =====================
void eseguiSequenzaServo() {
  Serial.println("Servo attach");
  servoMotore.attach(SERVO_PIN, 500, 2500);
  delay(300);

  muoviServoLento(SERVO_HOME, SERVO_FAULT_POS);
  delay(1000);
  muoviServoLento(SERVO_FAULT_POS, SERVO_HOME);
  delay(300);

  servoMotore.detach();
  Serial.println("Servo detach");
}

void muoviServoLento(int startAngle, int targetAngle) {
  if (targetAngle > startAngle) {
    for (int pos = startAngle; pos <= targetAngle; pos++) {
      servoMotore.write(pos);
      delay(SERVO_STEP_DELAY);
    }
  } else {
    for (int pos = startAngle; pos >= targetAngle; pos--) {
      servoMotore.write(pos);
      delay(SERVO_STEP_DELAY);
    }
  }
}

// ===================== ACQUISIZIONE SENSORI =====================
void acquisisciTutto() {
  acquisisciMacchina(pompa);
  acquisisciMacchina(motoriduttore);
  acquisisciMacchina(brushed);
}

void acquisisciMacchina(DatiMacchina &m) {
  m.microfono = leggiMicrofonoRMS(m.micPin);
  m.micPeak = leggiMicrofonoPeak(m.micPin);
  m.vibrazione = leggiAccelerometroSPI(m.csPin);
  m.vibPeak = m.vibrazione;
  m.rpm = 0;
}

int leggiMicrofonoRMS(int pin) {
  const int campioni = 80;
  long sommaQuadrati = 0;
  int media = 512;

  for (int i = 0; i < campioni; i++) {
    int valore = analogRead(pin);
    int centrato = valore - media;
    sommaQuadrati += (long)centrato * centrato;
    delayMicroseconds(300);
  }

  float rms = sqrt((float)sommaQuadrati / campioni);
  return (int)rms;
}

int leggiMicrofonoPeak(int pin) {
  const int campioni = 80;
  int minVal = 1023;
  int maxVal = 0;

  for (int i = 0; i < campioni; i++) {
    int valore = analogRead(pin);
    if (valore < minVal) minVal = valore;
    if (valore > maxVal) maxVal = valore;
    delayMicroseconds(300);
  }

  return maxVal - minVal;
}

// PLACEHOLDER ADXL345 SPI
// Sostituire con inizializzazione e lettura registri reali X/Y/Z.
int leggiAccelerometroSPI(int csPin) {
  (void)csPin;
  return random(100, 600);
}

// ===================== NANOEDGE AI =====================
void aggiornaSimilarityConNanoEdge() {
  aggiornaMacchinaAI(pompa);
  aggiornaMacchinaAI(motoriduttore);
  aggiornaMacchinaAI(brushed);
}

void aggiornaMacchinaAI(DatiMacchina &m) {
  m.similarity = calcolaSimilarityNanoEdge(m);
  m.anomalia = (m.similarity < SOGLIA_ANOMALIA);
}

// PLACEHOLDER NANOEDGE
// Sostituire questa funzione con la libreria reale generata da NanoEdge AI.
int calcolaSimilarityNanoEdge(DatiMacchina &m) {
  // Logica provvisoria: rumore/vibrazione più alti -> similarity più bassa
  int energia = m.microfono + m.micPeak + m.vibrazione / 2 + m.vibPeak / 2;
  int similarity = map(constrain(energia, 0, 1200), 0, 1200, 100, 40);
  return constrain(similarity, 0, 100);
}

// ===================== PUBBLICAZIONE MQTT =====================
void pubblicaTutto() {
  pubblicaDatiMacchina(pompa);
  pubblicaDatiMacchina(motoriduttore);
  pubblicaDatiMacchina(brushed);
}

void pubblicaDatiMacchina(DatiMacchina &m) {
  String topic = "banco/" + String(m.nome) + "/dati";
  String payload = creaJsonMacchina(m);

  mqttClient.publish(topic.c_str(), payload.c_str());

  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(payload);
}

void pubblicaAnomalieSoloSeRilevate() {
  pubblicaAnomaliaSeServe(pompa);
  pubblicaAnomaliaSeServe(motoriduttore);
  pubblicaAnomaliaSeServe(brushed);
}

void pubblicaAnomaliaSeServe(DatiMacchina &m) {
  if (!m.anomalia) return;

  String topic = "banco/" + String(m.nome) + "/anomalia";
  String payload = creaJsonMacchina(m);

  mqttClient.publish(topic.c_str(), payload.c_str());

  Serial.print("ANOMALIA -> ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(payload);
}

String creaJsonMacchina(DatiMacchina &m) {
  String payload = "{";
  payload += "\"macchina\":\"" + String(m.nome) + "\",";
  payload += "\"similarity\":" + String(m.similarity) + ",";
  payload += "\"microfono\":" + String(m.microfono) + ",";
  payload += "\"vibrazione\":" + String(m.vibrazione) + ",";
  payload += "\"mic_peak\":" + String(m.micPeak) + ",";
  payload += "\"vib_peak\":" + String(m.vibPeak) + ",";
  payload += "\"rpm\":" + String(m.rpm) + ",";
  payload += "\"anomalia\":" + String(m.anomalia ? "true" : "false") + ",";
  payload += "\"stato\":\"" + String(m.stato) + "\"";
  payload += "}";
  return payload;
}
