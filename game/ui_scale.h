#ifndef GAME_UI_SCALE_H
#define GAME_UI_SCALE_H

#include "raylib.h"

static inline float GameUiScale(void) {
  float widthScale = GetScreenWidth() / 1920.0f;
  float heightScale = GetScreenHeight() / 1080.0f;
  float scale = widthScale < heightScale ? widthScale : heightScale;
  if (scale < 0.5f)
    scale = 0.5f;
  if (scale > 1.5f)
    scale = 1.5f;
  return scale;
}

static inline int GameUiPixels(float value) {
  return (int)(value * GameUiScale() + 0.5f);
}

static inline const int *GameUiFontCodepoints(int *count) {
  static int codepoints[240];
  static bool initialized;
  if (!initialized) {
    int index = 0;
    for (int codepoint = 32; codepoint <= 255; codepoint++)
      codepoints[index++] = codepoint;
    codepoints[index++] = 0x0152;
    codepoints[index++] = 0x0153;
    codepoints[index++] = 0x2018;
    codepoints[index++] = 0x2019;
    codepoints[index++] = 0x201C;
    codepoints[index++] = 0x201D;
    codepoints[index++] = 0x2013;
    codepoints[index++] = 0x2014;
    codepoints[index++] = 0x2026;
    codepoints[index++] = 0x00A7;
    codepoints[index++] = 0x00B7;
    initialized = true;
  }
  if (count)
    *count = 224 + 11;
  return codepoints;
}

#endif
