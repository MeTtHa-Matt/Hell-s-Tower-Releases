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

static bool CombatSettingsNeutral(void) {
  const FunctionalSettings *settings = FunctionalGet();
  return settings->difficulty == 0 && settings->extraLives == 0 &&
         fabsf(settings->duration) < 0.0001f &&
         fabsf(settings->playerSpeed - 1) < 0.0001f &&
         fabsf(settings->hitbox - 1) < 0.0001f &&
         fabsf(settings->bulletSpeed - 1) < 0.0001f &&
         fabsf(settings->bulletSize - 1) < 0.0001f &&
         fabsf(settings->bulletSpawn - 1) < 0.0001f &&
         fabsf(settings->nearbyBulletSpeed - 1) < 0.0001f &&
         fabsf(settings->spikesBallSpeed - 1) < 0.0001f &&
         fabsf(settings->ringSpeed - 1) < 0.0001f &&
         fabsf(settings->ringGap - 1) < 0.0001f &&
         fabsf(settings->mirrorRightSpeed - 1) < 0.0001f &&
         fabsf(settings->sealWarning) < 0.0001f &&
         fabsf(settings->sealCharge - 1) < 0.0001f &&
         fabsf(settings->sealSpawn - 1) < 0.0001f &&
         fabsf(settings->sealChargeDecay - 1) < 0.0001f &&
         fabsf(settings->sealDashCooldown - 1) < 0.0001f &&
         fabsf(settings->invincibility) < 0.0001f &&
         fabsf(settings->regeneration) < 0.0001f &&
         settings->boss4ShardHeal == 0 && !settings->shield &&
         !settings->clearBullets && !settings->removeRings &&
         !settings->ringDash && !settings->mirrorInvincible &&
         !settings->mirrorRepel && !settings->sealWard &&
         !settings->bulletHellHighAid && !settings->ringHighAid &&
         !settings->mirrorHighAid && !settings->sealHighAid &&
         !settings->lastStand;
}

int main(void) {
  const int normal[DROP_RARITY_COUNT] = {7000, 2200, 650, 120, 20, 10};
  const int lucky[DROP_RARITY_COUNT] = {6200, 2200, 1300, 240, 40, 20};
  REQUIRE(TestDropRates(false, normal) == 0);
  REQUIRE(TestDropRates(true, lucky) == 0);
  REQUIRE(ItemsRarityForRoll(-1, false) == DROP_COMMON);
  REQUIRE(ItemsRarityForRoll(10000, false) == DROP_UNIQUE);
  REQUIRE(BalancePickMinigame(0, -1) == 0);
  REQUIRE(BalancePickMinigame(4, -1) == 4);
  for (int previous = 0; previous < 5; previous++) {
    bool seen[5] = {false};
    for (uint32_t roll = 0; roll < 4; roll++) {
      int selected = BalancePickMinigame(roll, previous);
      REQUIRE(selected >= 0 && selected < 5 && selected != previous);
      seen[selected] = true;
    }
    for (int selected = 0; selected < 5; selected++)
      if (selected != previous)
        REQUIRE(seen[selected]);
  }

  for (int difficulty = 1; difficulty <= 10; difficulty++) {
    float progress = BalanceDifficultyProgress(difficulty);
    REQUIRE(progress >= 0 && progress <= 1);
    REQUIRE(BalanceSealWarning(difficulty, 0) >= 0.35f);
    REQUIRE(BalanceSealWarning(difficulty, 0) <= 1.15f);
    REQUIRE(BalanceSealWarning(difficulty, 0.12f) >= 0.35f);
    REQUIRE(BalanceSealWarning(difficulty, 0.12f) <= 1.32f);
    REQUIRE(BalanceSealSpawnInterval(difficulty, 4) >= 0.28f);
    REQUIRE(BalanceSealSpawnInterval(difficulty, 4) <= 1.1f);
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
  REQUIRE(BalanceBulletHellSpeed(1) > 145);
  REQUIRE(BalanceBulletHellSpeed(10) > 400);
  REQUIRE(BalanceBulletHellSpread(1) < 75);
  REQUIRE(BalanceBulletHellSpawnInterval(1) < 0.95f);
  REQUIRE(BalanceBulletHellSpawnInterval(10) < 0.45f);
  REQUIRE(BalanceRingCountMinimum(10) >= 12);
  REQUIRE(BalanceBossIntensity(10, 10) > 1.6f);
  REQUIRE(BalanceBossIntensity(50, 10) > BalanceBossIntensity(10, 10));

  REQUIRE(BalanceTowerAttemptEtrisReward(0, 10) == 0);
  REQUIRE(BalanceTowerAttemptEtrisReward(1, 1) == 1);
  REQUIRE(BalanceTowerAttemptEtrisReward(30, 1) == 4);
  REQUIRE(BalanceTowerAttemptEtrisReward(30, 10) == 15);
  REQUIRE(BalanceTowerAttemptEtrisReward(50, 10) == 25);
  REQUIRE(BalanceTowerAttemptEtrisReward(30, 10) >
          BalanceTowerAttemptEtrisReward(30, 1));
  REQUIRE(BalanceTowerAttemptEtrisReward(50, 10) >
          BalanceTowerAttemptEtrisReward(30, 10));
  REQUIRE(BalanceMiniGameModifierCount(3, 0, 0) == 0);
  REQUIRE(BalanceMiniGameModifierCount(4, 14, 99) == 1);
  REQUIRE(BalanceMiniGameModifierCount(4, 15, 99) == 0);
  REQUIRE(BalanceMiniGameModifierCount(8, 74, 99) == 1);
  REQUIRE(BalanceMiniGameModifierCount(8, 75, 99) == 0);
  REQUIRE(BalanceMiniGameModifierCount(9, 99, 99) == 2);
  REQUIRE(BalanceMiniGameModifierCount(10, 99, 34) == 3);
  REQUIRE(BalanceMiniGameModifierCount(10, 99, 35) == 2);
  REQUIRE(BalanceMiniGameDuration(50, 10, 30) >
          BalanceMiniGameDuration(1, 1, 30));
  REQUIRE(BalanceMiniGameDurationMax(50, 10, 30) >
          BalanceMiniGameDurationMin(50, 10, 30));
  REQUIRE(BalanceRandomSpeedMax(50, 10, 145) >
          BalanceRandomSpeedMin(50, 10, 145));
  REQUIRE(BalanceRingGapRandomMax(50, 10) <
          BalanceRingGapRandomMax(1, 1));

  FunctionalReset();
  FunctionalApplyCharacter(2);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 1.15f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyCharacter(3);
  REQUIRE(fabsf(FunctionalGet()->etrisMultiplier - 1.1f) < 0.0001f);
  REQUIRE(FunctionalGet()->extraLives == 0 && !FunctionalGet()->lastStand);
  FunctionalReset();
  FunctionalApplyCharacter(4);
  REQUIRE(FunctionalGet()->bonusLevelItems == 1);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 1.0f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->duration) < 0.0001f);
  FunctionalReset();
  FunctionalApplyCharacter(5);
  REQUIRE(FunctionalGet()->extraLives == 0);
  REQUIRE(fabsf(FunctionalGet()->etrisMultiplier - 1.15f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyCharacterTitle(2);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 1.1f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyCharacterTitle(3);
  REQUIRE(fabsf(FunctionalGet()->etrisMultiplier - 1.1f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyCharacterTitle(4);
  REQUIRE(FunctionalGet()->bonusLevelItems == 1);
  FunctionalReset();
  FunctionalApplyCharacterTitle(5);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 1.15f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealWarning) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->invincibility) < 0.0001f);
  FunctionalReset();
  FunctionalApplyPlayerTitle(3);
  REQUIRE(FunctionalGet()->bonusLevelItems == 1);
  FunctionalReset();
  FunctionalApplyPlayerTitle(4);
  REQUIRE(fabsf(FunctionalGet()->etrisMultiplier - 1.1f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyPlayerTitle(0);
  FunctionalApplyPlayerTitle(1);
  REQUIRE(fabsf(FunctionalGet()->etrisMultiplier - 1.25f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 1.2f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyPlayerTitle(2);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 1.1f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealWarning) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->duration) < 0.0001f);
  FunctionalReset();
  FunctionalApplyPlayerTitle(0);
  FunctionalApplyPlayerTitle(1);
  FunctionalAddEffect(64);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 2.4f) < 0.0001f);
  REQUIRE(FunctionalGet()->bonusLevelItems == 1);
  REQUIRE(FunctionalGet()->extraLives == 1 && FunctionalGet()->shield);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.8f) < 0.0001f);
  REQUIRE(FunctionalGet()->difficulty == -1);
  REQUIRE(FunctionalDifficulty(10) == 9);
  FunctionalReset();
  FunctionalApplyPlayerTitle(5);
  REQUIRE(fabsf(FunctionalGet()->etrisMultiplier - 1.15f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyPlayerTitle(6);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 1.1f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyPlayerTitle(7);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 1.05f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyPlayerTitle(8);
  REQUIRE(FunctionalGet()->bonusLevelItems == 1);
  FunctionalReset();
  FunctionalApplyPlayerTitle(9);
  REQUIRE(fabsf(FunctionalGet()->etrisMultiplier - 1.1f) < 0.0001f);
  for (int character = 0; character < 6; character++) {
    FunctionalReset();
    FunctionalApplyCharacter(character);
    FunctionalApplyCharacterTitle(character);
    REQUIRE(CombatSettingsNeutral());
  }
  for (int title = 0; title < 13; title++) {
    FunctionalReset();
    FunctionalApplyPlayerTitle(title);
    REQUIRE(CombatSettingsNeutral());
  }
  FunctionalReset();
  FunctionalApplyPlayerTitle(10);
  REQUIRE(fabsf(FunctionalGet()->etrisMultiplier - 1.2f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyPlayerTitle(11);
  REQUIRE(fabsf(FunctionalGet()->experienceMultiplier - 1.15f) < 0.0001f);
  FunctionalReset();
  FunctionalApplyPlayerTitle(12);
  REQUIRE(FunctionalGet()->bonusLevelItems == 1);
  FunctionalReset();
  FunctionalAddEffect(69);
  REQUIRE(fabsf(FunctionalGet()->spikesBallSpeed - 0.85f) < 0.0001f);
  FunctionalAddEffect(70);
  REQUIRE(fabsf(FunctionalGet()->invincibility - 0.75f) < 0.0001f);
  FunctionalReset();
  FunctionalAddEffect(71);
  REQUIRE(FunctionalGet()->extraLives == 3);
  REQUIRE(FunctionalGet()->shield && FunctionalGet()->lastStand);
  REQUIRE(fabsf(FunctionalGet()->spikesBallSpeed - 0.5f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->bulletSize - 0.7f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->invincibility - 2.0f) < 0.0001f);
  FunctionalReset();
  FunctionalAddEffect(72);
  REQUIRE(FunctionalGet()->extraLives == 1 && FunctionalGet()->shield);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.75f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealCharge - 1.3f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealDashCooldown - 0.6f) < 0.0001f);
  FunctionalReset();
  FunctionalAddEffect(73);
  REQUIRE(FunctionalGet()->extraLives == 1);
  REQUIRE(fabsf(FunctionalGet()->playerSpeed - 1.12f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.9f) < 0.0001f);
  FunctionalReset();
  FunctionalAddEffect(74);
  REQUIRE(FunctionalGet()->extraLives == 2 && FunctionalGet()->shield);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.7f) < 0.0001f);
  REQUIRE(FunctionalGet()->boss4ShardHeal == 1);
  REQUIRE(FunctionalGet()->difficulty == -2);
  REQUIRE(FunctionalDifficulty(9) == 7);
  FunctionalReset();
  FunctionalAddEffect(75);
  REQUIRE(FunctionalGet()->extraLives == 2 && FunctionalGet()->shield);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.75f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->invincibility - 1.5f) < 0.0001f);
  FunctionalReset();
  FunctionalAddEffect(76);
  REQUIRE(FunctionalGet()->extraLives == 2 && FunctionalGet()->shield);
  REQUIRE(fabsf(FunctionalGet()->playerSpeed - 1.15f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.75f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->invincibility - 1.0f) < 0.0001f);
  FunctionalReset();
  FunctionalAddEffect(80);
  REQUIRE(FunctionalGet()->extraLives == 2 && FunctionalGet()->shield);
  REQUIRE(FunctionalGet()->sealWard);
  REQUIRE(fabsf(FunctionalGet()->sealWarning - 0.6f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealSpawn - 1.3f) < 0.0001f);
  REQUIRE(FunctionalGet()->difficulty == -2);
  FunctionalReset();
  FunctionalAddEffect(81);
  REQUIRE(FunctionalGet()->extraLives == 2 && FunctionalGet()->shield);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.7f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->invincibility - 1.5f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealWarning - 0.8f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealSpawn - 1.3f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealDashCooldown - 0.8f) < 0.0001f);
  REQUIRE(FunctionalGet()->sealWard && FunctionalGet()->difficulty == -2);
  FunctionalReset();
  FunctionalAddEffect(82);
  REQUIRE(FunctionalGet()->extraLives == 2 && FunctionalGet()->shield);
  REQUIRE(FunctionalGet()->lastStand && FunctionalGet()->sealWard);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.65f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->invincibility - 1.25f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealDashCooldown - 0.7f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealSpawn - 1.2f) < 0.0001f);
  REQUIRE(FunctionalGet()->difficulty == -2);
  FunctionalReset();
  FunctionalAddEffect(83);
  REQUIRE(FunctionalGet()->extraLives == 2 && FunctionalGet()->shield);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.7f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->invincibility - 1.5f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealWarning - 0.8f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealSpawn - 1.3f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealDashCooldown - 0.8f) < 0.0001f);
  REQUIRE(FunctionalGet()->sealWard && FunctionalGet()->difficulty == -2);
  FunctionalReset();
  FunctionalAddEffect(84);
  REQUIRE(FunctionalGet()->extraLives == 2 && FunctionalGet()->shield);
  REQUIRE(FunctionalGet()->lastStand && FunctionalGet()->sealWard);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.65f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->invincibility - 1.25f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealDashCooldown - 0.7f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->sealSpawn - 1.2f) < 0.0001f);
  REQUIRE(FunctionalGet()->difficulty == -2);
  FunctionalReset();
  FunctionalAddEffect(77);
  REQUIRE(FunctionalGet()->extraLives == 2 && FunctionalGet()->shield);
  REQUIRE(FunctionalGet()->lastStand);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.65f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->invincibility - 1.5f) < 0.0001f);
  FunctionalReset();
  FunctionalAddEffect(78);
  REQUIRE(FunctionalGet()->extraLives == 2 && FunctionalGet()->shield);
  REQUIRE(FunctionalGet()->lastStand);
  REQUIRE(fabsf(FunctionalGet()->playerSpeed - 1.1f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.8f) < 0.0001f);
  FunctionalReset();
  FunctionalAddEffect(79);
  REQUIRE(FunctionalGet()->extraLives == 1 && FunctionalGet()->shield);
  REQUIRE(fabsf(FunctionalGet()->playerSpeed - 1.15f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->hitbox - 0.75f) < 0.0001f);
  REQUIRE(fabsf(FunctionalGet()->invincibility - 1.0f) < 0.0001f);
  FunctionalReset();
  FunctionalAddEffect(68);
  REQUIRE(FunctionalGet()->difficulty == -2);
  REQUIRE(FunctionalGet()->sealWard);
  FunctionalReset();
  FunctionalAddEffect(72);
  REQUIRE(FunctionalGet()->difficulty == -2);
  REQUIRE(FunctionalGet()->shield);
  return 0;
}
