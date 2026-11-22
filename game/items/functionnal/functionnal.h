#ifndef FUNCTIONNAL_H
#define FUNCTIONNAL_H

#include <stdbool.h>

typedef struct {
  int difficulty, extraLives;
  float duration, playerSpeed, hitbox, bulletSpeed, bulletSize;
  float bulletSpawn, nearbyBulletSpeed, invincibility, regeneration;
  float spikesBallSpeed;
  float ringSpeed, ringGap, mirrorRightSpeed;
  float sealWarning, sealCharge, sealSpawn, sealChargeDecay, sealDashCooldown;
  int boss4ShardHeal;
  float experienceMultiplier, etrisMultiplier, clearBulletsAt;
  float ringDashCooldown, mirrorInvincibleDuration, mirrorRepelRange;
  int bonusLevelItems, ringSkipInterval;
  bool shield, clearBullets, removeRings, ringDash;
  bool mirrorInvincible, mirrorRepel, sealWard;
  bool bulletHellHighAid, ringHighAid, mirrorHighAid, sealHighAid;
  bool lastStand;
} FunctionalSettings;

void FunctionalReset(void);
void FunctionalAddEffect(int effect);
const FunctionalSettings *FunctionalGet(void);
int FunctionalDifficulty(int difficulty);
bool FunctionalBlockDamage(void);
bool FunctionalBlockDamageAtLives(int lives);
void FunctionalApplyCharacter(int character);
void FunctionalApplyCharacterTitle(int character);
void FunctionalApplyPlayerTitle(int title);

#endif
