from pathlib import Path

PRIMARY = Path('platformio/src/panel_4848s040/main.cpp').read_text(encoding='utf-8')
MIRROR = Path('src/main.cpp').read_text(encoding='utf-8')
KEYBOARD = Path('include/virtual_keyboard.h').read_text(encoding='utf-8')
assert PRIMARY == MIRROR, 'Firmware mirror drift'
for token in ('WSL DISPONIBLE', 'PROBAR WSL', 'ENVIAR 3C', 'EDITAR ORDEN 3C', 'CANCELAR', '3c-backend.local', 'Preferences backendPrefs', 'kBackendMdnsService[] = \"3c\"'):
    assert token in PRIMARY, f'Recovered UI/mDNS contract missing: {token}'
for token in ('123', 'ABC', 'NumericSymbols'):
    assert token in KEYBOARD, f'Keyboard contract missing: {token}'
assert 'PROBAR BACKEND' not in PRIMARY
assert 'BACKEND DISPONIBLE' not in PRIMARY
print('Recovered Sep-19 UI + production mDNS contract: PASS')
