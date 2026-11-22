#ifndef GAME_GAME_ZONE_BALANCE_H
#define GAME_GAME_ZONE_BALANCE_H

#include <stdint.h>

int BalancePickMinigame(uint32_t randomValue, int previous);
float BalanceDifficultyProgress(int difficulty);
float BalanceBossIntensity(int floor, int difficulty);
float BalanceBulletHellSpeed(int difficulty);
float BalanceBulletHellSpread(int difficulty);
float BalanceBulletHellSpawnInterval(int difficulty);
float BalanceRingGapDegrees(int difficulty);
float BalanceRingSpeed(int difficulty);
float BalanceRingSpacing(int difficulty);
int BalanceRingCountMinimum(int difficulty);
int BalanceTowerAttemptEtrisReward(float elapsedSeconds, int difficulty);
/* Rolls are in [0, 99] so modifier-count thresholds can be tested exactly. */
int BalanceMiniGameModifierCount(int difficulty, int chanceRoll,
                                 int extraRoll);
float BalanceMiniGameDuration(int floor, int difficulty, float baseDuration);
float BalanceMiniGameDurationMin(int floor, int difficulty,
                                 float baseDuration);
float BalanceMiniGameDurationMax(int floor, int difficulty,
                                 float baseDuration);
float BalanceRandomSpeedMin(int floor, int difficulty, float baseSpeed);
float BalanceRandomSpeedMax(int floor, int difficulty, float baseSpeed);
float BalanceRingGapRandomMin(int floor, int difficulty);
float BalanceRingGapRandomMax(int floor, int difficulty);
float BalanceMirrorSpeed(int difficulty);
float BalanceMirrorSpread(int difficulty);
float BalanceMirrorSpawnInterval(int difficulty);
float BalanceSealWarning(int difficulty, float bonus);
float BalanceSealSpawnInterval(int difficulty, int sealed);

#endif
