#include "zone.h"
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

  float x = monster.x + monster.width + 8;
  float maxWidth = GetScreenWidth() - x - 8;

  int size = 20;
  int textWidth = MeasureText(damageDialogue, size);
  while (size > 12 && textWidth + 24 > maxWidth) {
    size--;
    textWidth = MeasureText(damageDialogue, size);
  }

  Rectangle box = {x, frame.y - 48, textWidth + 24.0f, 40};
  if (box.y < 8)
    box.y = 8;
  if (box.width > maxWidth)
    box.width = maxWidth;

  Rectangle source = {0, 0, (float)dialogueTex.width, (float)dialogueTex.height};
  DrawTexturePro(dialogueTex, source, box, (Vector2){0, 0}, 0, WHITE);

  int textX = (int)(box.x + (box.width - textWidth) / 2);
  int textY = (int)(box.y + (box.height - size) / 2 + 2);
  DrawText(damageDialogue, textX, textY, size, RAYWHITE);
}

static void DrawMonsterName(const char *imagePath, Rectangle monster) {
  const char *name = GetFileNameWithoutExt(imagePath);

  int size = (int)(GetScreenHeight() * 0.03f);
  if (size < 16)
    size = 16;
  if (size > 28)
    size = 28;

  int textWidth = MeasureText(name, size);
  Rectangle box = {0, 0, textWidth + 24.0f, size + 14.0f};

  box.x = monster.x + (monster.width - box.width) / 2;
  if (box.x < 4)
    box.x = 4;
  if (box.x > GetScreenWidth() - box.width - 4)
    box.x = GetScreenWidth() - box.width - 4;

  box.y = monster.y - box.height - 6;
  if (box.y < 4)
    box.y = 4;

  DrawRectangleRec(box, (Color){20, 20, 24, 230});
  DrawRectangleLinesEx(box, 2, RAYWHITE);

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
  float fw = sw * 0.58f;
  float fh = sh * 0.56f;
  Rectangle frame = {(sw - fw) / 2, sh * 0.94f - fh, fw, fh};

  DrawRectangleRec(frame, BLACK);
  DrawRectangleLinesEx(frame, 3, RAYWHITE);

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
  float bob = sinf((float)GetTime() * 4) * 5;
  Rectangle monster = {frame.x + (frame.width - mw) / 2, frame.y - mh + 8 + bob, mw, mh};

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