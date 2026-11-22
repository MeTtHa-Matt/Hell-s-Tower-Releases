#include "balance.h"

static float Clamp(float value, float min, float max) {
  return value < min ? min : value > max ? max : value;
}

int BalancePickMinigame(uint32_t randomValue, int previous) {
  if (previous < 0 || previous >= 5)
    return (int)(randomValue % 5);
  int choice = (int)(randomValue % 4);
  return choice >= previous ? choice + 1 : choice;
}

float BalanceDifficultyProgress(int difficulty) {
  return (Clamp(difficulty, 1, 10) - 1) / 9;
}

float BalanceBossIntensity(int floor, int difficulty) {
  int tier = (int)Clamp((float)(floor / 10), 1, 5) - 1;
  float tierProgress = tier / 4.0f;
  return 1.0f + BalanceDifficultyProgress(difficulty) *
                    (0.65f + 0.4f * tierProgress);
}

float BalanceBulletHellSpeed(int difficulty) {
  return 175 + 230 * BalanceDifficultyProgress(difficulty);
}

float BalanceBulletHellSpread(int difficulty) {
  return 65 - 48 * BalanceDifficultyProgress(difficulty);
}

float BalanceBulletHellSpawnInterval(int difficulty) {
  return 0.78f - 0.38f * BalanceDifficultyProgress(difficulty);
}

float BalanceRingGapDegrees(int difficulty) {
  return 78 - 52 * BalanceDifficultyProgress(difficulty);
}

float BalanceRingSpeed(int difficulty) {
  return 50 + 70 * BalanceDifficultyProgress(difficulty);
}

float BalanceRingSpacing(int difficulty) {
  return 70 + 5 * Clamp(difficulty, 1, 10);
}

int BalanceRingCountMinimum(int difficulty) {
  difficulty = (int)Clamp(difficulty, 1, 10);
  return difficulty <= 3 ? 8 : 9 + (difficulty - 4) / 2;
}

int BalanceTowerAttemptEtrisReward(float elapsedSeconds, int difficulty) {
  if (elapsedSeconds <= 0)
    return 0;
  int reward =
      (int)(elapsedSeconds * (0.1f + 0.04f * Clamp(difficulty, 1, 10)) + 0.5f);
  return reward > 0 ? reward : 1;
}

int BalanceMiniGameModifierCount(int difficulty, int chanceRoll,
                                 int extraRoll) {
  difficulty = (int)Clamp(difficulty, 1, 10);
  if (difficulty < 4)
    return 0;
  if (difficulty < 9)
    return chanceRoll < (difficulty - 3) * 15 ? 1 : 0;
  if (difficulty == 9)
    return 2;
  return 2 + (extraRoll < 35);
}

float BalanceMiniGameDuration(int floor, int difficulty, float baseDuration) {
  floor = (int)Clamp(floor, 1, 50);
  return baseDuration + floor * 0.35f +
         Clamp(difficulty, 1, 10) * 0.55f;
}

float BalanceMiniGameDurationMin(int floor, int difficulty,
                                 float baseDuration) {
  return BalanceMiniGameDuration(floor, difficulty, baseDuration) * 0.82f;
}

float BalanceMiniGameDurationMax(int floor, int difficulty,
                                 float baseDuration) {
  return BalanceMiniGameDuration(floor, difficulty, baseDuration) * 1.18f;
}

float BalanceRandomSpeedMin(int floor, int difficulty, float baseSpeed) {
  float floorScale = (float)Clamp(floor, 1, 50) / 50.0f;
  float difficultyScale = BalanceDifficultyProgress(difficulty);
  float bossScale = floor >= 10 && floor % 10 == 0
                        ? 1.0f + 0.15f * (floor / 10 - 1) / 4.0f *
                                      difficultyScale
                        : 1.0f;
  return baseSpeed *
         (1.0f + floorScale * 0.3f + difficultyScale * 0.15f) * bossScale;
}

float BalanceRandomSpeedMax(int floor, int difficulty, float baseSpeed) {
  float floorScale = (float)Clamp(floor, 1, 50) / 50.0f;
  float difficultyScale = BalanceDifficultyProgress(difficulty);
  float bossScale = floor >= 10 && floor % 10 == 0
                        ? 1.0f + 0.15f * (floor / 10 - 1) / 4.0f *
                                      difficultyScale
                        : 1.0f;
  return baseSpeed *
         (1.25f + floorScale * 0.55f + difficultyScale * 0.3f) * bossScale;
}

float BalanceRingGapRandomMin(int floor, int difficulty) {
  float floorScale = (float)Clamp(floor, 1, 50) / 50.0f;
  return Clamp(22.0f - 14.0f * floorScale -
                   8.0f * BalanceDifficultyProgress(difficulty),
               4.0f, 22.0f);
}

float BalanceRingGapRandomMax(int floor, int difficulty) {
  float floorScale = (float)Clamp(floor, 1, 50) / 50.0f;
  return Clamp(52.0f - 20.0f * floorScale -
                   12.0f * BalanceDifficultyProgress(difficulty),
               20.0f, 52.0f);
}

float BalanceMirrorSpeed(int difficulty) {
  return 120 + 190 * BalanceDifficultyProgress(difficulty);
}

float BalanceMirrorSpread(int difficulty) {
  return 70 - 45 * BalanceDifficultyProgress(difficulty);
}

float BalanceMirrorSpawnInterval(int difficulty) {
  return 0.75f - 0.4f * BalanceDifficultyProgress(difficulty);
}

float BalanceSealWarning(int difficulty, float bonus) {
  return Clamp(1.0f - 0.065f * (Clamp(difficulty, 1, 10) - 1) + bonus, 0.35f,
               1.32f);
}

float BalanceSealSpawnInterval(int difficulty, int sealed) {
  return Clamp(1.1f - 0.07f * (Clamp(difficulty, 1, 10) - 1) -
                   0.09f * sealed,
               0.28f, 1.1f);
}
