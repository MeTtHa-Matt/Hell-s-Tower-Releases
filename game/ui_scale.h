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

#endif
