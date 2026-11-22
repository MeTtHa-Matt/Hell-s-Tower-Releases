#include "functionnal.h"

static FunctionalSettings settings;
static bool shieldReady, lastStandReady;

void FunctionalReset(void) {
  settings = (FunctionalSettings){
      .playerSpeed = 1, .hitbox = 1, .bulletSpeed = 1,
      .bulletSize = 1, .bulletSpawn = 1, .nearbyBulletSpeed = 1,
      .spikesBallSpeed = 1,
      .ringSpeed = 1, .ringGap = 1, .mirrorRightSpeed = 1,
      .sealCharge = 1, .sealSpawn = 1, .sealChargeDecay = 1,
      .sealDashCooldown = 1, .experienceMultiplier = 1,
      .etrisMultiplier = 1, .clearBulletsAt = 15,
      .ringDashCooldown = 2, .mirrorInvincibleDuration = 5,
      .mirrorRepelRange = 100, .ringSkipInterval = 4};
  shieldReady = lastStandReady = false;
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
  case 40: settings.extraLives++; break;
  case 41: settings.playerSpeed *= 1.12f; break;
  case 42: settings.hitbox *= 0.90f; break;
  case 43: settings.invincibility += 0.5f; break;
  case 44: settings.duration += 2; break;
  case 45: settings.regeneration = 20; break;
  case 46:
    settings.clearBullets = true;
    settings.clearBulletsAt = 20;
    break;
  case 47: settings.bulletSpawn *= 1.15f; break;
  case 48: settings.nearbyBulletSpeed *= 0.85f; break;
  case 49: settings.bulletSize *= 0.90f; break;
  case 50: settings.ringGap *= 1.12f; break;
  case 51: settings.ringSpeed *= 0.88f; break;
  case 52:
    settings.ringDash = true;
    settings.ringDashCooldown *= 0.80f;
    break;
  case 53:
    settings.removeRings = true;
    settings.ringSkipInterval = 3;
    break;
  case 54: settings.mirrorRightSpeed *= 0.88f; break;
  case 55: settings.mirrorInvincibleDuration += 3; break;
  case 56:
    settings.mirrorRepel = true;
    settings.mirrorRepelRange *= 1.4f;
    break;
  case 57: settings.sealWarning += 0.18f; break;
  case 58: settings.sealCharge *= 1.2f; break;
  case 59: settings.sealSpawn *= 1.12f; break;
  case 60: settings.sealDashCooldown *= 0.8f; break;
  case 61: settings.sealWard = true; break;
  case 62: settings.etrisMultiplier *= 1.5f; break;
  case 63:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.lastStand = lastStandReady = true;
    settings.invincibility += 2;
    settings.difficulty -= 2;
    break;
  case 64:
    settings.experienceMultiplier *= 2;
    settings.bonusLevelItems++;
    settings.extraLives++;
    settings.shield = shieldReady = true;
    settings.hitbox *= 0.8f;
    settings.difficulty--;
    break;
  case 65:
    settings.bulletSpeed *= 0.65f;
    settings.bulletSpawn *= 1.4f;
    settings.clearBullets = true;
    settings.clearBulletsAt = 20;
    break;
  case 66:
    settings.ringGap *= 1.5f;
    settings.ringSpeed *= 0.7f;
    settings.ringDash = true;
    settings.ringDashCooldown *= 0.5f;
    break;
  case 67:
    settings.mirrorRightSpeed *= 0.5f;
    settings.mirrorInvincible = true;
    settings.mirrorInvincibleDuration = 10;
    settings.mirrorRepel = true;
    break;
  case 68:
    settings.sealWarning += 0.8f;
    settings.sealCharge *= 1.5f;
    settings.sealSpawn *= 1.3f;
    settings.sealWard = true;
    settings.difficulty -= 2;
    break;
  case 69: settings.spikesBallSpeed *= 0.85f; break;
  case 70: settings.invincibility += 0.75f; break;
  case 71:
    settings.extraLives += 3;
    settings.shield = shieldReady = true;
    settings.lastStand = lastStandReady = true;
    settings.invincibility += 2;
    settings.spikesBallSpeed *= 0.5f;
    settings.bulletSize *= 0.7f;
    break;
  case 72:
    settings.extraLives++;
    settings.shield = shieldReady = true;
    settings.hitbox *= 0.75f;
    settings.sealCharge *= 1.3f;
    settings.sealDashCooldown *= 0.6f;
    settings.difficulty -= 2;
    break;
  case 73:
    settings.extraLives++;
    settings.playerSpeed *= 1.12f;
    settings.hitbox *= 0.9f;
    break;
  case 74:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.hitbox *= 0.7f;
    settings.boss4ShardHeal = 1;
    settings.difficulty -= 2;
    break;
  case 75:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.hitbox *= 0.75f;
    settings.invincibility += 1.5f;
    break;
  case 76:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.playerSpeed *= 1.15f;
    settings.hitbox *= 0.75f;
    settings.invincibility += 1;
    break;
  case 77:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.lastStand = lastStandReady = true;
    settings.hitbox *= 0.65f;
    settings.invincibility += 1.5f;
    break;
  case 78:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.lastStand = lastStandReady = true;
    settings.playerSpeed *= 1.1f;
    settings.hitbox *= 0.8f;
    break;
  case 79:
    settings.extraLives++;
    settings.shield = shieldReady = true;
    settings.playerSpeed *= 1.15f;
    settings.hitbox *= 0.75f;
    settings.invincibility += 1;
    break;
  case 80:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.sealWard = true;
    settings.sealWarning += 0.6f;
    settings.sealSpawn *= 1.3f;
    settings.difficulty -= 2;
    break;
  case 81:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.hitbox *= 0.7f;
    settings.invincibility += 1.5f;
    settings.sealWarning += 0.8f;
    settings.sealSpawn *= 1.3f;
    settings.sealDashCooldown *= 0.8f;
    settings.sealWard = true;
    settings.difficulty -= 2;
    break;
  case 82:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.lastStand = lastStandReady = true;
    settings.hitbox *= 0.65f;
    settings.invincibility += 1.25f;
    settings.sealDashCooldown *= 0.7f;
    settings.sealSpawn *= 1.2f;
    settings.sealWard = true;
    settings.difficulty -= 2;
    break;
  case 83:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.hitbox *= 0.7f;
    settings.invincibility += 1.5f;
    settings.sealWarning += 0.8f;
    settings.sealSpawn *= 1.3f;
    settings.sealDashCooldown *= 0.8f;
    settings.sealWard = true;
    settings.difficulty -= 2;
    break;
  case 84:
    settings.extraLives += 2;
    settings.shield = shieldReady = true;
    settings.lastStand = lastStandReady = true;
    settings.hitbox *= 0.65f;
    settings.invincibility += 1.25f;
    settings.sealDashCooldown *= 0.7f;
    settings.sealSpawn *= 1.2f;
    settings.sealWard = true;
    settings.difficulty -= 2;
    break;
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

bool FunctionalBlockDamageAtLives(int lives) {
  if (FunctionalBlockDamage())
    return true;
  if (!settings.lastStand || !lastStandReady || lives > 1)
    return false;
  lastStandReady = false;
  return true;
}

void FunctionalApplyCharacter(int character) {
  switch (character) {
  case 2: settings.experienceMultiplier *= 1.15f; break;
  case 3: settings.etrisMultiplier *= 1.1f; break;
  case 4: settings.bonusLevelItems++; break;
  case 5: settings.etrisMultiplier *= 1.15f; break;
  }
}

void FunctionalApplyCharacterTitle(int character) {
  switch (character) {
  case 2: settings.experienceMultiplier *= 1.1f; break;
  case 3: settings.etrisMultiplier *= 1.1f; break;
  case 4: settings.bonusLevelItems++; break;
  case 5: settings.experienceMultiplier *= 1.15f; break;
  }
}

void FunctionalApplyPlayerTitle(int title) {
  switch (title) {
  case 0: settings.etrisMultiplier *= 1.25f; break;
  case 1: settings.experienceMultiplier *= 1.2f; break;
  case 2: settings.experienceMultiplier *= 1.1f; break;
  case 3: settings.bonusLevelItems++; break;
  case 4: settings.etrisMultiplier *= 1.1f; break;
  case 5: settings.etrisMultiplier *= 1.15f; break;
  case 6: settings.experienceMultiplier *= 1.1f; break;
  case 7: settings.experienceMultiplier *= 1.05f; break;
  case 8: settings.bonusLevelItems++; break;
  case 9: settings.etrisMultiplier *= 1.1f; break;
  case 10: settings.etrisMultiplier *= 1.2f; break;
  case 11: settings.experienceMultiplier *= 1.15f; break;
  case 12: settings.bonusLevelItems++; break;
  }
}
