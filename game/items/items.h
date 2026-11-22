#ifndef ITEMS_H
#define ITEMS_H

#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

#define QUEST_ITEM_COUNT 6

typedef enum {
  ITEM_SORT_RARITY,
  ITEM_SORT_NAME,
  ITEM_SORT_EFFECT_COUNT,
  ITEM_SORT_COUNT
} ItemSortOrder;

void ItemsReset(void);
void ItemsUnload(void);
void ItemsLoad(const char *data);
/* Returns a heap-allocated save string; the caller must free it. */
char *ItemsSave(void);
void ItemsGrantForLevelUp(void);
void ItemsGrantQuestLetter(void);
void ItemsGrantBossSeal(int floor);
void ItemsMigrateQuestRewards(int nextFloor);
bool ItemsMelinaLetterRead(void);
bool ItemsLetterReaderActive(void);
void ItemsCloseLetterReader(void);
void ItemsPrepareForFloor(int floor, const char *game, bool bossFloor);
void ItemsApplyForGame(const char *game, bool bossFloor);
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
int ItemsHighestOccupiedSlot(void);
bool ItemsSortStoredItems(ItemSortOrder order);
const char *ItemsEffectText(int effect);
const char *ItemsEffectRarity(int effect);
const char *ItemsEffectGame(int effect);
bool ItemsEffectCanBeForged(int effect);
int ItemsRarityRank(const char *rarity);
Color ItemsRarityColor(const char *rarity);
const char *ItemsItemName(int slot);
const char *ItemsItemRarestEffectRarity(int slot);
int ItemsRewardSlot(void);
void ItemsDrawPreview(int slot, Rectangle box);
void ItemsDrawKnownPreview(const char *name, Rectangle box);
void ItemsDrawTooltip(int slot, Vector2 position);
bool ItemsForgeCanComplete(const char *name, const int *effects,
                           int effectCount, const int *sacrifices,
                           int sacrificeCount);
int ItemsForgeComplete(const char *name, const int *effects, int effectCount,
                       const int *sacrifices, int sacrificeCount);
void ItemsCloseContextMenu(void);
void ItemsDrawInventory(Rectangle equipped[5], Rectangle stored[25],
                        Rectangle quest[QUEST_ITEM_COUNT], bool showQuestItems,
                        Rectangle contextBox, bool acceptInput);
void ItemsDrawReward(void);
void ItemsClearRewards(void);

#endif
