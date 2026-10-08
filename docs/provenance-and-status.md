# Provenance and project status

This repository separates **recovered project artifacts** from **later reconstructions** so that the portfolio remains honest.

## Recovered artifacts

- integrated Portenta firmware `banco_prova_brushed_final.ino` (public copy only removes network credentials)
- LIS2DW12 data-acquisition sketch
- MAX9814 data-acquisition sketch
- NanoEdge AI generated library archives (retained privately, not redistributed here)
- STEP mechanical assembly
- electrical/wiring schematics
- project planning, requirements, technical report and presentation (used to curate this repo, not all republished)

## Reconstructed artifacts

- Raspberry Pi `monitor_mqtt_mail.py`
- Flask `dashboard.py`
- systemd service example
- `portenta_banco_prova_reconstructed.ino`, which expresses the corrected intended FAULT flow but still uses placeholders for ADXL345/NanoEdge

These reconstructed files came from project notes/chat history. They are useful as a reproducible portfolio implementation, but should not be described as the exact original Raspberry source unless the original filesystem copy is later recovered.

## Important revision mismatch

Early project material explicitly lists the **LIS2DW12** accelerometer and the recovered integrated firmware implements it over I2C. A later report refers to **ADXL345 over SPI** and states that its real readout was still incomplete. This repository keeps both facts visible instead of pretending there was one perfectly consistent final revision.

The recovered integrated firmware also contains an older FAULT routine that forces an anomaly before moving the servo, while the final documentation states that the correct logic is servo -> sensors -> Edge AI -> anomaly event.

## What is still missing

The repo is already sufficient for a portfolio. For full historical reproducibility, the following would still be useful if ever found:

- the original Raspberry Pi filesystem/source files actually used on the bench;
- raw NanoEdge AI training/learning datasets and Studio project/configuration;
- runtime logs or a short video proving the complete end-to-end demo;
- an unambiguous as-built final wiring diagram for the exact hardware revision.
