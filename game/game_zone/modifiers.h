#ifndef GAME_ZONE_MODIFIERS_H
#define GAME_ZONE_MODIFIERS_H

#include "raylib.h"
#include <stdbool.h>

#define MINIGAME_MODIFIER_OPTIONS 4

int GameZonePickModifiers(int difficulty, int optionCount, bool *selected,
                          const char *const *names);
void GameZoneClearActiveModifiers(void);
void GameZoneDiscardPendingModifiers(void);
bool GameZoneActiveModifiersInitialized(void);
unsigned int GameZoneActiveModifierMask(void);
void GameZoneRestoreModifiers(unsigned int mask);
void GameZoneDrawActiveModifiers(Rectangle area, int yOffset);

#endif
