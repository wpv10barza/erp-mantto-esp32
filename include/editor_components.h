#pragma once

#include <array>
#include <cstdint>

namespace editor_ui {

struct Rect {
  int16_t left;
  int16_t top;
  int16_t right;
  int16_t bottom;

  constexpr bool contains(int x, int y) const {
    return x >= left && x < right && y >= top && y < bottom;
  }
  constexpr int width() const { return right - left; }
  constexpr int height() const { return bottom - top; }
};

enum class ToolbarAction : uint8_t {
  None,
  Home,
  MoveLeft,
  MoveRight,
  DeleteForward,
  Clear,
};

struct ToolbarButton {
  ToolbarAction action;
  const char* label;
  Rect rect;
};

class EditorLayout {
 public:
  static constexpr int kScreenWidth = 480;
  static constexpr int kScreenHeight = 480;
  static constexpr Rect title() { return {0, 0, 480, 42}; }
  // Visible touch target in the top-right of the editor title bar.
  // Keep it separate from the lower toolbar and keyboard hitboxes.
  static constexpr Rect topRightHome() { return {378, 4, 470, 39}; }
  static constexpr bool topRightHomeHit(int x, int y) {
    const Rect button = topRightHome();
    constexpr int inset = 3;
    return x >= button.left + inset && x < button.right - inset &&
           y >= button.top + inset && y < button.bottom - inset;
  }
  static constexpr Rect textField() { return {12, 48, 468, 146}; }
  static constexpr Rect toolbar() { return {12, 164, 468, 212}; }

  static constexpr bool inTextField(int x, int y) {
    return textField().contains(x, y);
  }
};

class ToolbarComponent {
 public:
  static constexpr int kGap = 6;
  static constexpr int kButtonWidth = 86;
  static constexpr int kButtonHeight = 42;
  static constexpr int kHitInset = 3;
  static constexpr int kLeft = 13;
  static constexpr int kTop = 167;

  static constexpr std::array<ToolbarButton, 5> buttons() {
    return {{
      {ToolbarAction::Home, "INICIO", {kLeft, kTop, kLeft + kButtonWidth, kTop + kButtonHeight}},
      {ToolbarAction::MoveLeft, "<", {kLeft + 1 * (kButtonWidth + kGap), kTop,
                                     kLeft + 1 * (kButtonWidth + kGap) + kButtonWidth, kTop + kButtonHeight}},
      {ToolbarAction::MoveRight, ">", {kLeft + 2 * (kButtonWidth + kGap), kTop,
                                      kLeft + 2 * (kButtonWidth + kGap) + kButtonWidth, kTop + kButtonHeight}},
      {ToolbarAction::DeleteForward, "DEL", {kLeft + 3 * (kButtonWidth + kGap), kTop,
                                            kLeft + 3 * (kButtonWidth + kGap) + kButtonWidth, kTop + kButtonHeight}},
      {ToolbarAction::Clear, "LIMPIAR", {kLeft + 4 * (kButtonWidth + kGap), kTop,
                                        kLeft + 4 * (kButtonWidth + kGap) + kButtonWidth, kTop + kButtonHeight}},
    }};
  }

  static inline ToolbarAction hitTest(int x, int y) {
    const auto list = buttons();
    for (const auto& button : list) {
      const Rect active{
          static_cast<int16_t>(button.rect.left + kHitInset),
          static_cast<int16_t>(button.rect.top + kHitInset),
          static_cast<int16_t>(button.rect.right - kHitInset),
          static_cast<int16_t>(button.rect.bottom - kHitInset)};
      if (active.contains(x, y)) return button.action;
    }
    return ToolbarAction::None;
  }
};

}  // namespace editor_ui
