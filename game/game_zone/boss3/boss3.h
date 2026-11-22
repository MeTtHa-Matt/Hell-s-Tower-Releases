#ifndef BOSS3_H
#define BOSS3_H

#include "raylib.h"

typedef enum {
  BOSS3_OFF = 0,
  BOSS3_PLAYING,
  BOSS3_WON,
  BOSS3_LOST
} Boss3Status;

/* Agares, Demon de la Vengeance :
 * le boss ne perd de la vie que si le joueur esquive (ESPACE) AU CONTACT
 * de ses attaques (riposte). */
void Boss3Start(int floorNumber, int difficulty);
void Boss3Update(Rectangle area);
Boss3Status Boss3State(void);
int Boss3DamageCount(void);
void Boss3Unload(void);

#endif
