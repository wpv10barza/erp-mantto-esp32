#pragma once
#include <cstdint>
namespace databricks_activation {
enum class State { Idle, Starting, Ready, AuthError, Rejected, Unknown };
inline const char* label(State s) {
  switch (s) {
    case State::Idle: return "Sin solicitar";
    case State::Starting: return "Iniciando; espere";
    case State::Ready: return "Backend verificado";
    case State::AuthError: return "Revise OAuth/permisos";
    case State::Rejected: return "Inicio rechazado";
    default: return "Sin confirmar; revise workspace";
  }
}
// Worker-owned state machine. POST occurs only on explicit begin, never in tick.
class Activation {
 public:
  State state = State::Idle;
  int http = 0;
  template<class Transport> void begin(Transport& t, uint32_t now) {
    if (state == State::Starting) return;
    http = 0;
    if (!t.authenticate()) { state = State::AuthError; return; }
    http = t.start();
    if (http == 401 || http == 403) state = State::AuthError;
    else if (http >= 200 && http < 300) {
      state = State::Starting; started = checked = now;
    } else if (http <= 0 || http >= 500) state = State::Unknown;
    else state = State::Rejected;
  }
  template<class Transport> void tick(Transport& t, uint32_t now) {
    if (state != State::Starting) return;
    if (uint32_t(now - started) >= 180000U) { state = State::Unknown; return; }
    if (uint32_t(now - checked) < 5000U) return;
    checked = now;
    if (t.healthy()) state = State::Ready;
  }
 private:
  uint32_t started = 0, checked = 0;
};
} // namespace databricks_activation
