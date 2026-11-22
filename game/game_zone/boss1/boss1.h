#ifndef BOSS1_H
#define BOSS1_H

#include "raylib.h"

typedef enum {
  BOSS1_OFF = 0,
  BOSS1_PLAYING,
  BOSS1_WON,
  BOSS1_LOST
} Boss1Status;

/* Behemot, Demon de l'Arrogance :
 * il charge aveuglement sur le joueur. Il faut se placer derriere un pilier
 * pour qu'il s'y fracasse : l'orgueilleux est humilie et perd de la vie. */
void Boss1Start(int floorNumber, int difficulty);
void Boss1Update(Rectangle area);
Boss1Status Boss1State(void);
int Boss1DamageCount(void);
void Boss1Unload(void);

#endif
