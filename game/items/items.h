#ifndef ITEMS_H
#define ITEMS_H

#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

#define QUEST_ITEM_COUNT 6

void ItemsReset(void);
void ItemsUnload(void);
void ItemsLoad(const char *data);
void ItemsSave(char *data, size_t size);
void ItemsGrantForLevelUp(void);
void ItemsGrantQuestLetter(void);
void ItemsGrantBossSeal(int floor);
void ItemsMigrateQuestRewards(int nextFloor);
bool ItemsMelinaLetterRead(void);
bool ItemsLetterReaderActive(void);
void ItemsCloseLetterReader(void);
void ItemsApplyForGame(const char *game);
void ItemsDestroy(int slot);
void ItemsCloseContextMenu(void);
void ItemsDrawInventory(Rectangle equipped[5], Rectangle stored[25],
                        Rectangle quest[QUEST_ITEM_COUNT], bool showQuestItems);
void ItemsDrawReward(void);

#endif
