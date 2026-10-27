"""v2.7.1: keyboard ENTER persists editor, command and status stream."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cpp = (root / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
mirror = (root / "src/main.cpp").read_text(encoding="utf-8")
layout = (root / "include/editor_components.h").read_text(encoding="utf-8")
assert cpp == mirror, "source mirrors must stay identical"
assert 'kFirmwareVersion[] = "2.7.5"' in cpp
assert "static constexpr Rect textField() { return {12, 48, 468, 146}; }" in layout
assert "static constexpr Rect statusDisplay() { return {12, 146, 468, 164}; }" in layout
assert "static constexpr Rect toolbar() { return {12, 164, 468, 212}; }" in layout
assert "constexpr int kKeyboardAreaTop = 224" in (
    root / "include/virtual_keyboard.h"
).read_text(encoding="utf-8")

ui = cpp[cpp.index("void handleTouch()"):cpp.index("}  // namespace", cpp.index("void handleTouch()"))]
main_editor = ui[ui.index("    if (commandEditorOpen) {"):]
on_enter = main_editor[main_editor.index("          case KeyKind::Enter:"):
                       main_editor.index("          case KeyKind::ToggleAlphaNumeric:")]
assert "queueNetworkRequest(NetworkAction::Send3C" in on_enter
assert "setEditorFlowStatus(PanelState::Busy," in on_enter
assert "commandEditorOpen = false" not in on_enter
assert "drawPanel();" not in on_enter
assert "commandBuffer.clear()" not in on_enter
assert "drawEditor();" not in on_enter
assert "if (editorFlowPending) break;" in on_enter

for guard in (
    "KeyKind::Character:", "KeyKind::Backspace:", "KeyKind::Space:",
    "ToolbarAction::MoveLeft:", "ToolbarAction::MoveRight:",
    "ToolbarAction::DeleteForward:", "ToolbarAction::Clear:",
):
    chunk = main_editor[main_editor.index(guard):][:180]
    assert "if (editorFlowPending) break;" in chunk, guard

assert "CommandFlowNotification" in cpp
assert "commandFlowQueue = xQueueCreate(1, sizeof(CommandFlowNotification))" in cpp
assert "xQueueOverwrite(commandFlowQueue, &notice)" in cpp
assert "xQueueReceive(commandFlowQueue, &flow, 0)" in cpp
assert "drawStatusDisplay(" in cpp
assert 'display->print(message);' in cpp
assert "if (changed && commandEditorOpen" in cpp
assert "void processNetworkUiUpdates()" in cpp
assert "drawEditorTextField();" in main_editor  # local keyboard redraw only
for expected in (
    '"Flujo activo / Procesando..."',
    '"Aprobado / Applied"',
    '"Desaprobado / Rejected"',
    '"Aprobado / Sin aplicar"',
):
    assert expected in cpp, expected
assert 'emitCommandFlow(PanelState::Applied, "Aprobado / Applied", true)' in cpp
assert 'emitCommandFlow(PanelState::Rejected, "Desaprobado / Rejected", true)' in cpp
assert "status == \"approved\"" in cpp
assert "if (editorFlowPending || otaCommitEditorOpen || historyScreenOpen ||" in cpp
assert "lastCommandId = \"\";" in cpp
assert '"pipeline' not in cpp.lower()

render = cpp[cpp.index("void drawStatusDisplay("):cpp.index("void drawEditorToolbar()")]
assert "fillScreen(" not in render
assert "drawEditorKeyboard(" not in render
assert "drawEditorTextField(" not in render
assert "display->fillRect(area.left, area.top, area.width(), area.height(), WHITE)" in render
print("v2.7.1 editor flow persistence, geometry and state display: PASS")
