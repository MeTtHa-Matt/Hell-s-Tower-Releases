#include "drop_rates.h"

int ItemsRarityForRoll(int roll, bool lucky) {
  static const int normal[DROP_RARITY_COUNT] = {7000, 2200, 650, 120, 20, 10};
  static const int luckyRates[DROP_RARITY_COUNT] = {6200, 2200, 1300, 240, 40, 20};
  const int *rates = lucky ? luckyRates : normal;
  if (roll < 0)
    roll = 0;
  if (roll > 9999)
    roll = 9999;
  for (int i = 0; i < DROP_RARITY_COUNT; i++) {
    if (roll < rates[i])
      return i;
    roll -= rates[i];
  }
  return DROP_COMMON;
}
