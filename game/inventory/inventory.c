#include "inventory.h"
#include "../items/items.h"
#include "../levels/levels.h"
#include "../tower.h"
#include "raylib.h"

#define SLOT 88
#define PITCH 98
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
  float scaleH = 410.0f / playerTex.height;
  float playerScale = scaleW;
  if (scaleH < playerScale)
    playerScale = scaleH;
  playerScale *= scale;

  float w = playerTex.width * playerScale;
  float h = playerTex.height * playerScale;
  Vector2 center = Point(250, 298);
  DrawTextureEx(playerTex, (Vector2){center.x - w / 2, center.y - h / 2},
                0, playerScale, WHITE);
}

bool DrawInventory(bool allowClose) {
  int character = TowerPlayerCharacter();
  if (character < 0 || character > 2)
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

  scale = sw * 0.9f / 987;
  if (sh * 0.9f / 733 < scale)
    scale = sh * 0.9f / 733;
  originX = (sw - 987 * scale) / 2 - 18 * scale;
  originY = (sh - 733 * scale) / 2 - 42 * scale;

  Rectangle panel = Box(18, 42, 987, 733);
  if (allowClose && (IsKeyPressed(KEY_ESCAPE) ||
      (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
       !CheckCollisionPointRec(GetMousePosition(), panel)))) {
    ItemsCloseContextMenu();
    return false;
  }

  Color gold = {225, 185, 40, 255};
  Color darkGold = {170, 130, 20, 255};
  Color textColor = {245, 230, 140, 255};

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 170});
  DrawRectangleRec(Box(18, 42, 987, 733), gold);
  DrawRectangleRec(Box(30, 54, 963, 709), BLACK);
  DrawRectangleLinesEx(Box(30, 54, 963, 709), 3 * scale, darkGold);

  DrawPlayer();
  DrawRectangleRec(Box(170, 66, 160, 38), (Color){18, 18, 24, 240});
  DrawRectangleLinesEx(Box(170, 66, 160, 38), 3 * scale, darkGold);
  static const char *names[] = {"Melina", "Elyssia", "Kaelen"};
  DrawCentered(names[character], 250, 85, 24, textColor);

  DrawCentered(TextFormat("LVL %d", LevelsLevel()), 245, 562, 40, textColor);
  DrawRectangleRec(Box(88, 595, 322, 33), (Color){220, 220, 30, 255});
  DrawRectangleRec(Box(94, 601, 310, 21), (Color){95, 105, 20, 255});
  float progress = (float)LevelsXP() / LevelsXPNeeded();
  if (progress > 1)
    progress = 1;
  DrawRectangleRec(Box(94, 601, 310 * progress, 21), (Color){220, 190, 45, 255});
  DrawCentered(TextFormat("EXP %d/%d", LevelsXP(), LevelsXPNeeded()), 249, 663, 40,
               textColor);

  Rectangle equipped[COLUMNS];
  Rectangle stored[COLUMNS * ROWS];
  for (int i = 0; i < COLUMNS; i++) {
    Rectangle slot = Box(481 + i * PITCH, 97, SLOT, SLOT);
    equipped[i] = slot;
    DrawSlot(slot, BLACK, (Color){150, 150, 150, 255});
  }

  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLUMNS; col++) {
      int index = row * COLUMNS + col;
      Rectangle slot = Box(481 + col * PITCH, 225 + row * 96, SLOT, SLOT);
      stored[index] = slot;
      DrawSlot(slot, (Color){38, 48, 48, 255}, (Color){110, 115, 115, 255});
    }
  }

  ItemsDrawInventory(equipped, stored);

  return true;
}

void UnloadInventory(void) {
  UnloadTexture(playerTex);
  playerTex = (Texture2D){0};
  playerTexCharacter = -1;
}