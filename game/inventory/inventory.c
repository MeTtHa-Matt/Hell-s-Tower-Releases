#include "inventory.h"
#include "../achievements/achievements.h"
#include "../characters/characters.h"
#include "../items/items.h"
#include "../levels/levels.h"
#include "../tower.h"
#include "../ui_scale.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

#define SLOT 88
#define PITCH 98
#define STORED_SLOT 70
#define STORED_PITCH 82
#define QUEST_SLOT 58
#define QUEST_GAP 8
#define COLUMNS 5
#define ROWS 5

static Texture2D playerTex;
static int playerTexCharacter = -1;
static float scale, originX, originY;

static Rectangle Box(float x, float y, float w, float h) {
  return (Rectangle){originX + x * scale, originY + y * scale, w * scale, h * scale};
}

static Vector2 Point(float x, float y) {
  return (Vector2){originX + x * scale, originY + y * scale};
}

static void DrawCentered(const char *text, float cx, float cy, int size, Color color) {
  int fontSize = (int)(size * scale);
  int x = (int)(originX + cx * scale) - MeasureText(text, fontSize) / 2;
  int y = (int)(originY + cy * scale) - fontSize / 2;
  DrawText(text, x, y, fontSize, color);
}

static void DrawFitCentered(const char *text, float cx, float cy, int width,
                            int size, Color color) {
  while (size > 8 && MeasureText(text, (int)(size * scale)) > width * scale)
    size--;
  DrawCentered(text, cx, cy, size, color);
}

static int DrawWrappedCentered(const char *text, float cx, float top,
                               float width, int size, Color color) {
  int fontSize = (int)(size * scale);
  char line[384] = "";
  char lines[4][384];
  int lineCount = 0;
  const char *word = text;
  while (*word) {
    const char *end = strchr(word, ' ');
    size_t wordLength = end ? (size_t)(end - word) : strlen(word);
    char candidate[384];
    snprintf(candidate, sizeof(candidate), "%s%.*s",
             line[0] ? " " : "", (int)wordLength, word);
    char combined[384];
    snprintf(combined, sizeof(combined), "%s%s", line, candidate);
    if (MeasureText(combined, fontSize) > width * scale) {
      if (line[0] && lineCount < 4) {
        snprintf(lines[lineCount++], sizeof(lines[lineCount]), "%s", line);
        line[0] = '\0';
        snprintf(line, sizeof(line), "%.*s", (int)wordLength, word);
      } else if (!line[0]) {
        break;
      }
    } else {
      snprintf(line, sizeof(line), "%s", combined);
    }
    if (!end)
      break;
    word = end + 1;
  }
  if (*line && lineCount < 4)
    snprintf(lines[lineCount++], sizeof(lines[lineCount]), "%s", line);
  int lineHeight = (int)(size * scale) + (int)(4 * scale);
  int y = (int)(originY + top * scale);
  for (int i = 0; i < lineCount; i++) {
    int x = (int)(originX + cx * scale) - MeasureText(lines[i], fontSize) / 2;
    DrawText(lines[i], x, y + i * lineHeight, fontSize, color);
  }
  return lineCount;
}

static void DrawSlot(Rectangle r, Color fill, Color border) {
  Color line = border;
  if (CheckCollisionPointRec(GetMousePosition(), r))
    line = WHITE;

  DrawRectangleRec(r, fill);
  DrawRectangleLinesEx(r, 4 * scale, line);
}

static void DrawPlayer(void) {
  if (playerTex.id == 0)
    return;

  float scaleW = 340.0f / playerTex.width;
  float scaleH = 450.0f / playerTex.height;
  float playerScale = scaleW;
  if (scaleH < playerScale)
    playerScale = scaleH;
  playerScale *= scale;

  float w = playerTex.width * playerScale;
  float h = playerTex.height * playerScale;
  Vector2 center = Point(220, 358);
  DrawTextureEx(playerTex, (Vector2){center.x - w / 2, center.y - h / 2},
                0, playerScale, WHITE);
}

bool DrawInventory(bool allowClose) {
  int character = TowerPlayerCharacter();
  if (character < 0 || character > 5)
    character = 1;
  if (playerTexCharacter != character) {
    if (playerTex.id != 0)
      UnloadTexture(playerTex);
    playerTex = LoadTexture(TextFormat("assets/player%d.png", character));
    playerTexCharacter = character;
    if (playerTex.id == 0)
      TraceLog(LOG_ERROR, "Portrait du personnage introuvable: %d", character);
  }

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();

  scale = sw * 0.9f / 1320;
  if (sh * 0.9f / 733 < scale)
    scale = sh * 0.9f / 733;
  originX = (sw - 1320 * scale) / 2 - 18 * scale;
  originY = (sh - 733 * scale) / 2 - 42 * scale;

  Rectangle panel = Box(18, 42, 1320, 733);
  if (allowClose &&
      (IsKeyPressed(KEY_ESCAPE) ||
       (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(GetMousePosition(), panel)))) {
    if (ItemsLetterReaderActive()) {
      ItemsCloseLetterReader();
      return true;
    }
    ItemsCloseContextMenu();
    return false;
  }

  Color gold = {225, 185, 40, 255};
  Color darkGold = {170, 130, 20, 255};
  Color textColor = {245, 230, 140, 255};

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 170});
  DrawRectangleRec(Box(18, 42, 1320, 733), gold);
  DrawRectangleRec(Box(30, 54, 1296, 709), BLACK);
  DrawRectangleLinesEx(Box(30, 54, 1296, 709), 3 * scale, darkGold);
  static const char *names[] = {
      "Melina", "Elyssia", "Kaelen", "Gorran", "Ysolde", "Thaddeus"};
  Rectangle nameBox = Box(72, 114, 296, 42);
  DrawRectangleRounded(nameBox, 0.04f, 8, (Color){18, 20, 24, 245});
  DrawRectangleRoundedLinesEx(nameBox, 0.04f, 8, 2 * scale,
                              (Color){225, 185, 40, 255});
  DrawCentered(names[character], 220, 135, 22, textColor);
  DrawPlayer();
  Rectangle skillInfo = Box(420, 266, 390, 90);
  DrawRectangleRounded(skillInfo, 0.025f, 8,
                       (Color){18, 20, 24, 245});
  DrawRectangleRoundedLinesEx(skillInfo, 0.025f, 8, 2 * scale,
                              (Color){170, 130, 20, 255});
  BeginScissorMode((int)skillInfo.x, (int)skillInfo.y,
                   (int)skillInfo.width, (int)skillInfo.height);
  DrawCentered("COMPÉTENCE", 615, 284, 16,
               (Color){225, 185, 40, 255});
  DrawWrappedCentered(CharacterSkillName(character), 615,
                      302, 350, 18, textColor);
  DrawWrappedCentered(CharacterSkillDescription(character),
                      615, 326, 350, 13,
                      (Color){235, 230, 210, 255});
  EndScissorMode();

  Rectangle titleInfo = Box(420, 370, 390, 100);
  DrawRectangleRounded(titleInfo, 0.025f, 8,
                       (Color){18, 20, 24, 245});
  DrawRectangleRoundedLinesEx(titleInfo, 0.025f, 8, 2 * scale,
                              (Color){170, 130, 20, 255});
  BeginScissorMode((int)titleInfo.x, (int)titleInfo.y,
                   (int)titleInfo.width, (int)titleInfo.height);
  DrawCentered("TITRE DU PERSONNAGE", 615, 388, 16,
               (Color){225, 185, 40, 255});
  DrawFitCentered(CharacterTitleName(character), 615, 410,
                  350, 17, textColor);
  DrawWrappedCentered(CharacterTitleDescription(character), 615,
                      432, 350, 13,
                      (Color){235, 230, 210, 255});
  EndScissorMode();

  Rectangle playerTitles = Box(420, 484, 390, 70);
  DrawRectangleRounded(playerTitles, 0.025f, 8,
                       (Color){18, 20, 24, 245});
  DrawRectangleRoundedLinesEx(playerTitles, 0.025f, 8, 2 * scale,
                              (Color){170, 130, 20, 255});
  BeginScissorMode((int)playerTitles.x, (int)playerTitles.y,
                   (int)playerTitles.width, (int)playerTitles.height);
  float playerTitleLabelY = 502;
  DrawCentered("TITRES DU JOUEUR", 615, playerTitleLabelY, 16,
               (Color){225, 185, 40, 255});
  if (AchievementsTitleCount() == 0) {
    DrawFitCentered("Aucun titre personnel débloqué.", 615,
                    playerTitleLabelY + 24, 350, 14,
                    (Color){210, 205, 190, 255});
  } else {
    int titleCount = AchievementsTitleCount();
    if (titleCount > 5)
      titleCount = 5;
    for (int i = 0; i < titleCount; i++)
      DrawFitCentered(AchievementsTitleName(i), 615,
                      playerTitleLabelY + 24 + i * 18, 350, 14,
                      (Color){235, 220, 155, 255});
  }
  EndScissorMode();

  DrawCentered(TextFormat("LVL %d", LevelsLevel()), 212, 602, 40, textColor);
  const float expWidth = 320.0f;
  const float expX = 212.0f - expWidth / 2.0f;
  DrawRectangleRec(Box(expX, 636, expWidth, 33), (Color){220, 220, 30, 255});
  DrawRectangleRec(Box(expX + 6, 642, expWidth - 12, 21), (Color){95, 105, 20, 255});
  float progress = (float)LevelsXP() / LevelsXPNeeded();
  if (progress > 1)
    progress = 1;
  DrawRectangleRec(Box(expX + 6, 642,
                       (expWidth - 12) * progress, 21),
                   (Color){220, 190, 45, 255});
  DrawCentered(TextFormat("EXP %d/%d", LevelsXP(), LevelsXPNeeded()), 212, 704, 34,
               textColor);

  Rectangle equipped[COLUMNS];
  Rectangle stored[COLUMNS * ROWS];
  Rectangle quest[QUEST_ITEM_COUNT] = {0};
  bool showQuestItems = character == 0;
  DrawCentered("EQUIPES", 1040, 80, 20, textColor);
  for (int i = 0; i < COLUMNS; i++) {
    Rectangle slot = Box(830 + i * PITCH, 100, SLOT, SLOT);
    equipped[i] = slot;
    DrawSlot(slot, BLACK, (Color){150, 150, 150, 255});
  }

  DrawCentered("SAC", 1040, 205, 20, textColor);
  float storedWidth = COLUMNS * STORED_SLOT +
                      (COLUMNS - 1) * (STORED_PITCH - STORED_SLOT);
  float storedX = 830 + (480 - storedWidth) / 2;
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLUMNS; col++) {
      int index = row * COLUMNS + col;
      Rectangle slot = Box(storedX + col * STORED_PITCH,
                           223 + row * STORED_PITCH, STORED_SLOT, STORED_SLOT);
      stored[index] = slot;
      DrawSlot(slot, (Color){38, 48, 48, 255}, (Color){110, 115, 115, 255});
    }
  }

  if (showQuestItems) {
    DrawCentered("OBJETS DE QUETE", 1040, 646, 22, textColor);
    float questWidth = QUEST_SLOT * QUEST_ITEM_COUNT +
                       QUEST_GAP * (QUEST_ITEM_COUNT - 1);
    float questX = 830 + (480 - questWidth) / 2;
    for (int i = 0; i < QUEST_ITEM_COUNT; i++) {
      Rectangle slot = Box(questX + i * (QUEST_SLOT + QUEST_GAP), 666,
                           QUEST_SLOT, QUEST_SLOT);
      quest[i] = slot;
      DrawSlot(slot, (Color){33, 30, 25, 255}, (Color){170, 130, 65, 255});
    }
  }

  Rectangle itemInfo = Box(420, 448, 390, 278);
  ItemsDrawInventory(equipped, stored, quest, showQuestItems, itemInfo);

  return true;
}

void UnloadInventory(void) {
  UnloadTexture(playerTex);
  playerTex = (Texture2D){0};
  playerTexCharacter = -1;
}