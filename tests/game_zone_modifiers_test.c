#include "../game/game_zone/modifiers.h"
#include <stdio.h>

#define REQUIRE(condition)                                                     \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "Échec ligne %d: %s\n", __LINE__, #condition);           \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  bool selected[MINIGAME_MODIFIER_OPTIONS] = {false};
  const char *names[MINIGAME_MODIFIER_OPTIONS] = {"A", "B", "C", "D"};

  GameZoneClearActiveModifiers();
  GameZoneDiscardPendingModifiers();
  GameZoneRestoreModifiers(0x15u);
  REQUIRE(GameZonePickModifiers(10, MINIGAME_MODIFIER_OPTIONS, selected,
                                names) == 2);
  REQUIRE(selected[0] && !selected[1] && selected[2] && !selected[3]);
  REQUIRE(GameZoneActiveModifierMask() == 0x5u);
  REQUIRE(GameZoneActiveModifiersInitialized());

  GameZoneClearActiveModifiers();
  REQUIRE(GameZoneActiveModifierMask() == 0);
  REQUIRE(!GameZoneActiveModifiersInitialized());
  return 0;
}
