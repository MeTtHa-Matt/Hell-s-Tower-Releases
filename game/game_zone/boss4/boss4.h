#ifndef BOSS4_H
#define BOSS4_H

#include "raylib.h"

typedef enum {
  BOSS4_OFF,
  BOSS4_PLAYING,
  BOSS4_WON,
  BOSS4_LOST
} Boss4Status;

void Boss4Start(int floorNumber, int difficulty);
void Boss4Update(Rectangle area);
Boss4Status Boss4State(void);
int Boss4DamageCount(void);
void Boss4Unload(void);

#endif
