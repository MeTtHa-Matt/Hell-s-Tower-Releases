#include "zone.h"
#include "../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static Texture2D monsterTex, dialogueTex;
static bool dialogueTexTried;
static char loadedPath[512], damageDialogue[256];
static int lastDamage;
static float dialogueTimer;

static bool ChooseDamageDialogue(void) {
  FILE *file = fopen("assets/monster_dialogues.txt", "r");
  char line[256];
  int count = 0;

  damageDialogue[0] = '\0';
  if (!file) {
    TraceLog(LOG_ERROR, "Fichier de dialogues introuvable");
    return false;
  }

  while (fgets(line, 256, file)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0')
      continue;
    count++;
    if (GetRandomValue(1, count) == 1)
      strcpy(damageDialogue, line);
  }

  fclose(file);
  return damageDialogue[0] != '\0';
}

static void UpdateDialogue(MiniGameDamageCallback damageCount) {
  int damage = 0;
  if (damageCount)
    damage = damageCount();

  if (damage != lastDamage) {
    dialogueTimer = 0;
    if (damage > lastDamage && ChooseDamageDialogue())
      dialogueTimer = 3.0f;
    lastDamage = damage;
  } else if (dialogueTimer > 0) {
    dialogueTimer -= GetFrameTime();
  }
}

static void DrawDamageDialogue(Rectangle frame, Rectangle monster) {
  if (dialogueTimer <= 0)
    return;

  if (!dialogueTexTried) {
    dialogueTexTried = true;
    dialogueTex = LoadTexture("assets/dialoguebox1.png");
  }
  if (dialogueTex.id == 0)
    return;

  float scale = GameUiScale();
  float x = monster.x + monster.width + 8 * scale;
  float right = GetScreenWidth() - 8 * scale;
  int padding = GameUiPixels(24);

  int size = GameUiPixels(20);
  int textWidth = MeasureText(damageDialogue, size);
  int minSize = GameUiPixels(12);
  if (x + textWidth + padding > right)
    x = monster.x - textWidth - padding - 8 * scale;
  if (x < 8 * scale)
    x = 8 * scale;
  float maxWidth = right - x;
  while (size > minSize && textWidth + padding > maxWidth) {
    size--;
    textWidth = MeasureText(damageDialogue, size);
  }
  if (maxWidth <= 0)
    return;

  Rectangle box = {x, frame.y - 48 * scale,
                   fminf(textWidth + padding, maxWidth), 40 * scale};
  if (box.y < 8 * scale)
    box.y = 8 * scale;
  if (box.width > maxWidth)
    box.width = maxWidth;

  Rectangle source = {0, 0, (float)dialogueTex.width, (float)dialogueTex.height};
  DrawTexturePro(dialogueTex, source, box, (Vector2){0, 0}, 0, WHITE);

  int textX = (int)(box.x + (box.width - textWidth) / 2);
  int textY = (int)(box.y + (box.height - size) / 2 + 2 * scale);
  DrawText(damageDialogue, textX, textY, size, RAYWHITE);
}

static void DrawMonsterName(const char *imagePath, Rectangle monster) {
  const char *name = GetFileNameWithoutExt(imagePath);

  float scale = GameUiScale();
  int size = (int)(GetScreenHeight() * 0.03f);
  if (size < GameUiPixels(16))
    size = GameUiPixels(16);
  if (size > GameUiPixels(28))
    size = GameUiPixels(28);

  int textWidth = MeasureText(name, size);
  Rectangle box = {0, 0, textWidth + 24 * scale, size + 14 * scale};

  box.x = monster.x + (monster.width - box.width) / 2;
  if (box.x < 4 * scale)
    box.x = 4 * scale;
  if (box.x > GetScreenWidth() - box.width - 4 * scale)
    box.x = GetScreenWidth() - box.width - 4 * scale;

  box.y = monster.y - box.height - 6 * scale;
  if (box.y < 4 * scale)
    box.y = 4 * scale;

  DrawRectangleRec(box, (Color){20, 20, 24, 230});
  DrawRectangleLinesEx(box, 2 * scale, RAYWHITE);

  int textX = (int)(box.x + (box.width - textWidth) / 2);
  int textY = (int)(box.y + (box.height - size) / 2);
  DrawText(name, textX, textY, size, RAYWHITE);
}

static void LoadZoneMonster(const char *path) {
  if (strcmp(loadedPath, path) == 0)
    return;
  snprintf(loadedPath, 512, "%s", path);
  UnloadTexture(monsterTex);
  monsterTex = LoadTexture(path);
}

void DrawGameZone(MiniGameCallback minigame,
                  MiniGameDamageCallback damageCount,
                  const char *monsterImagePath) {
  float sw = (float)GetScreenWidth();
  float sh = (float)GetScreenHeight();
  float ui = GameUiScale();
  float fw = sw * 0.58f;
  float fh = sh * 0.56f;
  Rectangle frame = {(sw - fw) / 2, sh * 0.94f - fh, fw, fh};

  DrawRectangleRec(frame, BLACK);
  DrawRectangleLinesEx(frame, 3 * ui, RAYWHITE);

  if (minigame) {
    minigame((Rectangle){frame.x + 3, frame.y + 3, frame.width - 6, frame.height - 6});
    UpdateDialogue(damageCount);
  }

  if (!monsterImagePath || !monsterImagePath[0])
    return;
  LoadZoneMonster(monsterImagePath);
  if (monsterTex.id == 0)
    return;

  float mw = frame.width * 0.28f;
  float mh = mw * monsterTex.height / monsterTex.width;
  float bob = sinf((float)GetTime() * 4) * 5 * ui;
  Rectangle monster = {frame.x + (frame.width - mw) / 2,
                       frame.y - mh + 8 * ui + bob, mw, mh};

  Rectangle source = {0, 0, (float)monsterTex.width, (float)monsterTex.height};
  DrawTexturePro(monsterTex, source, monster, (Vector2){0, 0}, 0, WHITE);

  DrawMonsterName(monsterImagePath, monster);
  DrawDamageDialogue(frame, monster);
}

void UnloadGameZone(void) {
  UnloadTexture(monsterTex);
  UnloadTexture(dialogueTex);
  monsterTex = (Texture2D){0};
  dialogueTex = (Texture2D){0};
  dialogueTexTried = false;
  loadedPath[0] = '\0';
  lastDamage = 0;
  dialogueTimer = 0;
}