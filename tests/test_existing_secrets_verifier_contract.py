from pathlib import Path
src = (Path(__file__).resolve().parents[1] / 'scripts' / 'Verificar-Secretos-Alumno-3C.ps1').read_text(encoding='utf-8')
for expected in (
    'alumno-3c',
    'polovaleriow@gmail.com',
    'list-scopes',
    'list-secrets',
    'get-secret',
    'current-user',
    'esp32_api_token',
    'reviewer_token',
    'gemini_api_key',
    'notepad.txt',
    'RegistrarGemini',
    'put-secret',
    'base64Local -cne',
):
    assert expected in src, f'missing safety contract: {expected}'
assert 'create-scope' not in src
assert 'delete-secret' not in src
assert 'delete-scope' not in src
assert 'New-RandomHexToken' not in src
assert 'Write-Host $remoto' not in src
assert 'Write-Host $base64Local' not in src
assert 'put-secret $ScopeNuevo reviewer_token' not in src
assert 'put-secret $ScopeNuevo esp32_api_token' not in src
print('Student secret verification contract: PASS')
