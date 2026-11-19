#ifndef GAME_ITEMS_DROP_RATES_H
#define GAME_ITEMS_DROP_RATES_H

#include <stdbool.h>

typedef enum {
  DROP_COMMON,
  DROP_RARE,
  DROP_EPIC,
  DROP_LEGENDARY,
  DROP_CURSED,
  DROP_UNIQUE,
  DROP_RARITY_COUNT
} DropRarity;

int ItemsRarityForRoll(int roll, bool lucky);

#endif
