#include <cassert>
#include <cstddef>

#include "virtual_keyboard.h"

namespace {

using virtual_keyboard::Key;
using virtual_keyboard::KeyKind;
using virtual_keyboard::KeyboardMode;
using virtual_keyboard::KeyRect;

void assertNoOverlaps(KeyboardMode mode) {
  Key keys[50]{};
  const size_t count = virtual_keyboard::buildKeys(mode, keys, 50);

  for (size_t i = 0; i < count; ++i) {
    assert(keys[i].rect.left < keys[i].rect.right);
    assert(keys[i].rect.top < keys[i].rect.bottom);
    assert(keys[i].rect.left >= 0);
    assert(keys[i].rect.top >= 0);
    assert(keys[i].rect.right <= virtual_keyboard::kScreenWidth);
    assert(keys[i].rect.bottom <= virtual_keyboard::kScreenHeight);

    for (size_t j = i + 1; j < count; ++j) {
      assert(!virtual_keyboard::rectanglesOverlap(keys[i].rect, keys[j].rect));
    }
  }
}

void assertHorizontalBoundaryIsExclusive(KeyboardMode mode) {
  Key keys[50]{};
  const size_t count = virtual_keyboard::buildKeys(mode, keys, 50);

  // Consecutive keys in the first populated row have a real gap. The right
  // edge of one key is not part of that key, and the next key starts later.
  const Key& first = keys[0];
  const Key& second = keys[1];
  assert(first.rect.bottom == second.rect.bottom);
  assert(first.rect.top == second.rect.top);
  assert(first.rect.right < second.rect.left);

  const int y = first.rect.top + 1;
  assert(virtual_keyboard::hitTestIndex(mode, first.rect.left, y) == 0);
  assert(virtual_keyboard::hitTestIndex(mode, first.rect.right - 1, y) == 0);
  assert(virtual_keyboard::hitTestIndex(mode, first.rect.right, y) == -1);
  assert(virtual_keyboard::hitTestIndex(mode, second.rect.left - 1, y) == -1);
  assert(virtual_keyboard::hitTestIndex(mode, second.rect.left, y) == 1);

  (void)count;
}

void assertVerticalBoundaryIsExclusive(KeyboardMode mode) {
  Key keys[50]{};
  const size_t count = virtual_keyboard::buildKeys(mode, keys, 50);
  assert(count > 10);

  // Key 0 in row 0 and the corresponding first key in row 1 are separated by
  // the row gap. Neither key owns the gap, so a touch there is ignored.
  const Key& row0 = keys[0];
  const Key& row1 = keys[10];
  assert(row0.rect.left == row1.rect.left);
  assert(row0.rect.bottom < row1.rect.top);

  const int x = row0.rect.left + 1;
  assert(virtual_keyboard::hitTestIndex(mode, x, row0.rect.bottom - 1) == 0);
  assert(virtual_keyboard::hitTestIndex(mode, x, row0.rect.bottom) == -1);
  assert(virtual_keyboard::hitTestIndex(mode, x, row1.rect.top - 1) == -1);
  assert(virtual_keyboard::hitTestIndex(mode, x, row1.rect.top) == 10);
}

void assertOutsideKeyboardIsRejected(KeyboardMode mode) {
  assert(virtual_keyboard::hitTestIndex(mode, -1, -1) == -1);
  assert(virtual_keyboard::hitTestIndex(mode, virtual_keyboard::kScreenWidth, 300) == -1);
  assert(virtual_keyboard::hitTestIndex(mode, 100, virtual_keyboard::kScreenHeight) == -1);
  assert(!virtual_keyboard::hitTest(mode, -1, 300));

  Key matched{};
  assert(!virtual_keyboard::hitTest(mode, -1, 300, &matched));
}

void assertControlKeysHit(KeyboardMode mode) {
  Key keys[50]{};
  const size_t count = virtual_keyboard::buildKeys(mode, keys, 50);
  assert(count >= 33);

  // Search by semantic key kind so the test stays valid if rows are extended.
  bool foundToggle = false;
  bool foundSpace = false;
  bool foundEnter = false;

  for (size_t i = 0; i < count; ++i) {
    const Key& key = keys[i];
    const int x = (static_cast<int>(key.rect.left) + key.rect.right) / 2;
    const int y = (static_cast<int>(key.rect.top) + key.rect.bottom) / 2;
    const int hit = virtual_keyboard::hitTestIndex(mode, x, y);
    assert(hit == static_cast<int>(i));

    if (key.definition.kind == KeyKind::ToggleAlphaNumeric) foundToggle = true;
    if (key.definition.kind == KeyKind::Space) foundSpace = true;
    if (key.definition.kind == KeyKind::Enter) foundEnter = true;
  }

  assert(foundToggle);
  assert(foundSpace);
  assert(foundEnter);
}

}  // namespace

int main() {
  // KeyRect itself is the primitive used by every virtual key.
  constexpr KeyRect rect{10, 20, 30, 40};
  static_assert(rect.contains(10, 20));
  static_assert(rect.contains(29, 39));
  static_assert(!rect.contains(30, 20));
  static_assert(!rect.contains(10, 40));

  assertNoOverlaps(KeyboardMode::Alpha);
  assertNoOverlaps(KeyboardMode::NumericSymbols);

  assertHorizontalBoundaryIsExclusive(KeyboardMode::Alpha);
  assertHorizontalBoundaryIsExclusive(KeyboardMode::NumericSymbols);
  assertVerticalBoundaryIsExclusive(KeyboardMode::Alpha);
  assertVerticalBoundaryIsExclusive(KeyboardMode::NumericSymbols);

  assertOutsideKeyboardIsRejected(KeyboardMode::Alpha);
  assertOutsideKeyboardIsRejected(KeyboardMode::NumericSymbols);

  assertControlKeysHit(KeyboardMode::Alpha);
  assertControlKeysHit(KeyboardMode::NumericSymbols);

  return 0;
}
