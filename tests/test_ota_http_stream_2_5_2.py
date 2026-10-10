from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
mirror = (root / "src/main.cpp").read_text(encoding="utf-8")
assert main == mirror
start = main.index("bool performOtaUpdate(")
end = main.index("bool checkForOtaUpdate(", start)
ota = main[start:end]
for required in (
    "class OtaFlashSink",
    "http.writeToStream(&sink)",
    "http.collectHeaders(otaHeaders, 2)",
    'http.addHeader("Accept-Encoding", "identity")',
    "Update.begin(expectedSize, U_FLASH)",
    "OTA TRANSFER result=%d received=%u expected=%u",
    "OTA PROGRESS bytes=%u/%u",
    "OTA HTTPS incompleto",
    "sink.failed()",
    "total != expectedSize",
    "if (actualSha != expectedSha)",
    "Update.end()",
):
    assert required in (main if required in ("class OtaFlashSink", "OTA PROGRESS bytes=%u/%u") else ota), required
for unsafe in (
    "http.getStreamPtr()",
    "Update.end(true)",
    "while (http.connected()",
):
    assert unsafe not in ota, unsafe
script = (root / "scripts/prepare_publish_ota_2_5_2.ps1").read_text(encoding="utf-8")
assert '$version = "2.5.2"' in script
assert 'kOtaVersion\\[\\]' not in script
print("OTA 2.5.2 chunk-aware stream, size, SHA-256 and fail-closed contract: PASS")
