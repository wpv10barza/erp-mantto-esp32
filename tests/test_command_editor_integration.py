from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
EDITOR = (ROOT / "include" / "editor_components.h").read_text(encoding="utf-8")
KEYBOARD = (ROOT / "include" / "virtual_keyboard.h").read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing editor integration contract: {label}: {needle}")


for needle, label in [
    ('"INICIO"', "return-to-main button"),
    ('ToolbarAction::MoveLeft', "left cursor action"),
    ('ToolbarAction::MoveRight', "right cursor action"),
    ('ToolbarAction::DeleteForward', "forward delete action"),
    ('ToolbarAction::Clear', "clear action"),
    ('class ToolbarComponent', "isolated toolbar component"),
    ('class EditorLayout', "isolated editor layout"),
]:
    require(EDITOR, needle, label)

for needle, label in [
    ('class KeyboardLayout', "isolated keyboard layout"),
    ('constexpr int kKeyboardX = (kScreenWidth - kKeyboardWidth) / 2;', "horizontal centering"),
    ('keyboardY(KeyboardMode mode)', "mode-specific vertical centering"),
    ('KeyKind::Backspace', "backspace"),
    ('KeyKind::Enter', "enter"),
    ('KeyKind::Space', "space"),
    ('KeyKind::ToggleAlphaNumeric', "ABC/123 mode"),
]:
    require(KEYBOARD, needle, label)

for needle, label in [
    ('drawEditorFrame()', "frame renderer"),
    ('drawEditorTextField()', "text-field renderer"),
    ('drawEditorToolbar()', "toolbar renderer"),
    ('drawEditorKeyboard()', "keyboard renderer"),
    ('commandEditorOpen = false;', "editor exit"),
    ('homePanel = HomePanel::None;', "return to home menu"),
    ('commandBuffer.moveLeft()', "cursor left"),
    ('commandBuffer.moveRight()', "cursor right"),
    ('commandBuffer.deleteForward()', "delete forward"),
    ('commandBuffer.clear()', "clear"),
    ('queueNetworkRequest(NetworkAction::Send3C, app_config::commandBuffer)', "send edited command"),
]:
    require(MAIN, needle, label)

editor_touch = MAIN[MAIN.index("    if (commandEditorOpen) {", MAIN.index("void handleTouch()")):]
char_case = editor_touch[editor_touch.index("case KeyKind::Character:"):editor_touch.index("case KeyKind::Backspace:")]
require(char_case, "drawEditorTextField();", "partial text redraw")
if "drawEditor();" in char_case:
    raise AssertionError("character input must not redraw the complete editor")

print("Editor component integration: PASS")
print("- INICIO returns to the final main menu")
print("- keyboard geometry is centered independently")
print("- left/right/delete/clear controls are isolated from the keyboard")
print("- normal typing redraws only the text field")
print("- original 3C send + human-review flow remains connected")
