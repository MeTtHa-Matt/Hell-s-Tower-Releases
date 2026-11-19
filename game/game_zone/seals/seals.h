#ifndef SEALS_H
#define SEALS_H

#include "raylib.h"

typedef enum {
  SEALS_OFF,
  SEALS_PLAYING,
  SEALS_WON,
  SEALS_LOST
} SealsStatus;

void SealsStart(int floor, int difficulty);
void SealsUpdate(Rectangle zone);
SealsStatus SealsState(void);
int SealsDamageCount(void);
void SealsUnload(void);

#endif
