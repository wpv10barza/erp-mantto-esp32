# Validation matrix

| Requirement | Implementation | Evidence |
|---|---|---|
| GitHub Actions | .github/workflows/ci.yml + physical-validation.yml | CI jobs + manual hardware job |
| PlatformIO workflow | platformio.ini + platformio/src/main.cpp | pio run -e esp32s3 |
| Tests | test/command_flow, test/command_buffer, test/hit_testing | pio test -e native |
| platformio.ini | native + esp32s3 environments | CI build |
| /api/device/v1/... | backend/device_api.py | pytest E2E |
| pending_confirmation / polling E2E | CommandFlow + backend tests | pytest + Unity |
| Physical ESP32 | self-hosted workflow | serial artifact when hardware is connected |
| ST7701 / GT911 base | preserved in src/main.yaml | physical workflow |

## Important distinction

The API health endpoint reports backend health. It returns physical_device=unknown intentionally. That value is not a physical ESP32 health check.
