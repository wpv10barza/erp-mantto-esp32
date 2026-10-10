#if defined(BOARD_PANEL_4848S040)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
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
constexpr char kFirmwareVersion[] = "2.5.2-white-async";
// OTA manifests use strict numeric semver; the white UI name is still shown.
constexpr char kOtaVersion[] = "2.5.2";

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

enum class NetworkAction : uint8_t { Health, CloudAndSheets, Send3C, OtaCheck, OtaInstall };
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
bool editingFirmwareCommit = false;
String selectedOtaCommit;
// Managed only by the network worker except otaBootConfirmed on the UI task.
String lastOtaMessage = "OTA sin verificar";
bool otaBootConfirmed = false;

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
  drawCentered(editingFirmwareCommit ? "ELEGIR COMMIT OTA" : "EDITAR ORDEN 3C", 11, 2, BLACK);

  // Dedicated shortcut; returns home without posting a maintenance command.
  const auto home = editor_ui::EditorLayout::topRightHome();
  display->fillRoundRect(home.left, home.top,
                         home.width(), home.height(), 8, WHITE);
  display->drawRoundRect(home.left, home.top,
                         home.width(), home.height(), 8, BLACK);
  display->setTextColor(BLACK);
  display->setTextSize(1);
  display->setCursor(home.left + (home.width() - 36) / 2,
                     home.top + (home.height() - 8) / 2);
  display->print("INICIO");
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
    display->setCursor(28, 316);
    display->print("Version: ");
    display->print(kFirmwareVersion);
    display->setCursor(28, 334);
    display->print(panelDetail.startsWith("OTA") || panelDetail.startsWith("FW ")
                       ? panelDetail : String("OTA: HTTPS + SHA-256"));
    display->setCursor(28, 353);
    if (selectedOtaCommit.length()) {
      display->print("Commit seleccionado: ");
      display->print(selectedOtaCommit.substring(0, 12));
    } else {
      display->print("Buscar ultima version o elegir un commit");
    }
    drawButton(22, 377, 140, 31, "BUSCAR OTA", WHITE);
    drawButton(170, 377, 132, 31, "COMMIT", WHITE);
    drawButton(310, 377, 146, 31, "INSTALAR", WHITE);
  } else if (homePanel == HomePanel::Wifi) {
    display->setCursor(28, 316);
    display->print("SSID: ");
    display->print(strlen(app_config::wifiSsid) ? app_config::wifiSsid : "No configurado");
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

void drawPanel() {
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
  if (!commandEditorOpen) drawPanel();
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
         strlen(app_config::databricksClientId) > 0 &&
         strlen(app_config::databricksClientSecret) > 0;
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
  authHttp.setAuthorization(app_config::databricksClientId, app_config::databricksClientSecret);
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
  if (strlen(app_config::apiToken)) {
    http.addHeader("X-3C-Device-Token", app_config::apiToken);
  }
}

int compareSemanticVersion(const String& leftRaw, const String& rightRaw) {
  String left = leftRaw;
  String right = rightRaw;
  left.trim();
  right.trim();
  if (left.startsWith("v") || left.startsWith("V")) left.remove(0, 1);
  if (right.startsWith("v") || right.startsWith("V")) right.remove(0, 1);

  for (int part = 0; part < 3; ++part) {
    const int leftDot = left.indexOf('.');
    const int rightDot = right.indexOf('.');
    String leftPart = leftDot >= 0 ? left.substring(0, leftDot) : left;
    String rightPart = rightDot >= 0 ? right.substring(0, rightDot) : right;
    const int leftDash = leftPart.indexOf('-');
    const int rightDash = rightPart.indexOf('-');
    if (leftDash >= 0) leftPart = leftPart.substring(0, leftDash);
    if (rightDash >= 0) rightPart = rightPart.substring(0, rightDash);
    const long leftValue = leftPart.toInt();
    const long rightValue = rightPart.toInt();
    if (leftValue < rightValue) return -1;
    if (leftValue > rightValue) return 1;
    left = leftDot >= 0 ? left.substring(leftDot + 1) : "";
    right = rightDot >= 0 ? right.substring(rightDot + 1) : "";
  }
  return 0;
}

bool isSha256Hex(const String& value) {
  if (value.length() != 64) return false;
  for (size_t index = 0; index < value.length(); ++index) {
    const char c = value[index];
    if (!isxdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

String sha256Hex(const unsigned char digest[32]) {
  static const char kHex[] = "01234567" "89abcdef";
  String output;
  output.reserve(64);
  for (size_t index = 0; index < 32; ++index) {
    output += kHex[(digest[index] >> 4) & 0x0F];
    output += kHex[digest[index] & 0x0F];
  }
  return output;
}

String absoluteFirmwareUrl(const String& candidate) {
  String url = candidate;
  url.trim();
  // Only allow same-origin Databricks App firmware paths. Prevent header leaks.
  if (!url.startsWith("/api/device/v1/firmware/") ||
      !url.endsWith(".bin") || url.indexOf("..") >= 0 ||
      url.indexOf('?') >= 0 || url.indexOf('#') >= 0) return "";
  return backendBaseUrl() + url;
}

void abortOta(const String& detail) {
  Update.abort();
  lastOtaMessage = detail;
  updatePanel(PanelState::Error, detail, true);
  Serial.printf("[ERROR] OTA %s\n", detail.c_str());
}

// HTTPClient parses both identity and chunked transfer framing here.
// The sink verifies that only the exact manifest size reaches the inactive
// partition. It never stores the whole firmware in ESP32 RAM.
class OtaFlashSink : public Stream {
 public:
  OtaFlashSink(mbedtls_sha256_context& sha, size_t expected)
      : sha_(sha), expected_(expected) {}

  size_t write(uint8_t byte) override { return write(&byte, 1); }
  size_t write(const uint8_t* buffer, size_t length) override {
    if (failed_ || !length) return 0;
    if (received_ > expected_ || length > expected_ - received_) {
      failure_ = "payload_exceeds_manifest";
      failed_ = true;
      return 0;
    }
    const size_t written = Update.write(const_cast<uint8_t*>(buffer), length);
    if (written != length) {
      failure_ = "flash_write_failed";
      failed_ = true;
      return written;
    }
    if (mbedtls_sha256_update_ret(&sha_, buffer, length) != 0) {
      failure_ = "sha256_update_failed";
      failed_ = true;
      return 0;
    }
    received_ += written;
    const size_t milestone = received_ / (128U * 1024U);
    if (milestone > lastMilestone_) {
      lastMilestone_ = milestone;
      Serial.printf("OTA PROGRESS bytes=%u/%u wifi_rssi=%d\n",
                    static_cast<unsigned>(received_),
                    static_cast<unsigned>(expected_),
                    static_cast<int>(WiFi.RSSI()));
    }
    return length;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t received() const { return received_; }
  bool failed() const { return failed_; }
  const char* failure() const { return failure_; }

 private:
  mbedtls_sha256_context& sha_;
  size_t expected_ = 0;
  size_t received_ = 0;
  size_t lastMilestone_ = 0;
  bool failed_ = false;
  const char* failure_ = "none";
};

bool performOtaUpdate(
    const String& version,
    const String& downloadUrl,
    const String& expectedSha256,
    size_t expectedSize) {
  if (!isSha256Hex(expectedSha256)) {
    lastOtaMessage = "OTA: SHA-256 invalido";
    updatePanel(PanelState::Error, lastOtaMessage, true);
    return false;
  }
  if (!ensureDatabricksAccessToken()) {
    lastOtaMessage = "OTA OAuth no disponible";
    updatePanel(PanelState::Error, lastOtaMessage, true);
    return false;
  }

  const String url = absoluteFirmwareUrl(downloadUrl);
  if (!url.length()) {
    lastOtaMessage = "OTA URL invalida";
    updatePanel(PanelState::Error, lastOtaMessage, true);
    return false;
  }

  updatePanel(PanelState::Busy, String("OTA ") + version + " descargando", true);

  HTTPClient http;
  http.setTimeout(app_config::otaHttpTimeoutMs);
  if (!http.begin(url)) {
    lastOtaMessage = "OTA no pudo abrir HTTPS";
    updatePanel(PanelState::Error, lastOtaMessage, true);
    return false;
  }
  http.setReuse(false);
  const char* otaHeaders[] = {"Content-Encoding", "Transfer-Encoding"};
  http.collectHeaders(otaHeaders, 2);
  http.addHeader("Accept-Encoding", "identity");
  addRequestAuth(http);
  const int code = http.GET();
  if (code != 200) {
    lastOtaMessage = String("OTA descarga HTTP ") + code;
    http.end();
    if (code == 401 && databricksAppEndpoint()) clearDatabricksAccessToken();
    updatePanel(PanelState::Error, lastOtaMessage, true);
    return false;
  }

  // The manifest is integrity-verified on the backend, but the ESP32
  // verifies actual received bytes independently before activating flash.
  const int contentLength = http.getSize();
  if (contentLength > 0 && static_cast<size_t>(contentLength) != expectedSize) {
    Serial.printf("[ERROR] OTA content-length=%d expected=%u\n",
                  contentLength, static_cast<unsigned>(expectedSize));
    http.end();
    lastOtaMessage = "OTA longitud HTTP distinta";
    updatePanel(PanelState::Error, lastOtaMessage, true);
    return false;
  }
  const String encoding = http.header("Content-Encoding");
  const String transferEncoding = http.header("Transfer-Encoding");
  if (encoding.length() && !encoding.equalsIgnoreCase("identity")) {
    Serial.printf("[ERROR] OTA unsupported content-encoding=%s\n",
                  encoding.c_str());
    http.end();
    lastOtaMessage = "OTA codificacion HTTP invalida";
    updatePanel(PanelState::Error, lastOtaMessage, true);
    return false;
  }
  Serial.printf("OTA HTTP start version=%s expected=%u content_length=%d transfer=%s free_heap=%u rssi=%d\n",
                version.c_str(), static_cast<unsigned>(expectedSize),
                contentLength, transferEncoding.c_str(),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<int>(WiFi.RSSI()));

  if (!Update.begin(expectedSize, U_FLASH)) {
    http.end();
    lastOtaMessage = "OTA sin particion disponible";
    updatePanel(PanelState::Error, lastOtaMessage, true);
    Serial.printf("[ERROR] Update.begin: %s\n", Update.errorString());
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  if (mbedtls_sha256_starts_ret(&sha, 0) != 0) {
    mbedtls_sha256_free(&sha);
    http.end();
    abortOta("OTA SHA init fallo");
    return false;
  }

  OtaFlashSink sink(sha, expectedSize);
  // HTTPClient handles chunk framing and read timeouts; getStreamPtr()
  // exposes raw transfer bytes and must not be used for chunked bodies.
  const int transferred = http.writeToStream(&sink);
  const size_t total = sink.received();
  const bool flashError = sink.failed();
  const String sinkFailure = sink.failure();
  const int wifiStatus = static_cast<int>(WiFi.status());
  const int wifiRssi = static_cast<int>(WiFi.RSSI());
  http.end();

  if (transferred < 0 || flashError || total != expectedSize ||
      static_cast<size_t>(transferred) != expectedSize) {
    Serial.printf("[ERROR] OTA TRANSFER result=%d received=%u expected=%u http_length=%d sink=%s wifi_status=%d rssi=%d update=%s\n",
                  transferred, static_cast<unsigned>(total),
                  static_cast<unsigned>(expectedSize), contentLength,
                  sinkFailure.c_str(), wifiStatus, wifiRssi,
                  Update.errorString());
    mbedtls_sha256_free(&sha);
    abortOta(flashError ? "OTA fallo escritura/SHA" : "OTA HTTPS incompleto");
    return false;
  }

  unsigned char digest[32] = {};
  const int shaResult = mbedtls_sha256_finish_ret(&sha, digest);
  mbedtls_sha256_free(&sha);
  if (shaResult != 0) {
    abortOta("OTA SHA final fallo");
    return false;
  }

  String actualSha = sha256Hex(digest);
  String expectedSha = expectedSha256;
  expectedSha.toLowerCase();
  if (actualSha != expectedSha) {
    Serial.printf("[ERROR] OTA SHA mismatch expected=%s actual=%s\n",
                  expectedSha.c_str(), actualSha.c_str());
    abortOta("OTA SHA-256 no coincide");
    return false;
  }

  if (!Update.end()) {
    Serial.printf("[ERROR] Update.end: %s\n", Update.errorString());
    abortOta("OTA no pudo finalizar");
    return false;
  }

  lastOtaMessage = String("OTA ") + version + " verificada";
  updatePanel(PanelState::Busy, "OTA OK; reiniciando", true);
  Serial.printf("OTA SUCCESS version=%s bytes=%u sha256=%s\n",
                version.c_str(), static_cast<unsigned>(total), actualSha.c_str());
  delay(1200);
  ESP.restart();
  return true;
}

bool checkForOtaUpdate(bool install, const String& commitRef = String()) {
  if (WiFi.status() != WL_CONNECTED) {
    lastOtaMessage = "OTA sin Internet";
    updatePanel(PanelState::Offline, lastOtaMessage, true);
    return false;
  }
  if (!ensureDatabricksAccessToken()) {
    lastOtaMessage = "OTA OAuth no disponible";
    updatePanel(PanelState::Error, lastOtaMessage, true);
    return false;
  }

  HTTPClient http;
  http.setTimeout(app_config::httpTimeoutMs);
  const String url = endpoint(commitRef.length()
      ? String("/api/device/v1/firmware/commits/") + commitRef
      : String("/api/device/v1/firmware/latest"));
  if (!http.begin(url)) {
    lastOtaMessage = "OTA manifest no disponible";
    updatePanel(PanelState::Error, lastOtaMessage, true);
    return false;
  }
  addRequestAuth(http);
  http.addHeader("X-Firmware-Version", kOtaVersion);
  const int code = http.GET();
  const String body = code > 0 ? http.getString() : http.errorToString(code);
  http.end();

  if (code == 401 && databricksAppEndpoint()) clearDatabricksAccessToken();
  if (code != 200) {
    lastOtaMessage = String("OTA manifest HTTP ") + code;
    updatePanel(PanelState::Error, lastOtaMessage, true);
    Serial.printf("[WARN] OTA manifest HTTP=%d detail=%s\n", code, body.c_str());
    return false;
  }

  if (commitRef.length()) {
    String resolvedSha = jsonStringValue(body, "git_commit_sha");
    resolvedSha.toLowerCase();
    if (resolvedSha.length() != 40 || !resolvedSha.startsWith(commitRef)) {
      lastOtaMessage = "OTA commit no coincide";
      updatePanel(PanelState::Error, lastOtaMessage, true);
      return false;
    }
  }
  const String version = jsonStringValue(body, "version");
  const String sha256 = jsonStringValue(body, "sha256");
  const String path = jsonStringValue(body, "url");
  const size_t size = static_cast<size_t>(jsonUnsignedLongValue(body, "size", 0UL));
  if (!version.length() || !path.length() || !isSha256Hex(sha256) || size == 0 || size > 0x400000UL) {
    lastOtaMessage = "OTA manifest invalido";
    updatePanel(PanelState::Error, lastOtaMessage, true);
    return false;
  }

  if (compareSemanticVersion(version, kOtaVersion) <= 0) {
    lastOtaMessage = String("FW ") + kOtaVersion + " al dia";
    updatePanel(PanelState::Ready, lastOtaMessage, true);
    return false;
  }

  lastOtaMessage = String("OTA disponible ") + version;
  Serial.printf("OTA AVAILABLE current=%s latest=%s size=%u\n",
                kOtaVersion, version.c_str(), static_cast<unsigned>(size));
  // A check is read-only. Flashing requires a second explicit touch.
  if (!install) {
    updatePanel(PanelState::Pending, lastOtaMessage, true);
    return false;
  }
  return performOtaUpdate(version, path, sha256, size);
}


void confirmOtaBootIfHealthy() {
  if (otaBootConfirmed || millis() < app_config::otaBootConfirmDelayMs) return;
  otaBootConfirmed = true;

  const esp_partition_t* running = esp_ota_get_running_partition();
  if (!running) return;
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  const esp_err_t stateResult = esp_ota_get_state_partition(running, &state);
  if (stateResult != ESP_OK || state != ESP_OTA_IMG_PENDING_VERIFY) return;

  if (displayReady) {
    const esp_err_t validResult = esp_ota_mark_app_valid_cancel_rollback();
    Serial.printf("OTA BOOT CONFIRM result=%d\n", static_cast<int>(validResult));
  } else {
    Serial.println("[ERROR] OTA boot self-test failed; rolling back");
    esp_ota_mark_app_invalid_rollback_and_reboot();
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


bool validOtaCommitRef(const String& ref) {
  if (ref.length() < 8 || ref.length() > 40) return false;
  for (size_t i = 0; i < ref.length(); ++i) {
    const char c = ref[i];
    if (!isxdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

bool queueNetworkRequest(NetworkAction action, const String& command = String()) {
  if (!networkQueue) {
    updatePanel(PanelState::Error, "Red no inicializada");
    return false;
  }
  NetworkRequest request{};
  request.action = action;
  if (action == NetworkAction::Send3C) {
    if (!command.length() || command.length() >= sizeof(request.command)) {
      updatePanel(PanelState::Error, "Orden 3C vacia o muy larga");
      return false;
    }
    command.toCharArray(request.command, sizeof(request.command));
  } else if (action == NetworkAction::OtaCheck || action == NetworkAction::OtaInstall) {
    // Never infer a Git commit from GitHub source on the device.
    // An empty commit is an explicit request for latest.
    if (command.length() && !validOtaCommitRef(command)) {
      updatePanel(PanelState::Error, "Commit OTA invalido");
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
              action == NetworkAction::OtaCheck ? "OTA: buscando" :
              action == NetworkAction::OtaInstall ? "OTA: preparando instalacion" :
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
        case NetworkAction::OtaCheck:
          checkForOtaUpdate(false, String(request.command));
          break;
        case NetworkAction::OtaInstall:
          checkForOtaUpdate(true, String(request.command));
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
  if (!strlen(app_config::wifiSsid)) {
    updatePanel(PanelState::Offline, "Configure local_config.h");
    Serial.println("Configure include/local_config.h antes de usar Wi-Fi.");
    return;
  }
  configureWifi();
  Serial.printf("Wi-Fi: iniciando STA, credenciales presentes, status=%d (%s)\n",
    static_cast<int>(WiFi.status()), wifiStatusLabel(WiFi.status()));
  WiFi.begin(app_config::wifiSsid, app_config::wifiPassword);
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
    if (commandEditorOpen) {
      // GT911 touch release confirmed by TapTracker, never on finger-down.
      if (editor_ui::EditorLayout::topRightHomeHit(tap.x, tap.y)) {
        commandEditorOpen = false;
        editingFirmwareCommit = false;
        homePanel = HomePanel::None;
        drawPanel();
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
            if (editingFirmwareCommit) {
              String commitRef(commandBuffer.c_str());
              commitRef.trim();
              commitRef.toLowerCase();
              commandEditorOpen = false;
              editingFirmwareCommit = false;
              homePanel = HomePanel::Firmware;
              if (!validOtaCommitRef(commitRef)) {
                updatePanel(PanelState::Error, "Commit: use 8-40 hex", true);
                return;
              }
              selectedOtaCommit = commitRef;
              drawPanel();
              queueNetworkRequest(NetworkAction::OtaCheck, selectedOtaCommit);
              return;
            }
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
            editingFirmwareCommit = false;
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
      // Firmware button coordinates are inside the expanded details, not
      // in the hidden secondary menu. Explicit user actions are mandatory.
      if (homePanel == HomePanel::Firmware && tap.y >= 377 && tap.y < 408) {
        if (tap.x >= 22 && tap.x < 162) {
          selectedOtaCommit = "";
          queueNetworkRequest(NetworkAction::OtaCheck);
          return;
        }
        if (tap.x >= 170 && tap.x < 302) {
          editingFirmwareCommit = true;
          commandEditorOpen = true;
          keyboardMode = virtual_keyboard::KeyboardMode::Alpha;
          commandBuffer.set(selectedOtaCommit.c_str());
          drawEditor();
          return;
        }
        if (tap.x >= 310 && tap.x < 456) {
          queueNetworkRequest(NetworkAction::OtaInstall, selectedOtaCommit);
          return;
        }
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
          editingFirmwareCommit = false;
          commandEditorOpen = true;
          commandBuffer.set(app_config::commandBuffer.c_str());
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
  Serial.printf("GPIO MAP: BL=%d LCD_CS=%d LCD_CLK=%d LCD_MOSI=%d TOUCH_SDA=%d TOUCH_SCL=%d DE=%d VSYNC=%d HSYNC=%d PCLK=%d\n",
                pins::backlight, pins::lcdCs, pins::lcdClock, pins::lcdMosi,
                pins::touchSda, pins::touchScl, pins::de, pins::vsync, pins::hsync, pins::pclk);
  Wire.begin(pins::touchSda, pins::touchScl, 400000);
  Wire.beginTransmission(kTouchAddress);
  const uint8_t touchProbe = Wire.endTransmission();
  Serial.printf("GT911 I2C probe addr=0x%02X result=%u bus=400kHz\n", kTouchAddress, touchProbe);
  audioReady = initializeAudio();
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
  confirmOtaBootIfHealthy();
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
    if (strlen(app_config::wifiSsid) &&
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
