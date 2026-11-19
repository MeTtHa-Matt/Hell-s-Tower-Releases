#ifndef GAME_GAME_ZONE_BALANCE_H
#define GAME_GAME_ZONE_BALANCE_H

float BalanceDifficultyProgress(int difficulty);
float BalanceBulletHellSpeed(int difficulty);
float BalanceBulletHellSpread(int difficulty);
float BalanceBulletHellSpawnInterval(int difficulty);
float BalanceRingGapDegrees(int difficulty);
float BalanceRingSpeed(int difficulty);
float BalanceRingSpacing(int difficulty);
int BalanceRingCountMinimum(int difficulty);
int BalanceTowerAttemptEtrisReward(float elapsedSeconds, int difficulty);
float BalanceMirrorSpeed(int difficulty);
float BalanceMirrorSpread(int difficulty);
float BalanceMirrorSpawnInterval(int difficulty);
float BalanceSealWarning(int difficulty, float bonus);
float BalanceSealSpawnInterval(int difficulty, int sealed);

#endif
