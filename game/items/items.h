#ifndef ITEMS_H
#define ITEMS_H

#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

void ItemsReset(void);
void ItemsUnload(void);
void ItemsLoad(const char *data);
void ItemsSave(char *data, size_t size);
void ItemsGrantForLevelUp(void);
void ItemsConsumeForGame(const char *game);
void ItemsDrawInventory(Rectangle equipped[5], Rectangle stored[25]);
void ItemsDrawReward(void);

#endif
