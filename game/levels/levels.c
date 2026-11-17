#include "levels.h"
#include "raylib.h"
#include <limits.h>
#include <math.h>

static int level, xp;
static int rewardLevel, rewardXP, rewardAmount;
static float notice;
static bool rewardLevelUp;

static int XPNeeded(int atLevel) {
  double needed = 100 * pow(1.5, atLevel);
  return needed > INT_MAX ? INT_MAX : (int)ceil(needed);
}

int LevelsXPNeeded(void) {
  return XPNeeded(level);
}

void LevelsLoad(int newLevel, int newXP) {
  level = newLevel < 0 ? 0 : newLevel;
  xp = newXP < 0 ? 0 : newXP;
  notice = 0;
  while (xp >= LevelsXPNeeded() && level < INT_MAX) {
    xp -= LevelsXPNeeded();
    level++;
  }
}

int LevelsLevel(void) { return level; }
int LevelsXP(void) { return xp; }

void LevelsAddXP(int amount) {
  if (amount <= 0)
    return;
  xp = amount > INT_MAX - xp ? INT_MAX : xp + amount;
  if (xp >= LevelsXPNeeded()) {
    while (xp >= LevelsXPNeeded() && level < INT_MAX) {
      xp -= LevelsXPNeeded();
      level++;
    }
    notice = 5;
  }
}

void LevelsShowReward(int oldLevel, int oldXP, int gainedXP) {
  rewardLevel = oldLevel;
  rewardXP = oldXP;
  rewardAmount = gainedXP;
  rewardLevelUp = level > oldLevel;
  notice = 5;
}

void LevelsDrawNotification(void) {
  if (notice <= 0)
    return;
  float elapsed = 5 - notice;
  notice -= GetFrameTime();
  if (notice < 0)
    notice = 0;

  int width = 460, height = 150;
  int x = (GetScreenWidth() - width) / 2;
  int y = GetScreenHeight() - height - 24;
  DrawRectangle(x, y, width, height, (Color){18, 18, 24, 240});
  DrawRectangleLinesEx((Rectangle){x, y, width, height}, 2,
                       (Color){225, 185, 40, 255});
  if (rewardLevelUp && elapsed < 2) {
    const char *title = "NIVEAU SUPERIEUR !";
    DrawText(title, (GetScreenWidth() - MeasureText(title, 36)) / 2,
             y + (height - 36) / 2, 36,
             (Color){245, 220, 130, 255});
    return;
  }

  float progress = (elapsed - (rewardLevelUp ? 2 : 0)) / 2.5f;
  if (progress < 0)
    progress = 0;
  if (progress > 1)
    progress = 1;
  double shown = rewardXP + rewardAmount * progress;
  int shownLevel = rewardLevel, shownXP = (int)shown;
  while (shownXP >= XPNeeded(shownLevel) && shownLevel < level) {
    shownXP -= XPNeeded(shownLevel);
    shownLevel++;
  }
  int needed = XPNeeded(shownLevel);
  Rectangle bar = {x + 38, y + 48, width - 76, 24};
  DrawText(TextFormat("NIVEAU %d", shownLevel),
           (GetScreenWidth() - MeasureText(TextFormat("NIVEAU %d", shownLevel), 22)) / 2,
           y + 12, 22, (Color){245, 220, 130, 255});
  DrawRectangleRec(bar, (Color){95, 105, 20, 255});
  DrawRectangleRec((Rectangle){bar.x, bar.y, bar.width * shownXP / needed, bar.height},
                   (Color){220, 190, 45, 255});
  DrawRectangleLinesEx(bar, 2, (Color){225, 185, 40, 255});
  DrawText(TextFormat("EXP %d/%d", shownXP, needed),
           (GetScreenWidth() - MeasureText(TextFormat("EXP %d/%d", shownXP, needed), 18)) / 2,
           y + 78, 18, RAYWHITE);
  DrawText(TextFormat("+%d XP", rewardAmount),
           (GetScreenWidth() - MeasureText(TextFormat("+%d XP", rewardAmount), 20)) / 2,
           y + 108, 20, (Color){245, 220, 130, 255});
}
