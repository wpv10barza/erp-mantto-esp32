# Physical ESP32-S3 validation

GitHub-hosted runners cannot prove USB, power, display, touch, audio, GPIO, PSRAM, or serial behavior on a physical ESP32-S3.

This repository separates:

- CI evidence: native tests, ESP32 compilation, and API E2E.
- Physical evidence: a manual workflow on a self-hosted runner connected to the board.

The physical workflow is "Physical ESP32-S3 validation" and requires a runner labeled:

    self-hosted, linux, x64, esp32

The uploaded serial log is evidence for the actual connected device. A successful cloud compile must not be reported as physical validation.

Hardware checks for the 480x480 Guition panel should include:

- ST7701 display initialization and 480x480 rendering
- GT911 touch response
- Wi-Fi association
- PSRAM allocation
- USB-C serial stability
- audio/GPIO behavior where fitted
- power stability during boot and network activity
