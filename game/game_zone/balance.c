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
