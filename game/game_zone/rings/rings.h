#ifndef RINGS_H
#define RINGS_H

#include "raylib.h"

typedef enum { RINGS_OFF, RINGS_PLAYING, RINGS_WON, RINGS_LOST } RingsStatus;

/* Lance une partie. difficulty : 1 (facile) à 10 (très dur). */
void RingsStart(int floor, int difficulty);

/* À appeler chaque frame avec la zone de jeu (callback de la zone). */
void RingsUpdate(Rectangle zone);

/* RINGS_OFF une fois la partie terminée et validée avec ENTREE. */
RingsStatus RingsState(void);

/* Nombre de dégâts subis depuis le début de la partie. */
int RingsDamageCount(void);

/* Libère la texture. */
void RingsUnload(void);

#endif
