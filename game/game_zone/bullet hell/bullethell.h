#ifndef BULLETHELL_H
#define BULLETHELL_H

#include "raylib.h"

typedef enum { HELL_OFF, HELL_PLAYING, HELL_WON, HELL_LOST } HellState;

void BulletHellStart(int difficulty); /* difficulté de 1 à 10 */
void BulletHellUpdate(Rectangle zone); /* logique + affichage, chaque frame */
HellState BulletHellState(void);
int BulletHellDamageCount(void);
void BulletHellUnload(void);

#endif
