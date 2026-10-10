#if defined(BOARD_PANEL_4848S040)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ctype.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include <esp_ota_ops.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <esp_system.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "app_config.h"
#include "command_buffer.h"
#include "command_text_viewport.h"
#include "editor_components.h"
#include "touch_input.h"
#include "panel_gpio.h"
#include "home_menu.h"
#include "virtual_keyboard.h"

namespace pins = panel_gpio;


namespace {
constexpr uint8_t kTouchAddress = 0x5D;
constexpr uint16_t kTouchStatusRegister = 0x814E;
constexpr uint16_t kTouchPointRegister = 0x814F;
constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 480;

WebServer web(80);
Arduino_ESP32SPI* displayBus = nullptr;
Arduino_RGB_Display* display = nullptr;

enum class PanelState {
  Booting,
  Offline,
  Ready,
  Busy,
  Pending,
  Applied,
  Rejected,
  Error,
};

enum class HomePanel {
  None,
  Backend,
  Sheets,
  Github,
  Firmware,
  Wifi,
  Databricks,
  Diagnostics,
  Device,
};

HomePanel homePanel = HomePanel::None;
constexpr char kFirmwareVersion[] = "2.7.0-ota-commit";
// Credentials are provisioned once over USB and then loaded from ESP32 NVS
// on subsequent credential-free OTA builds. NVS persists across OTA slots.
String runtimeWifiSsid, runtimeWifiPassword;
String runtimeDatabricksClientId, runtimeDatabricksClientSecret, runtimeApiToken;

void loadDeviceCredentials() {
  runtimeWifiSsid = app_config::wifiSsid;
  runtimeWifiPassword = app_config::wifiPassword;
  runtimeDatabricksClientId = app_config::databricksClientId;
  runtimeDatabricksClientSecret = app_config::databricksClientSecret;
  runtimeApiToken = app_config::apiToken;
  Preferences prefs;
  if (!prefs.begin("ota3c", false)) {
    Serial.println("[WARN] OTA NVS sin acceso; requiere configuracion embebida");
    return;
  }
  auto stored = [&prefs](const char* key, const char* compiled) -> String {
    if (compiled && strlen(compiled)) {
      prefs.putString(key, String(compiled));
      return String(compiled);
    }
    return prefs.getString(key, "");
  };
  runtimeWifiSsid = stored("wifi_ssid", app_config::wifiSsid);
  runtimeWifiPassword = stored("wifi_pass", app_config::wifiPassword);
  runtimeDatabricksClientId = stored("sp_id", app_config::databricksClientId);
  runtimeDatabricksClientSecret = stored("sp_secret", app_config::databricksClientSecret);
  runtimeApiToken = stored("device_key", app_config::apiToken);
  prefs.end();
  Serial.printf("OTA NVS: Wi-Fi=%s cloud_auth=%s device_token=%s (valores ocultos)\n",
    runtimeWifiSsid.length() ? "OK" : "NO",
    (runtimeDatabricksClientId.length() && runtimeDatabricksClientSecret.length()) ? "OK" : "NO",
    runtimeApiToken.length() ? "OK" : "NO");
}


PanelState panelState = PanelState::Booting;
String panelDetail = "Iniciando";
String lastBackendMessage = "Sin verificar";
String lastCommandId;
String databricksAccessToken;
unsigned long databricksTokenAcquiredMs = 0;
unsigned long databricksTokenLifetimeMs = 0;
unsigned long lastWifiAttempt = 0;
unsigned long lastHealthCheck = 0;
unsigned long lastCommandPoll = 0;
std::atomic<bool> backendAvailable{false};
bool displayReady = false;
bool audioReady = false;
bool mdnsReady = false;
bool wifiAnnounced = false;

enum class NetworkAction : uint8_t {
  Health, CloudAndSheets, Send3C, LoadHistory, CheckOtaCommit, InstallOta
};

// OTA is intentionally commit-selectable only among published Databricks builds.
// Never fetch raw commits or write flash without validating size and SHA-256.
constexpr size_t kOtaCommitCapacity = 64;
CommandBuffer<kOtaCommitCapacity> otaCommitBuffer;
bool otaCommitEditorOpen = false;
bool otaConfirmArmed = false;
struct OtaSelection {
  bool ready = false;
  char commit[41] = {};
  char version[32] = {};
  char sha256[65] = {};
  char url[110] = {};
  char message[86] = "Ingrese commit publicado";
  size_t size = 0;
};
OtaSelection otaSelection{};
portMUX_TYPE otaMux = portMUX_INITIALIZER_UNLOCKED;

bool validCommitSha(String value) {
  value.trim();
  if (value.length() < 7 || value.length() > 40) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    const char ch = value[i];
    if (!isxdigit(static_cast<unsigned char>(ch))) return false;
  }
  return true;
}

bool validSha256(const String& value) {
  if (value.length() != 64) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isxdigit(static_cast<unsigned char>(value[i]))) return false;
  }
  return true;
}

void setOtaStatus(const String& status, bool clearCandidate = false) {
  char message[86]{};
  status.toCharArray(message, sizeof(message));
  portENTER_CRITICAL(&otaMux);
  if (clearCandidate) otaSelection.ready = false;
  memcpy(otaSelection.message, message, sizeof(message));
  portEXIT_CRITICAL(&otaMux);
}

int otaVersionCompare(const char* latest, const char* current) {
  unsigned a[3]{}, b[3]{};
  if (sscanf(latest, "%u.%u.%u", &a[0], &a[1], &a[2]) != 3 ||
      sscanf(current, "%u.%u.%u", &b[0], &b[1], &b[2]) != 3) return -1;
  for (int i = 0; i < 3; ++i) {
    if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
  }
  return 0;
}

constexpr int kHistoryCapacity = 8;
struct HistoryItem {
  char when[28];
  char status[26];
  char text[146];
  char preview[146];
};
struct HistorySnapshot {
  HistoryItem orders[kHistoryCapacity];
  HistoryItem sheets[kHistoryCapacity];
  uint8_t orderCount = 0;
  uint8_t sheetCount = 0;
};
HistorySnapshot historyCache{};
portMUX_TYPE historyMux = portMUX_INITIALIZER_UNLOCKED;
bool historyScreenOpen = false;
bool historySheetTab = false;
bool historyDetailOpen = false;
int historyPage = 0;
int historySelected = 0;
std::atomic<bool> historyLoading{false};
struct NetworkRequest {
  NetworkAction action;
  char command[241];
};
struct UiNotification {
  PanelState state;
  char detail[160];
  bool sound;
};
QueueHandle_t networkQueue = nullptr;
QueueHandle_t uiQueue = nullptr;
TaskHandle_t networkTaskHandle = nullptr;
std::atomic<bool> pendingCommand{false};
unsigned long lastTouchActivityMs = 0;
unsigned long lastHandledTapMs = 0;
constexpr unsigned long kMissingReleaseMs = 80;
constexpr unsigned long kTouchDebounceMs = 200;

touch_input::TapTracker touchTracker;
constexpr size_t kCommandCapacity = 240;
CommandBuffer<kCommandCapacity> commandBuffer;
virtual_keyboard::KeyboardMode keyboardMode = virtual_keyboard::KeyboardMode::Alpha;
bool commandEditorOpen = false;

struct TouchSample {
  bool ready = false;
  bool touched = false;
  uint16_t x = 0;
  uint16_t y = 0;
};

uint16_t color565(uint8_t red, uint8_t green, uint8_t blue) {
  return display ? display->color565(red, green, blue) : 0;
}

const char* stateLabel(PanelState state) {
  switch (state) {
    case PanelState::Booting: return "INICIANDO";
    case PanelState::Offline: return "SIN CONEXION";
    case PanelState::Ready: return "DATABRICKS LISTO";
    case PanelState::Busy: return "PROCESANDO";
    case PanelState::Pending: return "PENDIENTE";
    case PanelState::Applied: return "APLICADO";
    case PanelState::Rejected: return "RECHAZADO";
    case PanelState::Error: return "ERROR";
  }
  return "3C";
}

uint16_t stateBackground(PanelState state) {
  switch (state) {
    case PanelState::Ready: return color565(5, 45, 27);
    case PanelState::Busy: return color565(8, 28, 58);
    case PanelState::Pending: return color565(68, 43, 2);
    case PanelState::Applied: return color565(2, 65, 28);
    case PanelState::Rejected: return color565(62, 29, 3);
    case PanelState::Error: return color565(65, 5, 9);
    case PanelState::Offline: return color565(18, 22, 30);
    case PanelState::Booting: return color565(10, 18, 38);
  }
  return 0;
}

void drawCentered(const String& text, int y, uint8_t size, uint16_t color) {
  if (!displayReady) return;
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  display->setTextSize(size);
  display->getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  int x = (kScreenWidth - static_cast<int>(width)) / 2;
  if (x < 4) x = 4;
  display->setTextColor(color);
  display->setCursor(x, y);
  display->print(text);
}

void drawButton(int x, int y, int width, int height, const char* label, uint16_t fill) {
  if (!displayReady) return;
  display->fillRoundRect(x, y, width, height, 16, fill);
  display->drawRoundRect(x, y, width, height, 16, BLACK);
  display->setTextSize(2);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textWidth = 0;
  uint16_t textHeight = 0;
  display->getTextBounds(label, 0, 0, &x1, &y1, &textWidth, &textHeight);
  display->setTextColor(BLACK);
  display->setCursor(x + (width - textWidth) / 2, y + (height - textHeight) / 2);
  display->print(label);
}

void drawEditorFrame() {
  if (!displayReady) return;
  display->fillScreen(WHITE);
  display->fillRect(0, 0, kScreenWidth, 42, WHITE);
  drawCentered("EDITAR ORDEN 3C", 11, 2, BLACK);
  display->drawRoundRect(368, 3, 101, 35, 8, BLACK);
  display->setTextSize(1);
  display->setTextColor(BLACK);
  display->setCursor(384, 16);
  display->print("HISTORIAL");
}

void drawEditorTextField() {
  if (!displayReady) return;
  const auto field = editor_ui::EditorLayout::textField();
  display->fillRoundRect(field.left, field.top, field.width(), field.height(), 10,
                         WHITE);
  display->drawRoundRect(field.left, field.top, field.width(), field.height(), 10, BLACK);
  display->setTextSize(2);
  display->setTextColor(BLACK);

  uint16_t prefixWidths[kCommandCapacity + 1] = {};
  String full(commandBuffer.c_str());
  for (size_t i = 0; i < full.length() && i < kCommandCapacity; ++i) {
    int16_t x1 = 0, y1 = 0; uint16_t w = 0, h = 0;
    display->getTextBounds(full.substring(0, i + 1), 0, 0, &x1, &y1, &w, &h);
    prefixWidths[i + 1] = w;
  }
  const auto window = command_text_viewport::compute(
      prefixWidths, commandBuffer.length(), commandBuffer.cursor(), field.width() - 22, 3);
  const String visible = full.substring(window.first, window.last);
  display->setCursor(field.left + 10, field.top + 34);
  display->print(visible);
  const int cursorX = field.left + 10 + window.cursorX;
  display->drawFastVLine(cursorX, field.top + 24, 28, BLACK);
}

void drawEditorToolbar() {
  if (!displayReady) return;
  for (const auto& button : editor_ui::ToolbarComponent::buttons()) {
    const bool home = button.action == editor_ui::ToolbarAction::Home;
    const bool destructive = button.action == editor_ui::ToolbarAction::Clear;
    const uint16_t fill = home ? WHITE
        : destructive ? WHITE : WHITE;
    display->fillRoundRect(button.rect.left, button.rect.top,
                           button.rect.width(), button.rect.height(), 9, fill);
    display->drawRoundRect(button.rect.left, button.rect.top,
                           button.rect.width(), button.rect.height(), 9, BLACK);
    display->setTextColor(BLACK);
    display->setTextSize(strlen(button.label) > 3 ? 1 : 2);
    int16_t x1 = 0, y1 = 0; uint16_t w = 0, h = 0;
    display->getTextBounds(button.label, 0, 0, &x1, &y1, &w, &h);
    display->setCursor(button.rect.left + (button.rect.width() - w) / 2,
                       button.rect.top + (button.rect.height() - h) / 2);
    display->print(button.label);
  }
}

void drawEditorKeyboard() {
  if (!displayReady) return;
  const int top = virtual_keyboard::KeyboardLayout::top(keyboardMode);
  const int bottom = virtual_keyboard::KeyboardLayout::bottom(keyboardMode);
  display->fillRect(0, 216, kScreenWidth, kScreenHeight - 216, WHITE);
  display->fillRoundRect(5, top - 6, 470, bottom - top + 12, 12, WHITE);

  virtual_keyboard::Key keys[50]{};
  const size_t count = virtual_keyboard::buildKeys(keyboardMode, keys, 50);
  for (size_t i = 0; i < count; ++i) {
    const auto& key = keys[i];
    uint16_t fill = WHITE;
    if (key.definition.kind == virtual_keyboard::KeyKind::Enter) fill = WHITE;
    if (key.definition.kind == virtual_keyboard::KeyKind::ToggleAlphaNumeric) fill = WHITE;
    if (key.definition.kind == virtual_keyboard::KeyKind::Space) fill = WHITE;

    display->fillRoundRect(key.rect.left, key.rect.top,
                           key.rect.right - key.rect.left,
                           key.rect.bottom - key.rect.top, 7, fill);
    display->drawRoundRect(key.rect.left, key.rect.top,
                           key.rect.right - key.rect.left,
                           key.rect.bottom - key.rect.top, 7, BLACK);
    display->setTextSize(strlen(key.definition.label) > 2 ? 1 : 2);
    int16_t x1 = 0, y1 = 0; uint16_t w = 0, h = 0;
    display->getTextBounds(key.definition.label, 0, 0, &x1, &y1, &w, &h);
    display->setTextColor(BLACK);
    display->setCursor(key.rect.left + ((key.rect.right - key.rect.left) - w) / 2,
                       key.rect.top + ((key.rect.bottom - key.rect.top) - h) / 2);
    display->print(key.definition.label);
  }
}

void drawEditor() {
  if (!displayReady) return;
  drawEditorFrame();
  drawEditorTextField();
  drawEditorToolbar();
  drawEditorKeyboard();
}

void drawOtaCommitField() {
  if (!displayReady) return;
  const auto f = editor_ui::EditorLayout::textField();
  display->fillRoundRect(f.left, f.top, f.width(), f.height(), 10, WHITE);
  display->drawRoundRect(f.left, f.top, f.width(), f.height(), 10, BLACK);
  display->setTextColor(BLACK);
  display->setTextSize(1);
  display->setCursor(24, 61);
  display->print("Git SHA 7-40 caracteres (solo hexadecimal)");
  display->setTextSize(2);
  const String content(otaCommitBuffer.c_str());
  const int start = content.length() > 30 ? content.length() - 30 : 0;
  display->setCursor(24, 98);
  display->print(content.substring(start));
  display->setTextSize(1);
  display->setCursor(24, 131);
  display->print("ENTER = buscar OTA publicada");
}

void drawOtaCommitEditor() {
  if (!displayReady) return;
  display->fillScreen(WHITE);
  drawCentered("EDITAR COMMIT OTA", 11, 2, BLACK);
  drawOtaCommitField();
  drawEditorToolbar();
  drawEditorKeyboard();
}

void drawChevron(int x, int y, bool down) {
  if (!displayReady) return;
  const uint16_t c = BLACK;
  if (down) {
    display->drawLine(x - 6, y - 3, x, y + 3, c);
    display->drawLine(x, y + 3, x + 6, y - 3, c);
  } else {
    display->drawLine(x - 3, y - 6, x + 3, y, c);
    display->drawLine(x + 3, y, x - 3, y + 6, c);
  }
}

void drawHomeBackground() {
  if (!displayReady) return;
  display->fillScreen(WHITE);
}

void drawHomeRow(
    int y,
    const char* tag,
    const char* title,
    const char* subtitle,
    bool expanded = false,
    int height = 45) {
  if (!displayReady) return;
  const uint16_t card = WHITE;
  const uint16_t border = BLACK;
  display->fillRoundRect(14, y, 452, height, 12, card);
  display->drawRoundRect(14, y, 452, height, 12, border);

  display->fillRoundRect(24, y + 7, 42, height - 14, 9, WHITE);
  display->setTextColor(BLACK);
  display->setTextSize(1);
  display->setCursor(35, y + 18);
  display->print(tag);

  display->setTextSize(2);
  display->setCursor(78, y + 7);
  display->print(title);

  if (subtitle && strlen(subtitle)) {
    display->setTextSize(1);
    display->setTextColor(BLACK);
    display->setCursor(78, y + 28);
    display->print(subtitle);
  }

  drawChevron(446, y + height / 2, expanded);
}

const char* homePanelTitle(HomePanel panel) {
  switch (panel) {
    case HomePanel::Backend: return "Conexion al backend";
    case HomePanel::Sheets: return "Google Sheets";
    case HomePanel::Github: return "GitHub Actions";
    case HomePanel::Firmware: return "Actualizar firmware";
    case HomePanel::Wifi: return "Wi-Fi 2.4 GHz";
    case HomePanel::Databricks: return "Nube Databricks";
    case HomePanel::Diagnostics: return "Diagnostico";
    case HomePanel::Device: return "Estado del dispositivo";
    case HomePanel::None: return "";
  }
  return "";
}

void drawExpandedPanel() {
  if (!displayReady || homePanel == HomePanel::None) return;
  display->fillRoundRect(14, 268, 452, 148, 14, WHITE);
  display->drawRoundRect(14, 268, 452, 148, 14, BLACK);
  display->setTextColor(BLACK);
  display->setTextSize(2);
  display->setCursor(28, 282);
  display->print(homePanelTitle(homePanel));
  drawChevron(446, 292, true);

  display->setTextSize(1);
  display->setTextColor(BLACK);

  if (homePanel == HomePanel::Backend) {
    display->setCursor(28, 316);
    display->print("HTTPS hacia Databricks Apps");
    display->setCursor(28, 336);
    display->print("OAuth M2M + token del dispositivo");
    display->setCursor(28, 356);
    display->print("Usa PROBAR CLOUD para validar extremo a extremo.");
  } else if (homePanel == HomePanel::Sheets) {
    display->setCursor(28, 316);
    display->print("Lectura y control mediante backend Databricks.");
    display->setCursor(28, 336);
    display->print("Cambios sujetos a revision humana.");
    display->setCursor(28, 356);
    display->print("El ESP32 nunca escribe Sheets directamente.");
  } else if (homePanel == HomePanel::Github) {
    display->setCursor(28, 316);
    display->print("GitHub Actions valida contratos y compilacion.");
    display->setCursor(28, 336);
    display->print("El firmware final se publica como artefacto.");
  } else if (homePanel == HomePanel::Firmware) {
    OtaSelection candidate{};
    portENTER_CRITICAL(&otaMux);
    candidate = otaSelection;
    portEXIT_CRITICAL(&otaMux);
    display->setCursor(28, 310);
    display->print("FW actual: ");
    display->print(kFirmwareVersion);
    display->setCursor(28, 327);
    display->print("Commit: ");
    display->print(String(otaCommitBuffer.c_str()).substring(0, 40));
    display->setCursor(28, 344);
    display->print(String(candidate.message).substring(0, 70));
    display->fillRoundRect(26, 361, 201, 43, 9, WHITE);
    display->drawRoundRect(26, 361, 201, 43, 9, BLACK);
    display->fillRoundRect(239, 361, 214, 43, 9, WHITE);
    display->drawRoundRect(239, 361, 214, 43, 9, BLACK);
    display->setTextSize(2);
    display->setCursor(47, 375);
    display->print("EDITAR SHA");
    display->setCursor(252, 375);
    display->print(otaConfirmArmed ? "CONFIRMAR" :
                   candidate.ready ? "INSTALAR OTA" : "BUSCAR OTA");
  } else if (homePanel == HomePanel::Wifi) {
    display->setCursor(28, 316);
    display->print("SSID: ");
    display->print(runtimeWifiSsid.length() ? runtimeWifiSsid : "No configurado");
    display->setCursor(28, 336);
    display->print("Estado: ");
    display->print(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado");
    display->setCursor(28, 356);
    display->print("Direccion IP gestionada por DHCP");
    display->setCursor(28, 376);
    display->print("RSSI: ");
    display->print(WiFi.RSSI());
    display->print(" dBm");
  } else if (homePanel == HomePanel::Databricks) {
    display->setCursor(28, 316);
    display->print("Backend: asistente-cloud-erp");
    display->setCursor(28, 336);
    display->print("Estado: ");
    display->print(backendAvailable ? "Conectado" : "Sin verificar");
    display->setCursor(28, 356);
    display->print("CLOUD HTTPS");
    display->setCursor(28, 376);
    display->print("OAuth 2.0 M2M");
  } else if (homePanel == HomePanel::Diagnostics) {
    display->setCursor(28, 316);
    display->print("PSRAM: ");
    display->print(psramFound() ? "OK" : "NO");
    display->setCursor(28, 336);
    display->print("Heap libre: ");
    display->print(ESP.getFreeHeap());
    display->setCursor(28, 356);
    display->print("Uptime: ");
    display->print(millis() / 1000UL);
    display->print(" s");
  } else if (homePanel == HomePanel::Device) {
    display->setCursor(28, 316);
    display->print("ID: ");
    display->print(app_config::deviceId);
    display->setCursor(28, 336);
    display->print("Firmware: ");
    display->print(kFirmwareVersion);
    display->setCursor(28, 356);
    display->print("Estado: ");
    display->print(stateLabel(panelState));
  }
}

bool queueNetworkRequest(NetworkAction action, const String& command = String());
void drawPanel();

void drawHistoryWrapped(const String& message, int top, int lines = 3) {
  display->setTextSize(1);
  display->setTextColor(BLACK);
  for (int row = 0, offset = 0; row < lines && offset < static_cast<int>(message.length()); ++row) {
    int end = offset + 58;
    if (end > static_cast<int>(message.length())) end = message.length();
    display->setCursor(24, top + 18 * row);
    display->print(message.substring(offset, end));
    offset = end;
  }
}

void drawHistoryScreen() {
  if (!displayReady) return;
  portENTER_CRITICAL(&historyMux);
  const int count = historySheetTab ? historyCache.sheetCount : historyCache.orderCount;
  portEXIT_CRITICAL(&historyMux);

  display->fillScreen(WHITE);
  display->setTextColor(BLACK);
  display->setTextSize(2);
  display->setCursor(16, 13);
  display->print("HISTORIAL");
  drawButton(358, 5, 108, 37, "VOLVER", WHITE);
  drawButton(14, 52, 220, 42, "ORDEN 3C", WHITE);
  drawButton(246, 52, 220, 42, "SHEETS", WHITE);
  display->setTextSize(1);
  display->setCursor(16, 103);
  display->print(historySheetTab ? "Cambios ejecutados" : "Ordenes / vista previa");

  if (historyDetailOpen && historySelected < count) {
    HistoryItem item{};
    portENTER_CRITICAL(&historyMux);
    item = historySheetTab ? historyCache.sheets[historySelected] : historyCache.orders[historySelected];
    portEXIT_CRITICAL(&historyMux);
    display->drawRoundRect(14, 124, 452, 288, 12, BLACK);
    display->setTextSize(2);
    display->setCursor(24, 145);
    display->print(historySheetTab ? "SHEETS APLICADO" : "ORDEN 3C");
    drawHistoryWrapped(String(item.when), 180, 1);
    drawHistoryWrapped(String("Estado: ") + item.status, 210, 2);
    drawHistoryWrapped(String(item.text), 255, 4);
    drawHistoryWrapped(String("Detalle: ") + item.preview, 348, 3);
    display->setCursor(24, 397);
    display->print("Toque para regresar a la lista");
  } else if (count == 0) {
    display->setTextSize(2);
    display->setCursor(25, 185);
    display->print(historyLoading ? "Consultando..." : "Sin registros");
    drawHistoryWrapped(String(panelDetail), 230, 2);
  } else {
    for (int row = 0; row < 3; ++row) {
      const int index = historyPage * 3 + row;
      if (index >= count) break;
      HistoryItem item{};
      portENTER_CRITICAL(&historyMux);
      item = historySheetTab ? historyCache.sheets[index] : historyCache.orders[index];
      portEXIT_CRITICAL(&historyMux);
      const int y = 126 + 94 * row;
      display->drawRoundRect(14, y, 452, 87, 10, BLACK);
      display->setTextSize(1);
      display->setCursor(26, y + 9);
      display->print(String(item.when).substring(0, 16));
      display->setCursor(232, y + 9);
      display->print(String(item.status).substring(0, 23));
      drawHistoryWrapped(String(item.text), y + 27, 2);
      display->setTextSize(1);
      display->setCursor(24, y + 75);
      display->print(String(item.preview).substring(0, 60));
    }
  }
  drawButton(14, 426, 138, 42, "ANT", WHITE);
  drawButton(162, 426, 156, 42, "ACTUALIZAR", WHITE);
  drawButton(328, 426, 138, 42, "SIG", WHITE);
}

void showHistoryScreen(bool sheets) {
  historyScreenOpen = true;
  historySheetTab = sheets;
  historyDetailOpen = false;
  historyPage = 0;
  historyLoading = true;
  drawHistoryScreen();
  queueNetworkRequest(NetworkAction::LoadHistory);
}

void handleHistoryTap(int x, int y) {
  if (y < 43 && x >= 350) {
    historyScreenOpen = false;
    historyDetailOpen = false;
    drawPanel();
  } else if (y >= 52 && y < 95) {
    historySheetTab = x >= 240;
    historyDetailOpen = false;
    historyPage = 0;
    drawHistoryScreen();
  } else if (y >= 426) {
    if (x >= 162 && x < 318) {
      historyLoading = true;
      drawHistoryScreen();
      queueNetworkRequest(NetworkAction::LoadHistory);
    } else if (x < 160 && historyPage > 0) {
      historyDetailOpen = false;
      --historyPage;
      drawHistoryScreen();
    } else if (x >= 325 && historyPage < 2) {
      historyDetailOpen = false;
      ++historyPage;
      drawHistoryScreen();
    }
  } else if (y >= 126 && y < 413) {
    if (historyDetailOpen) {
      historyDetailOpen = false;
    } else {
      const int row = (y - 126) / 94;
      portENTER_CRITICAL(&historyMux);
      const int count = historySheetTab ? historyCache.sheetCount : historyCache.orderCount;
      portEXIT_CRITICAL(&historyMux);
      const int selected = historyPage * 3 + row;
      if (row >= 3 || selected >= count) return;
      historySelected = selected;
      historyDetailOpen = true;
    }
    drawHistoryScreen();
  }
}

void drawPanel() {
  if (otaCommitEditorOpen) {
    drawOtaCommitEditor();
    return;
  }
  if (historyScreenOpen) {
    drawHistoryScreen();
    return;
  }
  if (commandEditorOpen) {
    drawEditor();
    return;
  }
  if (!displayReady) return;

  drawHomeBackground();

  display->setTextColor(BLACK);
  display->setTextSize(3);
  display->setCursor(18, 14);
  display->print("Interfaz Portatil");
  display->setTextSize(1);
  display->setTextColor(BLACK);
  display->setCursor(20, 44);
  display->print("Opciones del sistema");

  const uint16_t statusColor = BLACK;
  display->fillCircle(438, 48, 5, statusColor);
  display->drawRoundRect(358, 7, 108, 30, 9, BLACK);
  display->setTextSize(1);
  display->setCursor(373, 18);
  display->print("HISTORIAL");

  drawHomeRow(62, "DB", "Conexion al backend",
              "Conectar y validar el sistema", homePanel == HomePanel::Backend);
  drawHomeRow(111, "GS", "Google Sheets",
              "Control y revision de cambios", homePanel == HomePanel::Sheets);
  drawHomeRow(160, "CI", "GitHub Actions",
              "Probar flujo y firmware", homePanel == HomePanel::Github);
  drawHomeRow(209, "FW", "Actualizar firmware",
              "Desplegar nueva version", homePanel == HomePanel::Firmware);

  if (homePanel == HomePanel::None) {
    drawHomeRow(266, "WF", "Wi-Fi 2.4 GHz", "", false, 34);
    drawHomeRow(302, "DB", "Nube Databricks", "", false, 34);
    drawHomeRow(338, "DX", "Diagnostico", "", false, 34);
    drawHomeRow(374, "ID", "Estado del dispositivo", "", false, 34);
  } else {
    drawExpandedPanel();
  }

  drawButton(14, 426, 220, 42, "PROBAR CLOUD", WHITE);
  drawButton(246, 426, 220, 42, "ENVIAR 3C", WHITE);
}

void playTone(uint16_t frequency, uint16_t durationMs) {
  if (!audioReady || !app_config::panelAudioEnabled || frequency == 0) return;
  constexpr uint32_t sampleRate = 16000;
  constexpr size_t framesPerChunk = 128;
  int16_t samples[framesPerChunk * 2];
  const uint32_t totalFrames = sampleRate * durationMs / 1000;
  uint32_t frame = 0;
  while (frame < totalFrames) {
    size_t frames = totalFrames - frame;
    if (frames > framesPerChunk) frames = framesPerChunk;
    for (size_t index = 0; index < frames; ++index) {
      const uint32_t phase = ((frame + index) * frequency * 2U) / sampleRate;
      const int16_t sample = (phase & 1U) ? 2600 : -2600;
      samples[index * 2] = sample;
      samples[index * 2 + 1] = sample;
    }
    size_t written = 0;
    i2s_write(I2S_NUM_0, samples, frames * 2 * sizeof(int16_t), &written, portMAX_DELAY);
    frame += frames;
  }
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void updatePanel(PanelState state, const String& detail, bool sound = false) {
  // The FreeRTOS network task must NEVER touch the RGB framebuffer.
  if (networkTaskHandle && xTaskGetCurrentTaskHandle() == networkTaskHandle) {
    if (uiQueue) {
      UiNotification notice{};
      notice.state = state;
      detail.toCharArray(notice.detail, sizeof(notice.detail));
      notice.sound = sound;
      xQueueOverwrite(uiQueue, &notice);
    }
    return;
  }
  const bool changed = state != panelState;
  panelState = state;
  panelDetail = detail;
  if ((!commandEditorOpen && !otaCommitEditorOpen) || historyScreenOpen) drawPanel();
  Serial.printf("PANEL STATE -> %s | %s\n", stateLabel(panelState), panelDetail.c_str());
  if (!sound || !changed) return;
  if (state == PanelState::Applied || state == PanelState::Ready) playTone(880, 70);
  else if (state == PanelState::Pending || state == PanelState::Busy) playTone(620, 55);
  else if (state == PanelState::Rejected || state == PanelState::Error) playTone(220, 110);
}

String normalizedStatus(String status) {
  status.trim();
  status.toLowerCase();
  return status;
}

void setTransportError(const char* phase, int code, const String& detail, bool clearCommand) {
  backendAvailable = false;
  if (clearCommand) { lastCommandId = ""; pendingCommand = false; }
  const String message = String(phase) + " HTTP " + code;
  updatePanel(PanelState::Error, message, true);
  Serial.printf("[ERROR] transport phase=%s code=%d detail=%s\n", phase, code, detail.c_str());
}

void setProtocolError(const char* phase, const String& detail) {
  backendAvailable = false;
  lastCommandId = "";
  pendingCommand = false;
  const String message = String(phase) + ": " + (detail.length() ? detail : "respuesta invalida");
  updatePanel(PanelState::Error, message, true);
  Serial.printf("[ERROR] protocol phase=%s detail=%s\n", phase, detail.c_str());
}

bool initializeAudio() {
  if (!app_config::panelAudioEnabled) {
    Serial.println("Audio deshabilitado: GPIO 1/2/40 reservados para relays.");
    return false;
  }
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = 16000;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 4;
  config.dma_buf_len = 128;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  i2s_pin_config_t pinConfig = {};
  pinConfig.bck_io_num = pins::audioBclk;
  pinConfig.ws_io_num = pins::audioLrclk;
  pinConfig.data_out_num = pins::audioData;
  pinConfig.data_in_num = I2S_PIN_NO_CHANGE;
  if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &pinConfig) != ESP_OK) {
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }
  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

#if !defined(PANEL_PRODUCTION_BUILD) || !PANEL_PRODUCTION_BUILD
void runDisplayDiagnostic() {
  if (!displayReady) return;
  Serial.println("DISPLAY DIAGNOSTIC: RED");
  display->fillScreen(color565(255, 0, 0));
  delay(400);
  Serial.println("DISPLAY DIAGNOSTIC: GREEN");
  display->fillScreen(color565(0, 255, 0));
  delay(400);
  Serial.println("DISPLAY DIAGNOSTIC: BLUE");
  display->fillScreen(color565(0, 0, 255));
  delay(400);
  Serial.println("DISPLAY DIAGNOSTIC: WHITE");
  display->fillScreen(color565(255, 255, 255));
  delay(400);
}
#endif

bool initializeDisplay() {
  Serial.println("DISPLAY: creating 9-bit SPI command bus");
  displayBus = new Arduino_ESP32SPI(
    GFX_NOT_DEFINED, pins::lcdCs, pins::lcdClock, pins::lcdMosi, GFX_NOT_DEFINED);
  Serial.println("DISPLAY: creating RGB panel 480x480");
  auto* rgbPanel = new Arduino_ESP32RGBPanel(
    pins::de, pins::vsync, pins::hsync, pins::pclk,
    pins::red[0], pins::red[1], pins::red[2], pins::red[3], pins::red[4],
    pins::green[0], pins::green[1], pins::green[2], pins::green[3], pins::green[4], pins::green[5],
    pins::blue[0], pins::blue[1], pins::blue[2], pins::blue[3], pins::blue[4],
    1, 10, 8, 50,
    1, 10, 8, 20,
    0, 12000000, false, 0, 0, 0);
  // Match the Guition ESP32-4848S040 reference: ST7701 type9,
  // rotation 1, 12 MHz RGB PCLK, RGB565 big-endian disabled.
  Serial.println("DISPLAY: using Arduino-GFX ST7701 type9 init sequence");
  display = new Arduino_RGB_Display(
    kScreenWidth, kScreenHeight, rgbPanel, 1, true,
    displayBus, GFX_NOT_DEFINED,
    st7701_type9_init_operations, sizeof(st7701_type9_init_operations));
  Serial.println("DISPLAY: calling display->begin()");
  if (!display->begin()) {
    Serial.println("DISPLAY: display->begin() FAILED");
    return false;
  }
  Serial.println("DISPLAY: display->begin() OK");
  pinMode(pins::backlight, OUTPUT);
  analogWrite(pins::backlight, app_config::panelBrightness);
  Serial.printf("DISPLAY: backlight GPIO %d PWM=%u\n", pins::backlight, app_config::panelBrightness);
  display->displayOn();
  Serial.println("DISPLAY: displayOn() OK");
  displayReady = true;
#if !defined(PANEL_PRODUCTION_BUILD) || !PANEL_PRODUCTION_BUILD
  runDisplayDiagnostic();
#else
  Serial.println("DISPLAY: production build; startup RGB diagnostic disabled");
#endif
  drawPanel();
  Serial.println("DISPLAY: first UI frame drawn");
  return true;
}

bool i2cRead(uint16_t reg, uint8_t* data, size_t length) {
  Wire.beginTransmission(kTouchAddress);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(kTouchAddress, static_cast<uint8_t>(length)) != length) return false;
  for (size_t index = 0; index < length; ++index) data[index] = Wire.read();
  return true;
}

bool i2cWriteByte(uint16_t reg, uint8_t value) {
  Wire.beginTransmission(kTouchAddress);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

TouchSample readTouch() {
  TouchSample sample;
  uint8_t status = 0;
  if (!i2cRead(kTouchStatusRegister, &status, 1) || !(status & 0x80)) return sample;
  sample.ready = true;
  const uint8_t points = status & 0x0F;
  if (points > 0 && points <= 5) {
    uint8_t data[7] = {};
    if (i2cRead(kTouchPointRegister, data, sizeof(data))) {
      const uint16_t rawX = data[1] | (static_cast<uint16_t>(data[2]) << 8);
      const uint16_t rawY = data[3] | (static_cast<uint16_t>(data[4]) << 8);
      const auto mapped = touch_input::mapRaw(rawX, rawY);
      if (mapped.valid) {
        sample.x = static_cast<uint16_t>(mapped.x);
        sample.y = static_cast<uint16_t>(mapped.y);
        sample.touched = true;
        if (!touchTracker.active()) {
          Serial.printf("TOUCH raw=(%u,%u) mapped=(%u,%u)\n",
                        rawX, rawY, sample.x, sample.y);
        }
      }
    }
  }
  i2cWriteByte(kTouchStatusRegister, 0);
  return sample;
}

bool cloudEndpointConfigured() {
  return strlen(app_config::assistantBaseUrl) > 0;
}

bool databricksAppEndpoint() {
  if (!cloudEndpointConfigured()) return false;
  String base = app_config::assistantBaseUrl;
  base.toLowerCase();
  return base.indexOf(".databricksapps.com") >= 0;
}

String backendBaseUrl() {
  String base = app_config::assistantBaseUrl;
  base.trim();
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base;
}

String endpoint(const String& path) {
  const String base = backendBaseUrl();
  return base.length() ? base + path : String();
}

String jsonEscape(const String& input) {
  String output;
  output.reserve(input.length() + 16);
  for (size_t index = 0; index < input.length(); ++index) {
    const char value = input[index];
    if (value == '\\' || value == '"') { output += '\\'; output += value; }
    else if (value == '\n') output += "\\n";
    else if (static_cast<uint8_t>(value) >= 0x20) output += value;
  }
  return output;
}

unsigned long jsonUnsignedLongValue(
    const String& json, const char* key, unsigned long fallback) {
  const String token = String("\"") + key + "\"";
  int position = json.indexOf(token);
  if (position < 0) return fallback;
  position = json.indexOf(':', position + token.length());
  if (position < 0) return fallback;
  position++;
  while (position < static_cast<int>(json.length()) && isspace(json[position])) position++;
  String digits;
  while (position < static_cast<int>(json.length()) && isdigit(json[position])) digits += json[position++];
  return digits.length() ? static_cast<unsigned long>(digits.toInt()) : fallback;
}

String jsonStringValue(const String& json, const char* key) {
  const String token = String("\"") + key + "\"";
  int position = json.indexOf(token);
  if (position < 0) return "";
  position = json.indexOf(':', position + token.length());
  if (position < 0) return "";
  position++;
  while (position < static_cast<int>(json.length()) && isspace(json[position])) position++;
  if (position >= static_cast<int>(json.length()) || json[position] != '"') return "";
  position++;
  String value;
  while (position < static_cast<int>(json.length())) {
    const char current = json[position++];
    if (current == '"') break;
    if (current == '\\' && position < static_cast<int>(json.length())) value += json[position++];
    else value += current;
  }
  return value;
}

void clearDatabricksAccessToken() {
  databricksAccessToken = "";
  databricksTokenAcquiredMs = 0;
  databricksTokenLifetimeMs = 0;
}

bool databricksOAuthConfigured() {
  return strlen(app_config::databricksWorkspaceUrl) > 0 &&
         runtimeDatabricksClientId.length() > 0 &&
         runtimeDatabricksClientSecret.length() > 0;
}

bool ensureDatabricksAccessToken() {
  if (!databricksAppEndpoint()) return true;
  if (!databricksOAuthConfigured()) {
    lastBackendMessage = "Databricks OAuth M2M no configurado";
    return false;
  }
  if (databricksAccessToken.length() &&
      millis() - databricksTokenAcquiredMs < databricksTokenLifetimeMs) return true;

  String workspace = app_config::databricksWorkspaceUrl;
  workspace.trim();
  while (workspace.endsWith("/")) workspace.remove(workspace.length() - 1);
  const String tokenUrl = workspace + "/oidc/v1/token";

  HTTPClient authHttp;
  authHttp.setTimeout(app_config::httpTimeoutMs);
  if (!authHttp.begin(tokenUrl)) {
    lastBackendMessage = "No se pudo abrir OAuth Databricks";
    return false;
  }
  authHttp.setAuthorization(runtimeDatabricksClientId.c_str(), runtimeDatabricksClientSecret.c_str());
  authHttp.addHeader("Content-Type", "application/x-www-form-urlencoded");
  const String form = String("grant_type=client_credentials&scope=") + app_config::databricksOauthScope;
  const int code = authHttp.POST(form);
  const String body = code > 0 ? authHttp.getString() : authHttp.errorToString(code);
  authHttp.end();

  if (code != 200) {
    clearDatabricksAccessToken();
    lastBackendMessage = String("OAuth Databricks HTTP ") + code;
    Serial.printf("[ERROR] OAuth Databricks HTTP=%d (respuesta omitida)\n", code);
    return false;
  }

  const String token = jsonStringValue(body, "access_token");
  const unsigned long expiresSeconds = jsonUnsignedLongValue(body, "expires_in", 3600UL);
  if (!token.length()) {
    clearDatabricksAccessToken();
    lastBackendMessage = "OAuth Databricks sin access_token";
    return false;
  }

  databricksAccessToken = token;
  databricksTokenAcquiredMs = millis();
  const unsigned long rawLifetimeMs = expiresSeconds * 1000UL;
  databricksTokenLifetimeMs =
      rawLifetimeMs > app_config::oauthRefreshSkewMs
          ? rawLifetimeMs - app_config::oauthRefreshSkewMs
          : rawLifetimeMs / 2UL;
  lastBackendMessage = "OAuth Databricks renovado";
  Serial.printf("DATABRICKS: OAuth M2M listo; expires_in=%lu s\n", expiresSeconds);
  return true;
}

void addRequestAuth(HTTPClient& http) {
  if (databricksAccessToken.length()) {
    http.addHeader("Authorization", String("Bearer ") + databricksAccessToken);
  }
  if (runtimeApiToken.length()) {
    http.addHeader("X-3C-Device-Token", runtimeApiToken);
  }
}

bool checkBackendHealthOnce() {
  if (!backendBaseUrl().length()) {
    backendAvailable = false;
    lastBackendMessage = "Sin endpoint configurado";
    updatePanel(PanelState::Error, "Endpoint no configurado", true);
    return false;
  }
  if (!ensureDatabricksAccessToken()) {
    backendAvailable = false;
    updatePanel(PanelState::Error, lastBackendMessage, true);
    return false;
  }

  updatePanel(PanelState::Busy,
    databricksAppEndpoint() ? "Verificando Databricks" : "Verificando endpoint 3C");
  HTTPClient http;
  http.setTimeout(app_config::httpTimeoutMs);
  const String url = endpoint("/api/device/v1/health");
  if (!http.begin(url)) {
    lastBackendMessage = "No se pudo abrir URL " + url;
    http.end();
    backendAvailable = false;
    updatePanel(PanelState::Error, lastBackendMessage, true);
    Serial.printf("[ERROR] health begin failed url=%s\n", url.c_str());
    return false;
  }

  addRequestAuth(http);
  const int code = http.GET();
  lastBackendMessage = code > 0 ? http.getString() : http.errorToString(code);
  http.end();
  if (code == 401 && databricksAppEndpoint()) clearDatabricksAccessToken();
  backendAvailable = code == 200;
  updatePanel(
    backendAvailable ? PanelState::Ready : PanelState::Error,
    backendAvailable
      ? (databricksAppEndpoint() ? "Databricks conectado" : "Endpoint 3C conectado")
      : String("Health HTTP ") + code,
    true);
  if (!backendAvailable) {
    Serial.printf("[ERROR] health HTTP=%d endpoint=%s detail=%s\n",
      code, backendBaseUrl().c_str(), lastBackendMessage.c_str());
  }
  Serial.printf("GET health -> %d %s endpoint=%s\n",
    code, lastBackendMessage.c_str(), backendBaseUrl().c_str());
  return backendAvailable;
}

bool checkBackendHealth() {
  if (WiFi.status() != WL_CONNECTED) {
    backendAvailable = false;
    updatePanel(PanelState::Offline, "Sin acceso a Internet", true);
    return false;
  }
  return checkBackendHealthOnce();
}

bool checkGoogleSheetsVerify() {
  if (WiFi.status() != WL_CONNECTED) {
    updatePanel(PanelState::Offline, "Wi-Fi desconectado", true);
    return false;
  }
  if (!backendBaseUrl().length() || !ensureDatabricksAccessToken()) {
    updatePanel(PanelState::Error, "Backend no disponible", true);
    return false;
  }

  HTTPClient http;
  http.setTimeout(app_config::httpTimeoutMs);
  if (!http.begin(endpoint("/api/sheet/verify"))) {
    updatePanel(PanelState::Error, "No se pudo verificar Sheets", true);
    return false;
  }
  addRequestAuth(http);
  const int code = http.GET();
  lastBackendMessage = code > 0 ? http.getString() : http.errorToString(code);
  http.end();
  if (code == 401 && databricksAppEndpoint()) clearDatabricksAccessToken();

  if (code == 200) {
    backendAvailable = true;
    updatePanel(PanelState::Ready, "Cloud + Google Sheets OK", true);
    return true;
  }
  updatePanel(PanelState::Error, String("Sheets HTTP ") + code, true);
  return false;
}

bool checkCloudStack() {
  if (!checkBackendHealth()) return false;
  return checkGoogleSheetsVerify();
}

void fetchHistory() {
  if (WiFi.status() != WL_CONNECTED || !ensureDatabricksAccessToken()) {
    historyLoading = false;
    updatePanel(PanelState::Offline, "Historial: sin conexion");
    return;
  }
  HTTPClient http;
  http.setTimeout(app_config::httpTimeoutMs);
  if (!http.begin(endpoint("/api/device/v1/history/panel?device_id=" +
                             String(app_config::deviceId) + "&limit=8"))) {
    historyLoading = false;
    updatePanel(PanelState::Error, "Historial: URL invalida");
    return;
  }
  addRequestAuth(http);
  const int code = http.GET();
  const String body = code == 200 ? http.getString() : "";
  http.end();
  if (code == 401 && databricksAppEndpoint()) clearDatabricksAccessToken();
  if (code != 200) {
    historyLoading = false;
    updatePanel(PanelState::Error, String("Historial HTTP ") + code);
    return;
  }
  HistorySnapshot loaded{};
  int position = 0;
  while (position < static_cast<int>(body.length())) {
    const int newline = body.indexOf('\n', position);
    const String line = body.substring(position, newline < 0 ? body.length() : newline);
    position = newline < 0 ? body.length() : newline + 1;
    if (!line.length()) continue;
    String fields[5];
    int start = 0;
    for (int i = 0; i < 5; ++i) {
      const int tab = line.indexOf('\t', start);
      fields[i] = line.substring(start, tab < 0 ? line.length() : tab);
      if (tab < 0) break;
      start = tab + 1;
    }
    HistoryItem* item = nullptr;
    if (fields[0] == "O" && loaded.orderCount < kHistoryCapacity)
      item = &loaded.orders[loaded.orderCount++];
    if (fields[0] == "S" && loaded.sheetCount < kHistoryCapacity)
      item = &loaded.sheets[loaded.sheetCount++];
    if (!item) continue;
    fields[1].toCharArray(item->when, sizeof(item->when));
    fields[2].toCharArray(item->status, sizeof(item->status));
    fields[3].toCharArray(item->text, sizeof(item->text));
    fields[4].toCharArray(item->preview, sizeof(item->preview));
  }
  portENTER_CRITICAL(&historyMux);
  historyCache = loaded;
  portEXIT_CRITICAL(&historyMux);
  historyLoading = false;
  Serial.printf("HISTORY 3C -> %u orders, %u Sheets changes\n",
    loaded.orderCount, loaded.sheetCount);
  updatePanel(PanelState::Ready, "Historial actualizado");
}


bool checkOtaCommit(const String& rawCommit) {
  String sha = rawCommit;
  sha.trim();
  sha.toLowerCase();
  if (!validCommitSha(sha)) {
    setOtaStatus("SHA invalido (7-40 hex)", true);
    updatePanel(PanelState::Error, "Commit OTA invalido");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED || !ensureDatabricksAccessToken()) {
    setOtaStatus("Sin Wi-Fi / OAuth Databricks", true);
    updatePanel(PanelState::Offline, "OTA: sin conexion");
    return false;
  }
  setOtaStatus("Consultando publicacion OTA...", true);
  updatePanel(PanelState::Busy, "Buscando firmware OTA");
  HTTPClient http;
  http.setTimeout(app_config::httpTimeoutMs);
  if (!http.begin(endpoint("/api/device/v1/firmware/by-commit/" + sha))) {
    setOtaStatus("No se pudo abrir OTA", true);
    updatePanel(PanelState::Error, "OTA URL invalida");
    return false;
  }
  addRequestAuth(http);
  const int code = http.GET();
  const String body = code == 200 ? http.getString() : "";
  http.end();
  if (code == 401 && databricksAppEndpoint()) clearDatabricksAccessToken();
  if (code != 200) {
    setOtaStatus(code == 404 ? "Commit sin OTA publicada" :
                 String("OTA consulta HTTP ") + code, true);
    updatePanel(PanelState::Error, String("OTA HTTP ") + code);
    Serial.printf("OTA LOOKUP HTTP=%d commit=%s\n", code, sha.c_str());
    return false;
  }
  const String fullSha = jsonStringValue(body, "source_sha");
  const String version = jsonStringValue(body, "version");
  const String checksum = jsonStringValue(body, "sha256");
  const String path = jsonStringValue(body, "url");
  const size_t fileSize = jsonUnsignedLongValue(body, "size", 0UL);
  if (fullSha.length() != 40 || !fullSha.startsWith(sha) ||
      !validSha256(checksum) ||
      !path.startsWith("/api/device/v1/firmware/") || !path.endsWith(".bin") ||
      fileSize < 65536 || fileSize > 0x400000 ||
      otaVersionCompare(version.c_str(), kFirmwareVersion) <= 0) {
    setOtaStatus("Build no apta, antigua o invalida", true);
    updatePanel(PanelState::Error, "OTA: version/manifest invalido");
    return false;
  }
  OtaSelection candidate{};
  candidate.ready = true;
  fullSha.toCharArray(candidate.commit, sizeof(candidate.commit));
  version.toCharArray(candidate.version, sizeof(candidate.version));
  checksum.toCharArray(candidate.sha256, sizeof(candidate.sha256));
  path.toCharArray(candidate.url, sizeof(candidate.url));
  candidate.size = fileSize;
  const String message = String("OTA ") + version + " lista; pulse INSTALAR";
  message.toCharArray(candidate.message, sizeof(candidate.message));
  portENTER_CRITICAL(&otaMux);
  otaSelection = candidate;
  portEXIT_CRITICAL(&otaMux);
  Serial.printf("OTA VERIFIED commit=%s version=%s size=%u\n",
                fullSha.c_str(), version.c_str(), static_cast<unsigned>(fileSize));
  updatePanel(PanelState::Ready, "OTA lista para instalar");
  return true;
}

bool installSelectedOta() {
  OtaSelection selected{};
  portENTER_CRITICAL(&otaMux);
  selected = otaSelection;
  portEXIT_CRITICAL(&otaMux);
  if (!selected.ready || !validSha256(String(selected.sha256)) ||
      !String(selected.url).startsWith("/api/device/v1/firmware/") ||
      !String(selected.url).endsWith(".bin") ||
      selected.size < 65536 || selected.size > 0x400000) {
    setOtaStatus("Seleccione y verifique commit", true);
    updatePanel(PanelState::Error, "OTA sin manifest valido");
    return false;
  }
  if (!esp_ota_get_next_update_partition(nullptr)) {
    setOtaStatus("Sin particion OTA; usar USB", true);
    updatePanel(PanelState::Error, "OTA necesita instalacion USB");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED || !ensureDatabricksAccessToken()) {
    setOtaStatus("Red no disponible", true);
    updatePanel(PanelState::Offline, "OTA sin conexion");
    return false;
  }
  const String url = endpoint(String(selected.url));
  HTTPClient http;
  http.setTimeout(30000);
  if (!http.begin(url)) {
    setOtaStatus("URL de descarga invalida", true);
    updatePanel(PanelState::Error, "OTA URL invalida");
    return false;
  }
  addRequestAuth(http);
  const int code = http.GET();
  if (code != 200) {
    if (code == 401 && databricksAppEndpoint()) clearDatabricksAccessToken();
    http.end();
    setOtaStatus(String("Descarga HTTP ") + code, true);
    updatePanel(PanelState::Error, String("OTA descarga HTTP ") + code);
    return false;
  }
  const int contentLength = http.getSize();
  if (contentLength <= 0 || static_cast<size_t>(contentLength) != selected.size) {
    http.end();
    setOtaStatus("Tamano OTA no coincide", true);
    updatePanel(PanelState::Error, "OTA tamano invalido");
    return false;
  }
  updatePanel(PanelState::Busy, "OTA: descargando y verificando");
  if (!Update.begin(selected.size, U_FLASH)) {
    http.end();
    setOtaStatus("Update.begin fallo; usar USB", true);
    updatePanel(PanelState::Error, "OTA particion insuficiente");
    return false;
  }
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  bool ok = mbedtls_sha256_starts_ret(&sha, 0) == 0;
  WiFiClient* stream = http.getStreamPtr();
  uint8_t data[2048]{};
  size_t received = 0;
  unsigned long progress = millis();
  while (ok && received < selected.size) {
    const int available = stream->available();
    if (available <= 0) {
      if (!http.connected() || millis() - progress > 30000UL) { ok = false; break; }
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    size_t length = static_cast<size_t>(available);
    if (length > sizeof(data)) length = sizeof(data);
    if (length > selected.size - received) length = selected.size - received;
    const int n = stream->readBytes(data, length);
    if (n <= 0 || mbedtls_sha256_update_ret(&sha, data, n) != 0 ||
        Update.write(data, n) != static_cast<size_t>(n)) {
      ok = false;
      break;
    }
    received += static_cast<size_t>(n);
    progress = millis();
  }
  unsigned char digest[32]{};
  if (ok) ok = (mbedtls_sha256_finish_ret(&sha, digest) == 0);
  mbedtls_sha256_free(&sha);
  http.end();
  char hashText[65]{};
  for (int i = 0; i < 32; ++i) {
    snprintf(hashText + i * 2, 3, "%02x", digest[i]);
  }
  if (!ok || received != selected.size ||
      !String(hashText).equalsIgnoreCase(selected.sha256)) {
    Update.abort();
    setOtaStatus("Descarga incompleta o SHA-256 incorrecto", true);
    updatePanel(PanelState::Error, "OTA abortada, firmware intacto");
    return false;
  }
  if (!Update.end(true)) {
    Update.abort();
    setOtaStatus("No se pudo activar particion OTA", true);
    updatePanel(PanelState::Error, "Update.end fallo");
    return false;
  }
  setOtaStatus("OTA verificada; reiniciando");
  updatePanel(PanelState::Ready, "OTA SHA-256 OK; reiniciando");
  Serial.printf("OTA SUCCESS version=%s source_sha=%s bytes=%u\n",
                selected.version, selected.commit, static_cast<unsigned>(received));
  vTaskDelay(pdMS_TO_TICKS(900));
  ESP.restart();
  return true;
}

int send3CCommand(const String& rawCommand) {
  String command = rawCommand;
  command.trim();
  if (!command.length()) {
    updatePanel(PanelState::Error, "Comando vacio", true);
    return 400;
  }
  if (WiFi.status() != WL_CONNECTED) {
    updatePanel(PanelState::Offline, "Sin acceso a Internet", true);
    return 503;
  }

  updatePanel(PanelState::Busy, "Enviando a Databricks", true);
  if (!backendBaseUrl().length()) {
    updatePanel(PanelState::Error, "Endpoint no configurado", true);
    return 503;
  }
  if (!ensureDatabricksAccessToken()) {
    updatePanel(PanelState::Error, lastBackendMessage, true);
    return 503;
  }

  HTTPClient http;
  http.setTimeout(app_config::httpTimeoutMs);
  if (!http.begin(endpoint("/api/device/v1/commands"))) {
    updatePanel(PanelState::Error, "No se pudo abrir endpoint 3C", true);
    return 503;
  }
  http.addHeader("Content-Type", "application/json");
  addRequestAuth(http);

  char randomPart[9];
  snprintf(randomPart, sizeof(randomPart), "%08lx", static_cast<unsigned long>(esp_random()));
  const String requestId = String(app_config::deviceId) + "-" + randomPart + "-" + String(millis());
  const String body = "{\"device_id\":\"" + jsonEscape(app_config::deviceId) +
    "\",\"request_id\":\"" + jsonEscape(requestId) +
    "\",\"text\":\"" + jsonEscape(command) + "\"}";
  const int code = http.POST(body);
  lastBackendMessage = code > 0 ? http.getString() : http.errorToString(code);
  http.end();
  if (code == 401 && databricksAppEndpoint()) clearDatabricksAccessToken();

  if (code == 200 || code == 202) {
    backendAvailable = true;
    lastCommandId = jsonStringValue(lastBackendMessage, "command_id");
    if (!lastCommandId.length()) {
      setProtocolError("POST", "falta command_id");
      Serial.printf("POST 3C -> %d %s\n", code, lastBackendMessage.c_str());
      return code;
    }
    lastCommandPoll = millis();
    pendingCommand = true;
    updatePanel(PanelState::Pending, "CONFIRMACIÓN REQUERIDA EN WEB", true);
  } else {
    setTransportError("POST", code, lastBackendMessage, true);
  }
  Serial.printf("POST 3C -> %d %s\n", code, lastBackendMessage.c_str());
  return code;
}

void pollCommandStatus() {
  if (!lastCommandId.length() || WiFi.status() != WL_CONNECTED) return;
  if (!backendBaseUrl().length()) return;
  if (!ensureDatabricksAccessToken()) {
    setProtocolError("OAUTH", lastBackendMessage);
    return;
  }
  HTTPClient http;
  http.setTimeout(app_config::httpTimeoutMs);
  if (!http.begin(endpoint("/api/device/v1/commands/" + lastCommandId))) {
    setTransportError("POLL", -1, "No se pudo abrir endpoint 3C", true);
    return;
  }
  addRequestAuth(http);
  const int code = http.GET();
  const String body = code > 0 ? http.getString() : http.errorToString(code);
  lastBackendMessage = body;
  http.end();
  if (code == 401 && databricksAppEndpoint()) clearDatabricksAccessToken();
  if (code != 200) {
    setTransportError("POLL", code, body, true);
    return;
  }

  String status = normalizedStatus(jsonStringValue(body, "status"));
  const String result = jsonStringValue(body, "result");
  Serial.printf("GET command status -> %d status=%s result=%s\n", code, status.c_str(), result.c_str());

  if (status == "applied") {
    updatePanel(PanelState::Applied, result.length() ? result : "Confirmado en backend 3C", true);
    lastCommandId = "";
    pendingCommand = false;
  } else if (status == "rejected") {
    updatePanel(PanelState::Rejected, result.length() ? result : "Rechazado en backend 3C", true);
    lastCommandId = "";
    pendingCommand = false;
  } else if (status == "error" || status == "failed" || status == "fallido") {
    setProtocolError("POLL", result.length() ? result : "Error reportado por backend 3C");
  } else if (status == "pending_confirmation" || status == "pending" || status == "pendiente") {
    backendAvailable = true;
    updatePanel(PanelState::Pending, "CONFIRMACIÓN REQUERIDA EN WEB");
  } else {
    setProtocolError("POLL", status.length() ? String("estado desconocido '") + status + "'" : "falta status");
  }
}


bool queueNetworkRequest(NetworkAction action, const String& command) {
  if (!networkQueue) {
    updatePanel(PanelState::Error, "Red no inicializada");
    return false;
  }
  NetworkRequest request{};
  request.action = action;
  if (action == NetworkAction::Send3C || action == NetworkAction::CheckOtaCommit) {
    if (!command.length() || command.length() >= sizeof(request.command)) {
      updatePanel(PanelState::Error, "Orden 3C vacia o muy larga");
      return false;
    }
    command.toCharArray(request.command, sizeof(request.command));
  }
  if (xQueueSend(networkQueue, &request, 0) != pdTRUE) {
    updatePanel(PanelState::Error, "Solicitudes en espera; intente nuevamente");
    return false;
  }
  updatePanel(PanelState::Busy,
              action == NetworkAction::Send3C ? "Orden 3C en cola" :
              action == NetworkAction::CheckOtaCommit ? "Consulta OTA en cola" :
              action == NetworkAction::InstallOta ? "Instalacion OTA en cola" :
              "Consulta cloud en cola");
  return true;
}

void processNetworkUiUpdates() {
  if (!uiQueue) return;
  UiNotification notice{};
  if (xQueueReceive(uiQueue, &notice, 0) == pdTRUE) {
    updatePanel(notice.state, String(notice.detail), notice.sound);
  }
}

void networkWorker(void* parameter) {
  (void)parameter;
  // Owns OAuth, Databricks HTTPS, Sheets verification and 3C status polling.
  bool sawWifi = false;
  for (;;) {
    NetworkRequest request{};
    if (xQueueReceive(networkQueue, &request, pdMS_TO_TICKS(40)) == pdTRUE) {
      switch (request.action) {
        case NetworkAction::Health:
          checkBackendHealth();
          break;
        case NetworkAction::CloudAndSheets:
          checkCloudStack();
          break;
        case NetworkAction::Send3C:
          send3CCommand(String(request.command));
          break;
        case NetworkAction::LoadHistory:
          fetchHistory();
          break;
        case NetworkAction::CheckOtaCommit:
          checkOtaCommit(String(request.command));
          break;
        case NetworkAction::InstallOta:
          installSelectedOta();
          break;
      }
      lastHealthCheck = millis();
    }
    if (WiFi.status() != WL_CONNECTED) {
      sawWifi = false;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    if (!sawWifi) {
      sawWifi = true;
      lastHealthCheck = millis();
      checkBackendHealth();
      continue;
    }
    if (lastCommandId.length() &&
        millis() - lastCommandPoll >= app_config::commandPollMs) {
      lastCommandPoll = millis();
      pollCommandStatus();
    } else if (!lastCommandId.length() &&
               millis() - lastHealthCheck >= app_config::healthCheckMs) {
      lastHealthCheck = millis();
      checkBackendHealth();
    }
  }
}

const char controlPage[] PROGMEM = R"HTML(
<!doctype html><html lang="es"><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:system-ui;max-width:680px;margin:auto;padding:24px;background:#eef3f7}section{background:white;padding:20px;border-radius:16px;box-shadow:0 5px 20px #0001}button,textarea{font:inherit}button{padding:13px 18px;border:0;border-radius:10px;background:#08784f;color:white}textarea{box-sizing:border-box;width:100%;min-height:120px;padding:12px;margin:8px 0 12px}.warn{color:#805500}</style>
<h1>Panel ESP32-4848S040 3C</h1><section><p class="warn">La orden se envía a Databricks Apps y queda pendiente de confirmación humana. Google Sheets cambia solo después de la confirmación web.</p><textarea id="text" placeholder="Cambia la tarea J10 a mensual"></textarea><button onclick="send3c()">Enviar al asistente</button><button onclick="health()">Probar Databricks</button><pre id="result"></pre></section>
<script>async function send3c(){const b=new URLSearchParams({text:document.querySelector('#text').value});const r=await fetch('/api/3c',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});result.textContent=r.status+' '+await r.text()}async function health(){const r=await fetch('/api/backend-health',{method:'POST'});result.textContent=r.status+' '+await r.text()}</script></html>
)HTML";

void configureWebServer() {
  web.on("/", HTTP_GET, [] { web.send_P(200, "text/html; charset=utf-8", controlPage); });
  web.on("/health", HTTP_GET, [] {
    const String body = String("{\"ok\":true,\"board\":\"ESP32-4848S040\",\"wifi\":") +
      (WiFi.status() == WL_CONNECTED ? "true" : "false") +
      ",\"backend\":" + (backendAvailable ? "true" : "false") +
      ",\"pending\":" + (pendingCommand ? "true" : "false") +
      ",\"transport\":\"internet\"}";
    web.send(200, "application/json", body);
  });
  web.on("/api/backend-health", HTTP_POST, [] {
        const bool queued = queueNetworkRequest(NetworkAction::Health);
    web.send(queued ? 202 : 503, "application/json",
             queued ? "{\"status\":\"queued\"}" : "{\"error\":\"queue_full\"}");
  });
  web.on("/api/3c", HTTP_POST, [] {
    app_config::commandBuffer = web.arg("text");
    const bool queued = queueNetworkRequest(NetworkAction::Send3C, app_config::commandBuffer);
    web.send(queued ? 202 : 503, "application/json",
             queued ? "{\"status\":\"queued\"}" : "{\"error\":\"queue_full\"}");
  });
  web.onNotFound([] { web.send(404, "application/json", "{\"error\":\"not found\"}"); });
  web.begin();
}

const char* wifiStatusLabel(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD: return "NO_SHIELD";
    case WL_IDLE_STATUS: return "IDLE";
    case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
    case WL_CONNECTED: return "CONNECTED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    default: return "UNKNOWN";
  }
}

void configureWifi() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(app_config::deviceId);
}

void connectWifi() {
  if (!runtimeWifiSsid.length()) {
    updatePanel(PanelState::Offline, "Configure local_config.h");
    Serial.println("Configure include/local_config.h antes de usar Wi-Fi.");
    return;
  }
  configureWifi();
  Serial.printf("Wi-Fi: iniciando STA, credenciales presentes, status=%d (%s)\n",
    static_cast<int>(WiFi.status()), wifiStatusLabel(WiFi.status()));
  WiFi.begin(runtimeWifiSsid.c_str(), runtimeWifiPassword.c_str());
  lastWifiAttempt = millis();
  updatePanel(PanelState::Busy, "Conectando Wi-Fi");
}

void handleTouch() {
  const TouchSample sample = readTouch();
  touch_input::Point tap{};
  if (sample.ready) {
    if (sample.touched) lastTouchActivityMs = millis();
    if (!touchTracker.update(sample.touched, sample.x, sample.y, &tap)) return;
  } else {
    // GT911 may omit the release frame; synthesize after its stream goes idle.
    if (!touchTracker.active() || millis() - lastTouchActivityMs < kMissingReleaseMs) return;
    if (!touchTracker.update(false, 0, 0, &tap)) return;
  }
  if (millis() - lastHandledTapMs < kTouchDebounceMs) return;
  lastHandledTapMs = millis();
  {
    if (otaCommitEditorOpen) {
      virtual_keyboard::Key key{};
      if (virtual_keyboard::hitTest(keyboardMode, tap.x, tap.y, &key)) {
        using virtual_keyboard::KeyKind;
        switch (key.definition.kind) {
          case KeyKind::Character:
            otaCommitBuffer.insert(key.definition.label);
            drawOtaCommitField();
            break;
          case KeyKind::Backspace:
            otaCommitBuffer.backspace();
            drawOtaCommitField();
            break;
          case KeyKind::Space:
            break;  // a commit SHA never contains spaces
          case KeyKind::ToggleAlphaNumeric:
            keyboardMode = keyboardMode == virtual_keyboard::KeyboardMode::Alpha
              ? virtual_keyboard::KeyboardMode::NumericSymbols :
                virtual_keyboard::KeyboardMode::Alpha;
            drawEditorKeyboard();
            break;
          case KeyKind::Enter: {
            String typed = otaCommitBuffer.c_str();
            typed.trim();
            typed.toLowerCase();
            if (!validCommitSha(typed)) {
              setOtaStatus("SHA invalido: 7-40 hex", true);
              drawOtaCommitField();
              break;
            }
            otaCommitBuffer.set(typed.c_str());
            otaConfirmArmed = false;
            otaCommitEditorOpen = false;
            homePanel = HomePanel::Firmware;
            drawPanel();
            queueNetworkRequest(NetworkAction::CheckOtaCommit, typed);
            return;
          }
        }
      } else {
        switch (editor_ui::ToolbarComponent::hitTest(tap.x, tap.y)) {
          case editor_ui::ToolbarAction::Home:
            otaCommitEditorOpen = false;
            otaConfirmArmed = false;
            homePanel = HomePanel::Firmware;
            drawPanel();
            break;
          case editor_ui::ToolbarAction::MoveLeft:
            otaCommitBuffer.moveLeft(); drawOtaCommitField(); break;
          case editor_ui::ToolbarAction::MoveRight:
            otaCommitBuffer.moveRight(); drawOtaCommitField(); break;
          case editor_ui::ToolbarAction::DeleteForward:
            otaCommitBuffer.deleteForward(); drawOtaCommitField(); break;
          case editor_ui::ToolbarAction::Clear:
            otaCommitBuffer.clear(); drawOtaCommitField(); break;
          case editor_ui::ToolbarAction::None: break;
        }
      }
      return;
    }
    if (historyScreenOpen) {
      handleHistoryTap(tap.x, tap.y);
      return;
    }
    if (commandEditorOpen) {
      if (tap.y < 43 && tap.x >= 358) {
        showHistoryScreen(false);
        return;
      }
      virtual_keyboard::Key key{};
      if (virtual_keyboard::hitTest(keyboardMode, tap.x, tap.y, &key)) {
        using virtual_keyboard::KeyKind;
        switch (key.definition.kind) {
          case KeyKind::Character:
            commandBuffer.insert(key.definition.label);
            drawEditorTextField();
            break;
          case KeyKind::Backspace:
            commandBuffer.backspace();
            drawEditorTextField();
            break;
          case KeyKind::Space:
            commandBuffer.insert(' ');
            drawEditorTextField();
            break;
          case KeyKind::Enter:
            app_config::commandBuffer = commandBuffer.c_str();
            commandEditorOpen = false;
            drawPanel();
            queueNetworkRequest(NetworkAction::Send3C, app_config::commandBuffer);
            return;
          case KeyKind::ToggleAlphaNumeric:
            keyboardMode = keyboardMode == virtual_keyboard::KeyboardMode::Alpha
                ? virtual_keyboard::KeyboardMode::NumericSymbols
                : virtual_keyboard::KeyboardMode::Alpha;
            drawEditorKeyboard();
            break;
        }
      } else {
        const auto action = editor_ui::ToolbarComponent::hitTest(tap.x, tap.y);
        switch (action) {
          case editor_ui::ToolbarAction::Home:
            commandEditorOpen = false;
            homePanel = HomePanel::None;
            drawPanel();
            break;
          case editor_ui::ToolbarAction::MoveLeft:
            commandBuffer.moveLeft();
            drawEditorTextField();
            break;
          case editor_ui::ToolbarAction::MoveRight:
            commandBuffer.moveRight();
            drawEditorTextField();
            break;
          case editor_ui::ToolbarAction::DeleteForward:
            commandBuffer.deleteForward();
            drawEditorTextField();
            break;
          case editor_ui::ToolbarAction::Clear:
            commandBuffer.clear();
            drawEditorTextField();
            break;
          case editor_ui::ToolbarAction::None:
            if (editor_ui::EditorLayout::inTextField(tap.x, tap.y)) {
              String text(commandBuffer.c_str());
              if (text.length()) {
                display->setTextSize(2);
                size_t best = 0;
                uint16_t bestDistance = UINT16_MAX;
                const auto field = editor_ui::EditorLayout::textField();
                for (size_t i = 0; i <= text.length() && i <= kCommandCapacity; ++i) {
                  int16_t x1 = 0, y1 = 0; uint16_t w = 0, h = 0;
                  display->getTextBounds(text.substring(0, i), 0, 0, &x1, &y1, &w, &h);
                  const uint16_t distance = static_cast<uint16_t>(
                    abs(static_cast<int>(field.left + 10 + w) - static_cast<int>(tap.x)));
                  if (distance < bestDistance) { bestDistance = distance; best = i; }
                }
                commandBuffer.setCursor(best);
                drawEditorTextField();
              }
            }
            break;
        }
      }
    } else {
      if (homePanel == HomePanel::Firmware &&
          tap.y >= 361 && tap.y < 405) {
        if (tap.x >= 26 && tap.x < 228) {
          otaCommitEditorOpen = true;
          otaConfirmArmed = false;
          setOtaStatus("Ingrese SHA publicado", true);
          keyboardMode = virtual_keyboard::KeyboardMode::Alpha;
          drawOtaCommitEditor();
          return;
        }
        if (tap.x >= 239 && tap.x < 454) {
          OtaSelection selection{};
          portENTER_CRITICAL(&otaMux);
          selection = otaSelection;
          portEXIT_CRITICAL(&otaMux);
          if (!selection.ready) {
            queueNetworkRequest(NetworkAction::CheckOtaCommit,
                                String(otaCommitBuffer.c_str()));
          } else if (!otaConfirmArmed) {
            otaConfirmArmed = true;
            setOtaStatus("Confirmar instalacion OTA");
            drawPanel();
          } else {
            otaConfirmArmed = false;
            queueNetworkRequest(NetworkAction::InstallOta);
          }
          return;
        }
      }
      if (tap.y < 43 && tap.x >= 350) {
        showHistoryScreen(false);
        return;
      }
      auto togglePanel = [](HomePanel requested) {
        homePanel = homePanel == requested ? HomePanel::None : requested;
        drawPanel();
      };

      const auto action = home_ui::hitTest(
          tap.x, tap.y, homePanel != HomePanel::None);

      switch (action) {
        case home_ui::Action::TestCloud:
          homePanel = HomePanel::None;
          queueNetworkRequest(NetworkAction::CloudAndSheets);
          break;
        case home_ui::Action::Send3C:
          homePanel = HomePanel::None;
          commandEditorOpen = true;
          keyboardMode = virtual_keyboard::KeyboardMode::Alpha;
          drawEditor();
          break;
        case home_ui::Action::Backend:
          togglePanel(HomePanel::Backend);
          break;
        case home_ui::Action::Sheets:
          togglePanel(HomePanel::Sheets);
          break;
        case home_ui::Action::Github:
          togglePanel(HomePanel::Github);
          break;
        case home_ui::Action::Firmware:
          otaConfirmArmed = false;
          togglePanel(HomePanel::Firmware);
          break;
        case home_ui::Action::Wifi:
          togglePanel(HomePanel::Wifi);
          break;
        case home_ui::Action::Databricks:
          togglePanel(HomePanel::Databricks);
          break;
        case home_ui::Action::Diagnostics:
          togglePanel(HomePanel::Diagnostics);
          break;
        case home_ui::Action::Device:
          togglePanel(HomePanel::Device);
          break;
        case home_ui::Action::None:
          if (homePanel != HomePanel::None &&
              tap.x >= 14 && tap.x < 466 &&
              tap.y >= 268 && tap.y < 416) {
            homePanel = HomePanel::None;
            drawPanel();
          }
          break;
      }
    }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.printf("ESP32-4848S040 3C | PSRAM: %s | %u bytes\n",
    psramFound() ? "OK" : "NO", ESP.getPsramSize());

  displayReady = initializeDisplay();
  if (!displayReady) Serial.println("No se pudo inicializar la pantalla ST7701.");
  // Confirm a good OTA boot only after the RGB display initialized.
  const esp_partition_t* active = esp_ota_get_running_partition();
  if (active) {
    esp_ota_img_states_t otaState = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(active, &otaState) == ESP_OK &&
        otaState == ESP_OTA_IMG_PENDING_VERIFY && displayReady) {
      esp_ota_mark_app_valid_cancel_rollback();
      Serial.println("OTA boot confirmed: display OK");
    }
  }

  Serial.printf("GPIO MAP: BL=%d LCD_CS=%d LCD_CLK=%d LCD_MOSI=%d TOUCH_SDA=%d TOUCH_SCL=%d DE=%d VSYNC=%d HSYNC=%d PCLK=%d\n",
                pins::backlight, pins::lcdCs, pins::lcdClock, pins::lcdMosi,
                pins::touchSda, pins::touchScl, pins::de, pins::vsync, pins::hsync, pins::pclk);
  Wire.begin(pins::touchSda, pins::touchScl, 400000);
  Wire.beginTransmission(kTouchAddress);
  const uint8_t touchProbe = Wire.endTransmission();
  Serial.printf("GT911 I2C probe addr=0x%02X result=%u bus=400kHz\n", kTouchAddress, touchProbe);
  audioReady = initializeAudio();
  loadDeviceCredentials();
  commandBuffer.set(app_config::commandBuffer.c_str());
  networkQueue = xQueueCreate(4, sizeof(NetworkRequest));
  uiQueue = xQueueCreate(1, sizeof(UiNotification));
  if (!networkQueue || !uiQueue ||
      xTaskCreatePinnedToCore(networkWorker, "3c_network", 16384, nullptr, 1,
                              &networkTaskHandle, 0) != pdPASS) {
    Serial.println("[ERROR] no se pudo iniciar task HTTPS");
    updatePanel(PanelState::Error, "Task HTTPS no disponible");
  }
  updatePanel(PanelState::Booting, "Hardware inicializado; Databricks Cloud");
  playTone(520, 60);
  connectWifi();
  configureWebServer();
}

void loop() {
  processNetworkUiUpdates();
  web.handleClient();
  handleTouch();
  processNetworkUiUpdates();

  // Touch and display must not await OAuth, DNS, Databricks or Google Sheets.
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiAnnounced) {
      wifiAnnounced = true;
      Serial.printf("Red con Internet lista; Wi-Fi RSSI=%d dBm\n", WiFi.RSSI());
    }
  } else {
    wifiAnnounced = false;
    if (runtimeWifiSsid.length() &&
        millis() - lastWifiAttempt >= app_config::wifiRetryMs) {
      lastWifiAttempt = millis();
      const wl_status_t status = WiFi.status();
      Serial.printf("Red sin acceso: status=%d (%s); reintentando enlace\n",
                    static_cast<int>(status), wifiStatusLabel(status));
      WiFi.reconnect();
      updatePanel(PanelState::Busy, "Reconectando Internet");
    }
  }
  delay(5);
}

#endif  // BOARD_PANEL_4848S040
