#include "title.h"
#include <math.h>
#include <raylib.h>

static void Ornament(float cx, float y, float half, Color c) {
  Color clear = Fade(c, 0);

  DrawRectangleGradientH((int)(cx - half), (int)y - 1, (int)half, 2, clear, c);
  DrawRectangleGradientH((int)cx, (int)y - 1, (int)half, 2, c, clear);
  DrawPoly((Vector2){cx, y}, 4, 7, 0, c);
}

void DrawMenuTitle(const char *title) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float t = (float)GetTime();

  float size = sh * 0.08f;
  float space = size * 0.12f;
  Font font = GetFontDefault();
  Vector2 textSize = MeasureTextEx(font, title, size, space);
  Vector2 pos = {(sw - textSize.x) / 2, sh * 0.18f};
  Color gold = {235, 195, 105, 255};

  float centerX = sw / 2.0f;
  float ornamentHalf = textSize.x / 2 + 60;
  Ornament(centerX, pos.y - 15, ornamentHalf, gold);
  Ornament(centerX, pos.y + textSize.y + 15, ornamentHalf, gold);

  Vector2 shadowPos = {pos.x + size * 0.06f, pos.y + size * 0.06f};
  DrawTextEx(font, title, shadowPos, size, space, (Color){35, 25, 10, 255});

  float glowRadius = size * 0.05f * (1 + 0.3f * sinf(t * 2));
  for (int i = 0; i < 12; i++) {
    float angle = i * PI / 6;
    Vector2 glowPos = {pos.x + cosf(angle) * glowRadius, pos.y + sinf(angle) * glowRadius};
    DrawTextEx(font, title, glowPos, size, space, Fade(gold, 0.1f));
  }

  for (int i = 0; i < 8; i++) {
    int stripeY = (int)(pos.y + i * textSize.y / 8);
    int stripeH = (int)(textSize.y / 8) + 1;
    Color stripeColor = {255, (unsigned char)(245 - 14 * i), (unsigned char)(180 - 18 * i), 255};

    BeginScissorMode((int)(pos.x - 10), stripeY, (int)textSize.x + 20, stripeH);
    DrawTextEx(font, title, pos, size, space, stripeColor);
    EndScissorMode();
  }

  float bandWidth = size * 0.5f;
  float bandX = pos.x - bandWidth + fmodf(t * 0.4f, 1) * (textSize.x + bandWidth);

  BeginScissorMode((int)bandX, (int)pos.y, (int)bandWidth, (int)textSize.y);
  DrawTextEx(font, title, pos, size, space, Fade(WHITE, 0.8f));
  EndScissorMode();
}