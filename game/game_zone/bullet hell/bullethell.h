#ifndef BULLETHELL_H
#define BULLETHELL_H

#include "raylib.h"

typedef enum { HELL_OFF, HELL_PLAYING, HELL_WON, HELL_LOST } HellState;

void BulletHellStart(int floor, int difficulty);
void BulletHellUpdate(Rectangle zone); /* logique + affichage, chaque frame */
HellState BulletHellState(void);
int BulletHellDamageCount(void);
void BulletHellUnload(void);

#endif
