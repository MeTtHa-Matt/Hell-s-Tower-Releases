#ifndef FINALBOSS_H
#define FINALBOSS_H

#include "raylib.h"

typedef enum {
  FINALBOSS_OFF,
  FINALBOSS_PLAYING,
  FINALBOSS_WON,
  FINALBOSS_LOST
} FinalBossStatus;

void FinalBossStart(int floorNumber, int difficulty);
void FinalBossUpdate(Rectangle area);
FinalBossStatus FinalBossState(void);
int FinalBossDamageCount(void);
void FinalBossUnload(void);

#endif
