#ifndef MIRROR_H
#define MIRROR_H

#include "raylib.h"

typedef enum { MIRROR_OFF, MIRROR_PLAYING, MIRROR_WON, MIRROR_LOST } MirrorStatus;

/* Lance une partie. difficulty : 1 (facile) à 10 (très dur). */
void MirrorStart(int difficulty);

/* À appeler chaque frame avec la zone de jeu (callback de la zone). */
void MirrorUpdate(Rectangle zone);

/* MIRROR_OFF une fois la partie terminée et validée avec ENTREE. */
MirrorStatus MirrorState(void);

/* Nombre de dégâts subis depuis le début de la partie. */
int MirrorDamageCount(void);

/* Libère les textures. */
void MirrorUnload(void);

#endif
