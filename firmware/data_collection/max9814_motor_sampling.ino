#define MIC_PIN         A0          // Uscita analogica MAX9814
#define SAMPLE_RATE_HZ  8000        // Frequenza di campionamento [Hz]
#define FRAME_SIZE      256         // Campioni per frame (NanoEdge AI)
#define ADC_RESOLUTION  12          // Bit risoluzione ADC (max 12 per C33)

// Modalità output: true = formato NanoEdge AI Studio, false = debug
#define NANOEDGE_MODE   true

// ---- Buffer ---------------------------------------------------------------

uint16_t sampleBuffer[FRAME_SIZE];

// ---- Timing ---------------------------------------------------------------

// Periodo di campionamento in microsecondi
const uint32_t SAMPLE_PERIOD_US = 1000000UL / SAMPLE_RATE_HZ;  // 125 µs @ 8kHz

// ---- Setup ----------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);  // Attendi Serial (max 3s)

  // Configura ADC a 12 bit per sfruttare la piena risoluzione del C33
  analogReadResolution(ADC_RESOLUTION);

  // Warm-up: il MAX9814 impiega ~100ms per stabilizzare l'AGC all'avvio
  delay(200);

  if (!NANOEDGE_MODE) {
    Serial.println("=== MAX9814 Motor Noise Sampler ===");
    Serial.print("Sample rate : "); Serial.print(SAMPLE_RATE_HZ); Serial.println(" Hz");
    Serial.print("Frame size  : "); Serial.print(FRAME_SIZE);      Serial.println(" samples");
    Serial.print("Frame time  : ");
    Serial.print((float)FRAME_SIZE / SAMPLE_RATE_HZ * 1000.0f, 1);
    Serial.println(" ms");
    Serial.println("-----------------------------------");
    Serial.println("Timestamp(µs) | ADC_raw | mV");
  } else {
    // Intestazione opzionale per NanoEdge AI Studio (commentata per default,
    // decommentare se il tool la richiede)
    // Serial.println("# MAX9814 8kHz 256samples/frame");
  }
}

// ---- Loop principale ------------------------------------------------------

void loop() {
  captureFrame();
  outputFrame();
}

// ---- Acquisizione frame ---------------------------------------------------

void captureFrame() {
  uint32_t t_start = micros();
  uint32_t t_next  = t_start;

  for (uint16_t i = 0; i < FRAME_SIZE; i++) {
    // Attendi il prossimo istante di campionamento (non-blocking tight loop)
    while ((int32_t)(micros() - t_next) < 0);

    sampleBuffer[i] = (uint16_t)analogRead(MIC_PIN);

    t_next += SAMPLE_PERIOD_US;  // Programma il prossimo campione
  }
}

// ---- Output frame ---------------------------------------------------------

void outputFrame() {
  if (NANOEDGE_MODE) {
    // Formato NanoEdge AI Studio:
    // tutti i campioni del frame su una singola riga, separati da spazio
    for (uint16_t i = 0; i < FRAME_SIZE; i++) {
      Serial.print(sampleBuffer[i]);
      if (i < FRAME_SIZE - 1) Serial.print(' ');
    }
    Serial.println();

  } else {
    // Formato debug: timestamp + valore grezzo + tensione in mV
    // Riferimento ADC: 3.3V, risoluzione 12-bit (4096 livelli)
    const float MV_PER_LSB = 3300.0f / 4096.0f;

    Serial.println("--- Frame ---");
    for (uint16_t i = 0; i < FRAME_SIZE; i++) {
      uint32_t t_us = (uint32_t)i * SAMPLE_PERIOD_US;
      float    mv   = sampleBuffer[i] * MV_PER_LSB;

      Serial.print(t_us);
      Serial.print("\t");
      Serial.print(sampleBuffer[i]);
      Serial.print("\t");
      Serial.println(mv, 1);
    }
  }
}