# Firmware notes

There are **two different Portenta sketches on purpose**.

## `recovered/banco_prova_brushed_recovered.ino`

This is the most important historical artifact: it is the recovered integrated project firmware, with only network credentials removed for public sharing.
It contains real LIS2DW12 acquisition, MAX9814 acquisition and NanoEdge AI API calls.
It also preserves an older FAULT path that forces an anomaly before the servo sequence; that mismatch is documented rather than hidden.

## `reconstructed/portenta_banco_prova_reconstructed.ino`

This file was reconstructed later from project notes/chat to express the **intended corrected FAULT logic**. It is not presented as the original firmware that ran on the test bench. Its ADXL345 and NanoEdge functions are placeholders.

## `data_collection/`

Standalone sketches used to collect sensor frames for NanoEdge AI Studio:

- LIS2DW12: 256 XYZ samples at 800 Hz.
- MAX9814: 256 ADC samples at 8 kHz.
