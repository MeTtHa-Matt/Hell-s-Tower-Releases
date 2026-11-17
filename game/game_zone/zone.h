#ifndef GAME_ZONE_H
#define GAME_ZONE_H

#include "raylib.h"

typedef void (*MiniGameCallback)(Rectangle zone);
typedef int (*MiniGameDamageCallback)(void);

void DrawGameZone(MiniGameCallback minigame,
                  MiniGameDamageCallback damageCount,
                  const char *monsterImagePath);
void UnloadGameZone(void);

#endif
