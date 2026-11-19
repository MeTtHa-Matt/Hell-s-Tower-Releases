#include "../game/game_zone/balance.h"
#include "../game/items/drop_rates.h"
#include "../game/items/functionnal/functionnal.h"
#include <math.h>
#include <stdio.h>

#define REQUIRE(condition)                                                     \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "Échec ligne %d: %s\n", __LINE__, #condition);           \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int TestDropRates(bool lucky, const int expected[DROP_RARITY_COUNT]) {
  int actual[DROP_RARITY_COUNT] = {0};
  for (int roll = 0; roll < 10000; roll++)
    actual[ItemsRarityForRoll(roll, lucky)]++;
  for (int i = 0; i < DROP_RARITY_COUNT; i++)
    if (actual[i] != expected[i])
      return 1;
  return 0;
}

int main(void) {
  const int normal[DROP_RARITY_COUNT] = {7000, 2200, 650, 120, 20, 10};
  const int lucky[DROP_RARITY_COUNT] = {6200, 2200, 1300, 240, 40, 20};
  REQUIRE(TestDropRates(false, normal) == 0);
  REQUIRE(TestDropRates(true, lucky) == 0);
  REQUIRE(ItemsRarityForRoll(-1, false) == DROP_COMMON);
  REQUIRE(ItemsRarityForRoll(10000, false) == DROP_UNIQUE);

  for (int difficulty = 1; difficulty <= 10; difficulty++) {
    float progress = BalanceDifficultyProgress(difficulty);
    REQUIRE(progress >= 0 && progress <= 1);
    REQUIRE(BalanceSealWarning(difficulty, 0) >= 0.8f);
    REQUIRE(BalanceSealWarning(difficulty, 0) <= 1.15f);
    REQUIRE(BalanceSealWarning(difficulty, 0.12f) >= 0.8f);
    REQUIRE(BalanceSealWarning(difficulty, 0.12f) <= 1.32f);
    REQUIRE(BalanceSealSpawnInterval(difficulty, 4) >= 0.9f);
    REQUIRE(BalanceSealSpawnInterval(difficulty, 4) <= 1.3f);
    if (difficulty == 10)
      continue;
    REQUIRE(BalanceDifficultyProgress(difficulty + 1) > progress);
    REQUIRE(BalanceBulletHellSpeed(difficulty + 1) >
            BalanceBulletHellSpeed(difficulty));
    REQUIRE(BalanceBulletHellSpread(difficulty + 1) <
            BalanceBulletHellSpread(difficulty));
    REQUIRE(BalanceBulletHellSpawnInterval(difficulty + 1) <
            BalanceBulletHellSpawnInterval(difficulty));
    REQUIRE(BalanceRingGapDegrees(difficulty + 1) <
            BalanceRingGapDegrees(difficulty));
    REQUIRE(BalanceRingSpeed(difficulty + 1) >
            BalanceRingSpeed(difficulty));
    REQUIRE(BalanceRingSpacing(difficulty + 1) >
            BalanceRingSpacing(difficulty));
    REQUIRE(BalanceRingCountMinimum(difficulty + 1) >=
            BalanceRingCountMinimum(difficulty));
    REQUIRE(BalanceMirrorSpeed(difficulty + 1) >
            BalanceMirrorSpeed(difficulty));
    REQUIRE(BalanceMirrorSpread(difficulty + 1) <
            BalanceMirrorSpread(difficulty));
    REQUIRE(BalanceMirrorSpawnInterval(difficulty + 1) <
            BalanceMirrorSpawnInterval(difficulty));
    REQUIRE(BalanceSealWarning(difficulty + 1, 0) <
            BalanceSealWarning(difficulty, 0));
    REQUIRE(BalanceSealSpawnInterval(difficulty + 1, 4) <=
            BalanceSealSpawnInterval(difficulty, 4));
  }

  REQUIRE(BalanceTowerAttemptEtrisReward(0, 10) == 0);
  REQUIRE(BalanceTowerAttemptEtrisReward(1, 1) == 1);
  REQUIRE(BalanceTowerAttemptEtrisReward(30, 1) == 4);
  REQUIRE(BalanceTowerAttemptEtrisReward(30, 10) == 15);
  REQUIRE(BalanceTowerAttemptEtrisReward(50, 10) == 25);
  REQUIRE(BalanceTowerAttemptEtrisReward(30, 10) >
          BalanceTowerAttemptEtrisReward(30, 1));
  REQUIRE(BalanceTowerAttemptEtrisReward(50, 10) >
          BalanceTowerAttemptEtrisReward(30, 10));

  FunctionalReset();
  FunctionalApplyCharacter(3);
  REQUIRE(FunctionalGet()->lastStand);
  REQUIRE(FunctionalBlockDamageAtLives(1));
  REQUIRE(!FunctionalBlockDamageAtLives(1));
  FunctionalReset();
  FunctionalApplyPlayerTitle(0);
  FunctionalApplyPlayerTitle(1);
  REQUIRE(fabsf(FunctionalGet()->etrisMultiplier - 1.25f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 1.2f) < 0.0001f);
  FunctionalAddEffect(64);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 2.4f) < 0.0001f);
  REQUIRE(FunctionalGet()->bonusLevelItems == 1);
  return 0;
}
