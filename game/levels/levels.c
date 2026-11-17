#include "levels.h"
#include "../ui_scale.h"
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

  float scale = GameUiScale();
  float maxScale = scale > 1 ? scale : 1;
  int width = (int)(460 * maxScale), height = (int)(150 * maxScale);
  if (width > GetScreenWidth() - 24)
    width = GetScreenWidth() - 24;
  if (height > GetScreenHeight() * 0.34f)
    height = (int)(GetScreenHeight() * 0.34f);
  int x = (GetScreenWidth() - width) / 2;
  int y = GetScreenHeight() - height - GameUiPixels(24);
  DrawRectangle(x, y, width, height, (Color){18, 18, 24, 240});
  DrawRectangleLinesEx((Rectangle){x, y, width, height}, 2 * scale,
                       (Color){225, 185, 40, 255});
  if (rewardLevelUp && elapsed < 2) {
    const char *title = "NIVEAU SUPERIEUR !";
    int titleSize = GameUiPixels(36);
    DrawText(title, (GetScreenWidth() - MeasureText(title, titleSize)) / 2,
             y + (height - titleSize) / 2, titleSize,
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
  int margin = GameUiPixels(38), barHeight = GameUiPixels(24);
  Rectangle bar = {x + margin, y + GameUiPixels(48),
                   width - margin * 2, barHeight};
  int levelSize = GameUiPixels(22);
  DrawText(TextFormat("NIVEAU %d", shownLevel),
           (GetScreenWidth() - MeasureText(TextFormat("NIVEAU %d", shownLevel),
                                            levelSize)) / 2,
           y + GameUiPixels(12), levelSize, (Color){245, 220, 130, 255});
  DrawRectangleRec(bar, (Color){95, 105, 20, 255});
  DrawRectangleRec((Rectangle){bar.x, bar.y, bar.width * shownXP / needed, bar.height},
                   (Color){220, 190, 45, 255});
  DrawRectangleLinesEx(bar, 2 * scale, (Color){225, 185, 40, 255});
  int xpSize = GameUiPixels(18), rewardSize = GameUiPixels(20);
  DrawText(TextFormat("EXP %d/%d", shownXP, needed),
           (GetScreenWidth() -
            MeasureText(TextFormat("EXP %d/%d", shownXP, needed), xpSize)) / 2,
           y + GameUiPixels(78), xpSize, RAYWHITE);
  DrawText(TextFormat("+%d XP", rewardAmount),
           (GetScreenWidth() -
            MeasureText(TextFormat("+%d XP", rewardAmount), rewardSize)) / 2,
           y + GameUiPixels(108), rewardSize, (Color){245, 220, 130, 255});
}
