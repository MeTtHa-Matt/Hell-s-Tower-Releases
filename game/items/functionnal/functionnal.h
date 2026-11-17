#ifndef FUNCTIONNAL_H
#define FUNCTIONNAL_H

#include <stdbool.h>

typedef struct {
  int difficulty, extraLives;
  float duration, playerSpeed, hitbox, bulletSpeed, bulletSize;
  float bulletSpawn, nearbyBulletSpeed, invincibility, regeneration;
  float ringSpeed, ringGap, mirrorRightSpeed;
  float sealWarning, sealCharge, sealSpawn;
  bool shield, clearBullets, removeRings, ringDash;
  bool mirrorInvincible, mirrorRepel;
} FunctionalSettings;

void FunctionalReset(void);
void FunctionalAddEffect(int effect);
const FunctionalSettings *FunctionalGet(void);
int FunctionalDifficulty(int difficulty);
bool FunctionalBlockDamage(void);

#endif
