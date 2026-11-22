#include "inventory.h"
#include "../achievements/achievements.h"
#include "../characters/characters.h"
#include "../items/items.h"
#include "../levels/levels.h"
#include "../tower.h"
#include "../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SLOT 76
#define PITCH 96
#define STORED_SLOT 66
#define STORED_PITCH 76
#define QUEST_SLOT 58
#define QUEST_GAP 8
#define COLUMNS 5
#define ROWS 5

static Texture2D playerTex;
static Texture2D pedestalTex;
static int playerTexCharacter = -1;
static float scale, originX, originY;
static int inventorySortMode = -1;
static float titleScroll = 0;
static bool titleDragging = false;
static float titleGrab = 0;

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
  int shadow = (int)(scale + 0.5f);
  if (shadow < 1)
    shadow = 1;
  DrawText(text, x + shadow, y + shadow, fontSize, (Color){0, 0, 0, 170});
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
    size_t wordLength = strlen(word);
    char combined[384];
    if (end)
      wordLength = (size_t)(end - word);
    if (line[0])
      snprintf(combined, sizeof(combined), "%s %.*s", line, (int)wordLength, word);
    else
      snprintf(combined, sizeof(combined), "%.*s", (int)wordLength, word);
    if (line[0] && MeasureText(combined, fontSize) > width * scale) {
      if (lineCount < 4) {
        snprintf(lines[lineCount], sizeof(lines[lineCount]), "%s", line);
        lineCount++;
      }
      snprintf(line, sizeof(line), "%.*s", (int)wordLength, word);
    } else {
      snprintf(line, sizeof(line), "%s", combined);
    }
    if (!end)
      break;
    word = end + 1;
  }
  if (*line && lineCount < 4) {
    snprintf(lines[lineCount], sizeof(lines[lineCount]), "%s", line);
    lineCount++;
  }
  int lineHeight = (int)(size * scale) + (int)(4 * scale);
  int y = (int)(originY + top * scale);
  for (int i = 0; i < lineCount; i++) {
    int x = (int)(originX + cx * scale) - MeasureText(lines[i], fontSize) / 2;
    DrawText(lines[i], x, y + i * lineHeight, fontSize, color);
  }
  return lineCount;
}

// Ligne séparatrice dorée qui s'estompe sur les bords.
static void DrawDivider(float cx, float cy, float width) {
  Rectangle r = Box(cx - width / 2, cy, width, 2);
  int half = (int)(r.width / 2);
  Color clear = {225, 185, 40, 0};
  Color gold = {225, 185, 40, 200};
  DrawRectangleGradientH((int)r.x, (int)r.y, half, (int)r.height, clear, gold);
  DrawRectangleGradientH((int)r.x + half, (int)r.y, half, (int)r.height, gold, clear);
}

// Cadre orné : double bordure, équerres dorées aux coins, losanges haut/bas.
static void DrawFrame(Rectangle r, Color accent) {
  float o = 3 * scale;
  float len = 14 * scale;
  float t = 3 * scale;
  Rectangle b = {r.x - o, r.y - o, r.width + 2 * o, r.height + 2 * o};
  Color dim = {110, 90, 25, 255};
  Color faint = {accent.r, accent.g, accent.b, 120};

  DrawRectangleLinesEx(r, 2 * scale, dim);
  DrawRectangleLinesEx(b, 1.5f * scale, faint);

  DrawRectangleRec((Rectangle){b.x, b.y, len, t}, accent);
  DrawRectangleRec((Rectangle){b.x, b.y, t, len}, accent);
  DrawRectangleRec((Rectangle){b.x + b.width - len, b.y, len, t}, accent);
  DrawRectangleRec((Rectangle){b.x + b.width - t, b.y, t, len}, accent);
  DrawRectangleRec((Rectangle){b.x, b.y + b.height - t, len, t}, accent);
  DrawRectangleRec((Rectangle){b.x, b.y + b.height - len, t, len}, accent);
  DrawRectangleRec((Rectangle){b.x + b.width - len, b.y + b.height - t, len, t}, accent);
  DrawRectangleRec((Rectangle){b.x + b.width - t, b.y + b.height - len, t, len}, accent);

  DrawPoly((Vector2){r.x + r.width / 2, b.y + t / 2}, 4, 5 * scale, 0, accent);
  DrawPoly((Vector2){r.x + r.width / 2, b.y + b.height - t / 2}, 4, 5 * scale, 0,
           accent);
}

// Carte d'information : fond sombre, bandeau de titre, cadre orné.
static void DrawCard(Rectangle r, Color accent) {
  Rectangle header = {r.x + 2 * scale, r.y + 2 * scale, r.width - 4 * scale, 26 * scale};
  DrawRectangleRec(r, (Color){14, 16, 20, 245});
  DrawRectangleGradientV((int)header.x, (int)header.y, (int)header.width,
                         (int)header.height, (Color){accent.r, accent.g, accent.b, 70},
                         (Color){accent.r, accent.g, accent.b, 0});
  DrawFrame(r, accent);
}

// Petites icônes dessinées avec des primitives : 0 flamme, 1 couronne, 2 bouclier.
static void DrawIcon(int kind, float cx, float cy) {
  Color orange = {235, 120, 30, 255};
  Color yellow = {255, 220, 90, 255};
  Color gold = {240, 200, 60, 255};
  Color steel = {170, 180, 195, 255};
  Color steelDark = {95, 105, 120, 255};

  switch (kind) {
  case 0:
    DrawCircleV(Point(cx, cy + 3), 7 * scale, orange);
    DrawTriangle(Point(cx, cy - 11), Point(cx - 7, cy + 3), Point(cx + 7, cy + 3), orange);
    DrawCircleV(Point(cx, cy + 5), 3.5f * scale, yellow);
    DrawTriangle(Point(cx, cy - 3), Point(cx - 3.5f, cy + 5), Point(cx + 3.5f, cy + 5),
                 yellow);
    break;
  case 1:
    DrawRectangleRec(Box(cx - 10, cy + 2, 20, 7), gold);
    DrawTriangle(Point(cx - 10, cy - 8), Point(cx - 14, cy + 2), Point(cx - 4, cy + 2), gold);
    DrawTriangle(Point(cx, cy - 11), Point(cx - 5, cy + 2), Point(cx + 5, cy + 2), gold);
    DrawTriangle(Point(cx + 10, cy - 8), Point(cx + 4, cy + 2), Point(cx + 14, cy + 2), gold);
    DrawRectangleRec(Box(cx - 10, cy + 5, 20, 2), (Color){170, 130, 20, 255});
    break;
  default:
    DrawRectangleRec(Box(cx - 9, cy - 10, 18, 11), steel);
    DrawTriangle(Point(cx - 9, cy + 1), Point(cx, cy + 11), Point(cx + 9, cy + 1), steel);
    DrawRectangleRec(Box(cx - 2, cy - 8, 4, 14), steelDark);
    DrawRectangleRec(Box(cx - 6, cy - 3, 12, 3), steelDark);
    break;
  }
}

static void DrawSlot(Rectangle r, Color fill, Color border) {
  Color line = border;
  Color top = {fill.r + 14, fill.g + 14, fill.b + 14, 255};
  bool hovered = CheckCollisionPointRec(GetMousePosition(), r);

  if (hovered) {
    Rectangle glow = {r.x - 3 * scale, r.y - 3 * scale, r.width + 6 * scale,
                      r.height + 6 * scale};
    DrawRectangleRec(glow, (Color){225, 185, 40, 70});
    line = (Color){255, 225, 110, 255};
  }

  DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height, top, fill);
  DrawRectangleLinesEx(
      (Rectangle){r.x + 4 * scale, r.y + 4 * scale, r.width - 8 * scale,
                  r.height - 8 * scale},
      1.5f * scale, (Color){0, 0, 0, 120});
  DrawRectangleLinesEx(r, 3 * scale, line);
}

static void DrawPlayer(void) {
  Color gold = {225, 185, 40, 255};
  Rectangle frame = Box(72, 160, 296, 430);
  DrawRectangleRec(frame, (Color){14, 16, 20, 235});

  if (playerTex.id != 0) {
    if (pedestalTex.id == 0) {
      pedestalTex = LoadTexture("assets/socle.png");
      if (pedestalTex.id == 0)
        TraceLog(LOG_ERROR, "Socle de l'inventaire introuvable");
      else
        SetTextureFilter(pedestalTex, TEXTURE_FILTER_POINT);
    }

    float scaleW = 420.0f / playerTex.width;
    float scaleH = 390.0f / playerTex.height;
    float playerScale = scaleW;
    if (scaleH < playerScale)
      playerScale = scaleH;
    playerScale *= scale;

    float w = playerTex.width * playerScale;
    float h = playerTex.height * playerScale;
    Vector2 feet = Point(220, 567.4f);
    Vector2 center = {feet.x, feet.y - h / 2};
    Rectangle content = {frame.x + 3 * scale, frame.y + 3 * scale,
                         frame.width - 6 * scale, frame.height - 6 * scale};
    BeginScissorMode((int)content.x, (int)content.y, (int)content.width,
                     (int)content.height);

    // Halo derrière le personnage et socle sous ses pieds.
    DrawCircleGradient(center, 170 * scale, (Color){225, 185, 40, 60},
                       (Color){225, 185, 40, 0});
    if (pedestalTex.id != 0) {
      float sourceTop = pedestalTex.height * 0.3711f;
      float sourceBottom = pedestalTex.height * 0.8301f;
      float surfaceY = pedestalTex.height * 0.5078f;
      Rectangle source = {0, sourceTop, (float)pedestalTex.width,
                          sourceBottom - sourceTop};
      float pedestalWidth = 360 * scale;
      float pedestalHeight =
          pedestalWidth * source.height / source.width;
      float surfaceOffset =
          pedestalHeight * (surfaceY - sourceTop) / source.height;
      Rectangle target = {center.x - pedestalWidth / 2,
                          feet.y - surfaceOffset, pedestalWidth, pedestalHeight};
      DrawTexturePro(pedestalTex, source, target, (Vector2){0, 0}, 0, WHITE);
    }
    Rectangle shadow = {center.x - w * 0.3f, center.y + h / 2 - 4 * scale,
                        w * 0.6f, 8 * scale};
    DrawRectangleRounded(shadow, 1.0f, 12, (Color){0, 0, 0, 130});

    DrawTextureEx(playerTex, (Vector2){center.x - w / 2, center.y - h / 2},
                  0, playerScale, WHITE);
    EndScissorMode();
  }

  DrawFrame(frame, gold);
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
    if (playerTex.id == 0) {
      TraceLog(LOG_ERROR, "Portrait du personnage introuvable: %d", character);
    } else {
      SetTextureFilter(playerTex, TEXTURE_FILTER_BILINEAR);
    }
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
    titleDragging = false;
    return false;
  }

  Color gold = {225, 185, 40, 255};
  Color darkGold = {170, 130, 20, 255};
  Color textColor = {245, 230, 140, 255};
  Color softText = {235, 230, 210, 255};
  Color cardBack = {14, 16, 20, 210};
  float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 2.0f);

  // Fond assombri avec vignette.
  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 175});
  DrawRectangleGradientV(0, 0, sw, sh / 4, (Color){0, 0, 0, 90}, (Color){0, 0, 0, 0});
  DrawRectangleGradientV(0, sh - sh / 4, sw, sh / 4, (Color){0, 0, 0, 0},
                         (Color){0, 0, 0, 90});

  // Cadre principal : bordure dorée, fond dégradé, coins ornés.
  Rectangle outer = Box(18, 42, 1320, 733);
  Rectangle inner = Box(30, 54, 1296, 709);
  DrawRectangleGradientV((int)outer.x, (int)outer.y, (int)outer.width,
                         (int)outer.height, (Color){240, 200, 60, 255},
                         (Color){170, 130, 20, 255});
  DrawRectangleGradientV((int)inner.x, (int)inner.y, (int)inner.width,
                         (int)inner.height, (Color){22, 20, 16, 255},
                         (Color){4, 4, 6, 255});
  DrawRectangleLinesEx(inner, 3 * scale, darkGold);

  float cornerX[4] = {30, 1326, 30, 1326};
  float cornerY[4] = {54, 54, 763, 763};
  float signX[4] = {1, -1, 1, -1};
  float signY[4] = {1, 1, -1, -1};
  Color ornament = {255, 225, 110, (unsigned char)(170 + 85 * pulse)};
  for (int i = 0; i < 4; i++) {
    float ox = cornerX[i] + signX[i] * 8;
    float oy = cornerY[i] + signY[i] * 8;
    float hx = ox;
    float hy = oy;
    float vx = ox;
    float vy = oy;
    if (signX[i] < 0) {
      hx = ox - 30;
      vx = ox - 3;
    }
    if (signY[i] < 0) {
      hy = oy - 3;
      vy = oy - 30;
    }
    DrawRectangleRec(Box(hx, hy, 30, 3), ornament);
    DrawRectangleRec(Box(vx, vy, 3, 30), ornament);
  }

  // ---------- Colonne gauche : personnage ----------
  static const char *names[] = {
      "Melina", "Elyssia", "Kaelen", "Gorran", "Ysolde", "Thaddeus"};
  DrawCentered("INVENTAIRE", 220, 80, 24, gold);
  DrawDivider(220, 94, 296);

  Rectangle nameBox = Box(72, 106, 296, 42);
  DrawRectangleRec(nameBox, (Color){18, 20, 24, 235});
  DrawFrame(nameBox, gold);
  DrawCentered(names[character], 220, 127, 22, textColor);

  DrawPlayer();

  // Niveau et expérience.
  DrawCentered(TextFormat("LVL %d", LevelsLevel()), 220, 618, 36, textColor);
  const float expWidth = 296.0f;
  const float expX = 72.0f;
  float progress = 0.0f;
  if (LevelsXPNeeded() > 0)
    progress = (float)LevelsXP() / LevelsXPNeeded();
  if (progress > 1)
    progress = 1;
  if (progress < 0)
    progress = 0;

  Rectangle expOuter = Box(expX, 640, expWidth, 33);
  Rectangle expTrack = Box(expX + 5, 645, expWidth - 10, 23);
  Rectangle expFill = Box(expX + 5, 645, (expWidth - 10) * progress, 23);
  DrawRectangleRounded(expOuter, 0.4f, 8, gold);
  DrawRectangleRounded(expTrack, 0.4f, 8, (Color){40, 44, 14, 255});
  if (expFill.width > 2 * scale) {
    DrawRectangleGradientV((int)expFill.x, (int)expFill.y, (int)expFill.width,
                           (int)expFill.height, (Color){250, 225, 90, 255},
                           (Color){200, 160, 30, 255});
    DrawRectangleRec((Rectangle){expFill.x, expFill.y, expFill.width, 3 * scale},
                     (Color){255, 255, 255, 90});
  }
  DrawCentered(TextFormat("%d%%", (int)(progress * 100)), 220, 657, 15, WHITE);
  DrawCentered(TextFormat("EXP %d/%d", LevelsXP(), LevelsXPNeeded()), 220, 702, 24,
               textColor);

  // ---------- Colonne centrale : fiches + détail objet ----------
  // Compétence
  Rectangle skillInfo = Box(402, 260, 390, 100);
  DrawCard(skillInfo, gold);
  BeginScissorMode((int)skillInfo.x, (int)skillInfo.y,
                   (int)skillInfo.width, (int)skillInfo.height);
  DrawIcon(0, 428, 274);
  DrawCentered("COMPÉTENCE", 597, 274, 16, gold);
  DrawFitCentered(CharacterSkillName(character), 597, 298, 350, 18, textColor);
  DrawDivider(597, 309, 330);
  DrawWrappedCentered(CharacterSkillDescription(character), 597, 318, 350, 13,
                      softText);
  EndScissorMode();

  // Titre du personnage
  Rectangle titleInfo = Box(402, 370, 390, 104);
  DrawCard(titleInfo, gold);
  BeginScissorMode((int)titleInfo.x, (int)titleInfo.y,
                   (int)titleInfo.width, (int)titleInfo.height);
  DrawIcon(1, 428, 384);
  DrawCentered("TITRE DU PERSONNAGE", 597, 384, 16, gold);
  DrawFitCentered(CharacterTitleName(character), 597, 408, 350, 17, textColor);
  DrawDivider(597, 420, 330);
  DrawWrappedCentered(CharacterTitleDescription(character), 597, 428, 350, 13,
                      softText);
  EndScissorMode();

  // Titres du joueur : liste avec barre de scroll (molette + curseur).
  Rectangle playerTitles = Box(402, 484, 390, 110);
  DrawCard(playerTitles, gold);
  float labelY = 502;
  float viewTop = 518;
  float viewHeight = 72;
  float rowPitch = 34;
  int titleCount = AchievementsTitleCount();
  float contentHeight = titleCount * rowPitch;
  float maxScroll = contentHeight - viewHeight;
  if (maxScroll < 0)
    maxScroll = 0;

  Rectangle trackRect = Box(776, viewTop + 2, 10, viewHeight - 4);
  float thumbHeight = trackRect.height;
  float freeSpace = 0;
  float thumbY = trackRect.y;
  Rectangle thumbRect = trackRect;
  Vector2 mouse = GetMousePosition();

  if (maxScroll > 0) {
    thumbHeight = trackRect.height * viewHeight / contentHeight;
    if (thumbHeight < 16 * scale)
      thumbHeight = 16 * scale;
    freeSpace = trackRect.height - thumbHeight;
    thumbY = trackRect.y + freeSpace * titleScroll / maxScroll;
    thumbRect = (Rectangle){trackRect.x, thumbY, trackRect.width, thumbHeight};

    if (allowClose) {
      if (CheckCollisionPointRec(mouse, playerTitles))
        titleScroll -= GetMouseWheelMove() * rowPitch;
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
          CheckCollisionPointRec(mouse, trackRect)) {
        titleDragging = true;
        if (CheckCollisionPointRec(mouse, thumbRect))
          titleGrab = mouse.y - thumbRect.y;
        else
          titleGrab = thumbHeight / 2;
      }
      if (titleDragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
          if (freeSpace > 0)
            titleScroll = (mouse.y - titleGrab - trackRect.y) / freeSpace * maxScroll;
        } else {
          titleDragging = false;
        }
      }
    }
  } else {
    titleDragging = false;
  }
  if (titleScroll > maxScroll)
    titleScroll = maxScroll;
  if (titleScroll < 0)
    titleScroll = 0;
  if (maxScroll > 0) {
    thumbY = trackRect.y + freeSpace * titleScroll / maxScroll;
    thumbRect = (Rectangle){trackRect.x, thumbY, trackRect.width, thumbHeight};
  }

  BeginScissorMode((int)playerTitles.x, (int)playerTitles.y,
                   (int)playerTitles.width, (int)playerTitles.height);
  DrawIcon(2, 428, labelY);
  DrawCentered("TITRES DU JOUEUR", 597, labelY, 16, gold);
  DrawCentered(TextFormat("%d", titleCount), 758, labelY, 14, softText);
  EndScissorMode();

  if (titleCount == 0) {
    DrawFitCentered("Aucun titre personnel débloqué.", 597, viewTop + 24, 350, 14,
                    (Color){170, 165, 150, 255});
  } else {
    Rectangle viewRect = Box(406, viewTop, 366, viewHeight);
    float textCenter = 597;
    float textWidth = 350;
    if (maxScroll > 0) {
      textCenter = 589;
      textWidth = 336;
    }
    BeginScissorMode((int)viewRect.x, (int)viewRect.y, (int)viewRect.width,
                     (int)viewRect.height);
    for (int i = 0; i < titleCount; i++) {
      float rowTop = viewTop + i * rowPitch - titleScroll;
      if (rowTop + rowPitch < viewTop || rowTop > viewTop + viewHeight)
        continue;
      DrawWrappedCentered(TextFormat("%s - %s",
                                     AchievementsTitleName(i),
                                     AchievementsTitleDescription(i)),
                          textCenter, rowTop + 2, textWidth, 12,
                          (Color){235, 220, 155, 255});
      if (i < titleCount - 1)
        DrawRectangleRec(        Box(422, rowTop + rowPitch - 2, 334, 1),
                         (Color){110, 90, 25, 120});
    }
    EndScissorMode();
  }

  if (maxScroll > 0) {
    Color thumbColor = gold;
    if (titleDragging || CheckCollisionPointRec(mouse, thumbRect))
      thumbColor = (Color){255, 235, 140, 255};
    DrawRectangleRec(trackRect, (Color){30, 28, 20, 255});
    DrawRectangleLinesEx(trackRect, 1.5f * scale, (Color){110, 90, 25, 255});
    DrawRectangleRec((Rectangle){thumbRect.x + scale, thumbRect.y + scale,
                                 thumbRect.width - 2 * scale,
                                 thumbRect.height - 2 * scale},
                     thumbColor);
  }

  // ---------- Colonne droite : équipement, sac, quêtes ----------
  Rectangle equipped[COLUMNS];
  Rectangle stored[COLUMNS * ROWS];
  Rectangle quest[QUEST_ITEM_COUNT] = {0};
  bool showQuestItems = character == 0;
  float columnCenter = 1071;

  // Équipement
  Rectangle equipCard = Box(826, 70, 490, 120);
  DrawRectangleRec(equipCard, cardBack);
  DrawFrame(equipCard, gold);
  DrawCentered("OBJETS ÉQUIPÉS", columnCenter, 87, 18, textColor);
  float equipWidth = COLUMNS * SLOT + (COLUMNS - 1) * (PITCH - SLOT);
  float equipX = columnCenter - equipWidth / 2;
  for (int i = 0; i < COLUMNS; i++) {
    Rectangle slot = Box(equipX + i * PITCH, 102, SLOT, SLOT);
    equipped[i] = slot;
    DrawSlot(slot, (Color){10, 10, 12, 255}, (Color){150, 150, 150, 255});
  }

  // Sac
  Rectangle bagCard = Box(826, 200, 490, 426);
  DrawRectangleRec(bagCard, cardBack);
  DrawFrame(bagCard, gold);
  DrawCentered("SAC", columnCenter, 221, 18, textColor);

  Rectangle sortButton = Box(1186, 208, 118, 28);
  bool sortHovered = CheckCollisionPointRec(GetMousePosition(), sortButton);
  Color sortFill = {38, 42, 45, 255};
  Color sortText = textColor;
  if (sortHovered) {
    sortFill = (Color){80, 68, 40, 255};
    sortText = WHITE;
  }
  DrawRectangleRounded(sortButton, 0.35f, 6, sortFill);
  DrawRectangleRoundedLinesEx(sortButton, 0.35f, 6, 2 * scale, gold);
  static const char *sortLabels[ITEM_SORT_COUNT] = {
      "RARETÉ", "NOM", "NB EFFETS"};
  const char *sortLabel = "TRIER";
  if (inventorySortMode >= 0)
    sortLabel = TextFormat("TRI : %s", sortLabels[inventorySortMode]);
  DrawFitCentered(sortLabel, 1245, 222, 104, 14, sortText);
  if (allowClose && sortHovered &&
      IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    int nextSortMode = ITEM_SORT_RARITY;
    if (inventorySortMode >= 0)
      nextSortMode = (inventorySortMode + 1) % ITEM_SORT_COUNT;
    if (ItemsSortStoredItems((ItemSortOrder)nextSortMode))
      inventorySortMode = nextSortMode;
  }

  float storedWidth = COLUMNS * STORED_SLOT +
                      (COLUMNS - 1) * (STORED_PITCH - STORED_SLOT);
  float storedX = columnCenter - storedWidth / 2;
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLUMNS; col++) {
      int index = row * COLUMNS + col;
      Rectangle slot = Box(storedX + col * STORED_PITCH,
                           246 + row * STORED_PITCH, STORED_SLOT, STORED_SLOT);
      stored[index] = slot;
      DrawSlot(slot, (Color){38, 48, 48, 255}, (Color){110, 115, 115, 255});
    }
  }

  // Objets de quête
  if (showQuestItems) {
    Rectangle questCard = Box(826, 636, 490, 102);
    DrawRectangleRec(questCard, (Color){22, 18, 12, 220});
    DrawFrame(questCard, (Color){200, 160, 80, 255});
    DrawCentered("OBJETS DE QUÊTE", columnCenter, 652, 18, textColor);
    float questSlot = QUEST_SLOT;
    float questWidth = questSlot * QUEST_ITEM_COUNT +
                       QUEST_GAP * (QUEST_ITEM_COUNT - 1);
    if (questWidth > 460) {
      questSlot = (460.0f - QUEST_GAP * (QUEST_ITEM_COUNT - 1)) / QUEST_ITEM_COUNT;
      questWidth = 460;
    }
    float questX = columnCenter - questWidth / 2;
    for (int i = 0; i < QUEST_ITEM_COUNT; i++) {
      Rectangle slot = Box(questX + i * (questSlot + QUEST_GAP), 670,
                           questSlot, questSlot);
      quest[i] = slot;
      DrawSlot(slot, (Color){33, 30, 25, 255}, (Color){170, 130, 65, 255});
    }
  }

  // Aide contextuelle en bas du cadre.
  DrawFitCentered("Clic : sélectionner / menu    |    Molette : défiler le sac / titres    |    ÉCHAP : fermer",
                  670, 752, 900, 13, (Color){150, 140, 100, 255});

  // Zone de détail de l'objet (sous les fiches, sans chevauchement).
  Rectangle itemInfo = Box(420, 474, 390, 264);
  ItemsDrawInventory(equipped, stored, quest, showQuestItems, itemInfo,
                     allowClose);

  return true;
}

void UnloadInventory(void) {
  if (playerTex.id != 0)
    UnloadTexture(playerTex);
  if (pedestalTex.id != 0)
    UnloadTexture(pedestalTex);
  playerTex = (Texture2D){0};
  pedestalTex = (Texture2D){0};
  playerTexCharacter = -1;
  titleScroll = 0;
  titleDragging = false;
}