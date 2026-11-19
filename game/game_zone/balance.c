#include "balance.h"

static float Clamp(float value, float min, float max) {
  return value < min ? min : value > max ? max : value;
}

float BalanceDifficultyProgress(int difficulty) {
  return (Clamp(difficulty, 1, 10) - 1) / 9;
}

float BalanceBulletHellSpeed(int difficulty) {
  return 145 + 135 * BalanceDifficultyProgress(difficulty);
}

float BalanceBulletHellSpread(int difficulty) {
  return 75 - 45 * BalanceDifficultyProgress(difficulty);
}

float BalanceBulletHellSpawnInterval(int difficulty) {
  return 0.95f - 0.3f * BalanceDifficultyProgress(difficulty);
}

float BalanceRingGapDegrees(int difficulty) {
  return 90 - 45 * BalanceDifficultyProgress(difficulty);
}

float BalanceRingSpeed(int difficulty) {
  return 35 + 45 * BalanceDifficultyProgress(difficulty);
}

float BalanceRingSpacing(int difficulty) {
  return 60 + 4 * Clamp(difficulty, 1, 10);
}

int BalanceRingCountMinimum(int difficulty) {
  difficulty = (int)Clamp(difficulty, 1, 10);
  return difficulty <= 3 ? 6 : 7 + (difficulty - 4) / 3;
}

int BalanceTowerAttemptEtrisReward(float elapsedSeconds, int difficulty) {
  if (elapsedSeconds <= 0)
    return 0;
  int reward =
      (int)(elapsedSeconds * (0.1f + 0.04f * Clamp(difficulty, 1, 10)) + 0.5f);
  return reward > 0 ? reward : 1;
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
  return baseSpeed * (0.85f + floorScale * 0.35f + difficultyScale * 0.15f);
}

float BalanceRandomSpeedMax(int floor, int difficulty, float baseSpeed) {
  float floorScale = (float)Clamp(floor, 1, 50) / 50.0f;
  float difficultyScale = BalanceDifficultyProgress(difficulty);
  return baseSpeed * (1.05f + floorScale * 0.55f + difficultyScale * 0.25f);
}

float BalanceRingGapRandomMin(int floor, int difficulty) {
  float floorScale = (float)Clamp(floor, 1, 50) / 50.0f;
  return 28.0f - 14.0f * floorScale - 8.0f * BalanceDifficultyProgress(difficulty);
}

float BalanceRingGapRandomMax(int floor, int difficulty) {
  float floorScale = (float)Clamp(floor, 1, 50) / 50.0f;
  return 55.0f - 18.0f * floorScale - 10.0f * BalanceDifficultyProgress(difficulty);
}

float BalanceMirrorSpeed(int difficulty) {
  return 100 + 155 * BalanceDifficultyProgress(difficulty);
}

float BalanceMirrorSpread(int difficulty) {
  return 75 - 40 * BalanceDifficultyProgress(difficulty);
}

float BalanceMirrorSpawnInterval(int difficulty) {
  return 0.85f - 0.3f * BalanceDifficultyProgress(difficulty);
}

float BalanceSealWarning(int difficulty, float bonus) {
  return Clamp(1.15f - 0.04f * (Clamp(difficulty, 1, 10) - 1) + bonus, 0.8f,
               1.32f);
}

float BalanceSealSpawnInterval(int difficulty, int sealed) {
  return Clamp(1.35f - 0.045f * (Clamp(difficulty, 1, 10) - 1) -
                   0.06f * sealed,
               0.9f, 1.3f);
}
