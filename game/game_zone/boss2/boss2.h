#ifndef BOSS2_H
#define BOSS2_H

#include "raylib.h"

typedef enum {
  BOSS2_OFF = 0,
  BOSS2_PLAYING,
  BOSS2_WON,
  BOSS2_LOST
} Boss2Status;

/* Selena, Deesse Dechue :
 * a intervalles reguliers, la lune inonde toute l'arene de lumiere mortelle.
 * Seuls les miroirs lunaires protegent : s'y tenir au moment de l'eclat
 * renvoie la lumiere sur la deesse et la blesse. */
void Boss2Start(int floorNumber, int difficulty);
void Boss2Update(Rectangle area);
Boss2Status Boss2State(void);
int Boss2DamageCount(void);
void Boss2Unload(void);

#endif
