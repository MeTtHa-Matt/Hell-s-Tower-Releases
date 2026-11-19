#include "title.h"
#include <raylib.h>

void DrawMenuTitle(Texture2D title) {
  if (title.id == 0)
    return;
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float width = sw * 0.32f;
  float height = width * title.height / title.width;
  Rectangle source = {0, 0, (float)title.width, (float)title.height};
  Rectangle dest = {(sw - width) / 2, sh * 0.12f, width, height};
  DrawTexturePro(title, source, dest, (Vector2){0, 0}, 0, WHITE);
}