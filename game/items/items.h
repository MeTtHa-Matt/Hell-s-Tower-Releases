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
void ItemsConsumeEquippedUses(void);
void ItemsDestroy(int slot);
int ItemsSellValue(int slot);
int ItemsSell(int slot);
bool ItemsCanStore(int count);
bool ItemsGrantShopPack(int count, int slots[5]);
int ItemsKnownItemCount(void);
const char *ItemsKnownItem(int index);
int ItemsKnownEffectCount(void);
int ItemsKnownEffect(int index);
const char *ItemsEffectText(int effect);
const char *ItemsEffectRarity(int effect);
const char *ItemsEffectGame(int effect);
const char *ItemsItemName(int slot);
int ItemsRewardSlot(void);
void ItemsDrawPreview(int slot, Rectangle box);
void ItemsDrawKnownPreview(const char *name, Rectangle box);
void ItemsDrawTooltip(int slot, Vector2 position);
bool ItemsForgeCanComplete(const char *name, const int *effects,
                           int effectCount, const int *sacrifices,
                           int sacrificeCount);
bool ItemsForgeComplete(const char *name, const int *effects, int effectCount,
                        const int *sacrifices, int sacrificeCount);
void ItemsCloseContextMenu(void);
void ItemsDrawInventory(Rectangle equipped[5], Rectangle stored[25],
                        Rectangle quest[QUEST_ITEM_COUNT], bool showQuestItems,
                        Rectangle contextBox);
void ItemsDrawReward(void);
void ItemsClearRewards(void);

#endif
