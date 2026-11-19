#include "functionnal.h"

static FunctionalSettings settings;
static bool shieldReady;

void FunctionalReset(void) {
  settings = (FunctionalSettings){
      .playerSpeed = 1, .hitbox = 1, .bulletSpeed = 1,
      .bulletSize = 1, .bulletSpawn = 1, .nearbyBulletSpeed = 1,
      .ringSpeed = 1, .ringGap = 1, .mirrorRightSpeed = 1,
      .sealCharge = 1, .sealSpawn = 1, .sealChargeDecay = 1,
      .sealDashCooldown = 1};
  shieldReady = false;
}

void FunctionalAddEffect(int effect) {
  switch (effect) {
  case 0: settings.extraLives++; break;
  case 1: settings.duration -= 3; break;
  case 2: settings.duration -= 5; break;
  case 3: settings.playerSpeed *= 1.15f; break;
  case 4: settings.playerSpeed *= 1.30f; break;
  case 5: settings.hitbox *= 0.80f; break;
  case 6: settings.shield = shieldReady = true; break;
  case 7: settings.difficulty--; break;
  case 8: settings.difficulty -= 2; break;
  case 9: settings.bulletSpeed *= 0.85f; break;
  case 10: settings.invincibility += 1; break;
  case 11: settings.regeneration = 15; break;
  case 12: settings.bulletSize *= 0.80f; break;
  case 13: settings.bulletSpawn *= 1.25f; break;
  case 14: settings.clearBullets = true; break;
  case 15: settings.nearbyBulletSpeed *= 0.75f; break;
  case 16: settings.ringSpeed *= 0.85f; break;
  case 17: settings.ringSpeed *= 0.70f; break;
  case 18: settings.ringGap *= 1.25f; break;
  case 19: settings.removeRings = true; break;
  case 20: settings.ringDash = true; break;
  case 21: settings.hitbox *= 0.75f; break;
  case 22: settings.mirrorInvincible = true; break;
  case 23: settings.mirrorRightSpeed *= 0.80f; break;
  case 24: settings.mirrorRepel = true; break;
  case 25: settings.playerSpeed *= 1.25f; break;
  case 26: settings.duration += 3; break;
  case 27: settings.difficulty++; break;
  case 28: settings.hitbox *= 1.15f; break;
  case 29: settings.extraLives--; break;
  case 30: settings.sealWarning += 0.12f; break;
  case 31: settings.sealCharge *= 1.15f; break;
  case 32: settings.sealSpawn *= 1.15f; break;
  case 33: settings.sealChargeDecay *= 0.6f; break;
  case 34: settings.sealDashCooldown *= 0.8f; break;
  case 35: settings.sealWard = true; break;
  case 36: settings.bulletHellHighAid = true; break;
  case 37: settings.ringHighAid = true; break;
  case 38: settings.mirrorHighAid = true; break;
  case 39: settings.sealHighAid = true; break;
  }
}

const FunctionalSettings *FunctionalGet(void) {
  return &settings;
}

int FunctionalDifficulty(int difficulty) {
  difficulty += settings.difficulty;
  if (difficulty < 1)
    return 1;
  if (difficulty > 10)
    return 10;
  return difficulty;
}

bool FunctionalBlockDamage(void) {
  if (!shieldReady)
    return false;
  shieldReady = false;
  settings.shield = false;
  return true;
}
