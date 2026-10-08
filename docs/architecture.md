# Architecture

```mermaid
flowchart LR
    F[FAULT button] --> S[Servo + mechanical brake]
    S --> M[Brushed DC motor behaviour changes]
    M --> MIC[MAX9814 microphone]
    M --> ACC[LIS2DW12 / accelerometer]
    MIC --> P[Arduino Portenta C33]
    ACC --> P
    P --> N[NanoEdge AI similarity]
    N --> MQ[MQTT over Wi-Fi]
    MQ --> R[Raspberry Pi + Mosquitto]
    R --> DB[(SQLite)]
    DB --> W[Flask dashboard /api/live]
    R --> E[Email alert on /anomalia]
```

The key conceptual separation is **fault injection vs fault detection**:

- the button/servo creates a physical disturbance;
- the sensors observe what changed;
- Edge AI decides whether the new signal is sufficiently different;
- only a detected anomaly should produce the `/anomalia` event.
