from pathlib import Path
P=Path('platformio/src/panel_4848s040/main.cpp').read_text()
M=Path('src/main.cpp').read_text()
K=Path('include/virtual_keyboard.h').read_text()
assert P == M
assert all(x in P for x in ('WSL DISPONIBLE','PROBAR WSL','ENVIAR 3C','EDITAR ORDEN 3C','CANCELAR','3c-backend.local'))
assert all(x in K for x in ('123','ABC','NumericSymbols'))
assert 'PROBAR BACKEND' not in P and 'BACKEND DISPONIBLE' not in P
print('Recovered UI + mDNS contract: PASS')
