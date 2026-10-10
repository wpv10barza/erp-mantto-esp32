# OTA HTTPS download troubleshooting and hardening (2.5.2)

Observed ESP32 2.5.0 attempting published 2.5.1:

```text
OTA AVAILABLE current=2.5.0 latest=2.5.1 size=1031520
PANEL STATE -> PROCESANDO | OTA 2.5.1 descargando
[ERROR] OTA OTA descarga incompleta
[ssl_client] select returned due to timeout 5000 ms
health HTTP=-1 connection refused
```

The manifest, SHA-256 and 1,031,520-byte size are valid **metadata**, but
the download and flash were not confirmed. The TLS error followed the
failed download; this does not establish that the server or Wi-Fi caused
the original interruption.

Firmware 2.5.2 changes:
- Pass the HTTP response to `HTTPClient::writeToStream` so the library
  decodes identity and chunked HTTP framing.
- Stream directly to the inactive OTA partition without buffering a
  megabyte in RAM. Enforce manifest's expected size at write time.
- Calculate streaming SHA-256 from accepted bytes and reject mismatches.
- Reject compressed Content-Encoding and prevent cross-origin redirects.
- Print progress every 128 KiB, final bytes, HTTP transfer result, Wi-Fi
  status/RSSI, free heap and Update errors (never OAuth/Wi-Fi secrets).
- Use `Update.end()` rather than `Update.end(true)` so an incomplete
  payload cannot be activated.
- Keep the 3C screen and command flow unchanged.

**Critical deployment limitation:** the physically running **2.5.0**
firmware cannot gain this fix by a GitHub commit alone. The published
`stable/2.5.1/` release MUST NOT be overwritten (immutable SHA-256).
For deployment of this change use USB COM9 after local build with private
`include/local_config.h`, or first complete an independent OTA 2.5.1
install, then publish 2.5.2 with a newly computed manifest and commit SHA.
If flashed over USB to 2.5.2, validate future OTA with a *newer* version
(e.g. 2.5.3); OTA cannot downgrade from 2.5.2 to 2.5.1.

Never upload credential-bearing firmware to a public GitHub artifact.
Use `scripts/prepare_publish_ota_2_5_2.ps1` only after choosing the
version and confirming Unity Catalog access permissions. It performs a
full clean build and refuses to overwrite the existing 2.5.1 release.

After installing the patched firmware, capture the exact
`OTA HTTP start`, `OTA PROGRESS` and `OTA TRANSFER` serial lines if
another download fails, plus the result of `health` after restart.
