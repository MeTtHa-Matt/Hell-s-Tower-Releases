#ifndef LEVELS_H
#define LEVELS_H

void LevelsLoad(int level, int xp);
int LevelsLevel(void);
int LevelsXP(void);
int LevelsXPNeeded(void);
void LevelsAddXP(int amount);
void LevelsShowReward(int oldLevel, int oldXP, int gainedXP);
void LevelsDrawNotification(void);

#endif
