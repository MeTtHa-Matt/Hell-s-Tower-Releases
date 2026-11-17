#include "monster.h"
#include "../buttons/button_sounds.h"
#include "../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static Texture2D tex, hoverTex;
static Color *texColors;
static int colorWidth, colorHeight;
static char imagePathLoaded[512], hoverPathLoaded[512];

static bool ContainsOpaquePixel(Vector2 point, Rectangle bounds) {
  if (!texColors || !CheckCollisionPointRec(point, bounds))
    return false;

  int x = (int)((point.x - bounds.x) * colorWidth / bounds.width);
  int y = (int)((point.y - bounds.y) * colorHeight / bounds.height);
  if (x < 0 || x >= colorWidth || y < 0 || y >= colorHeight)
    return false;
  return texColors[y * colorWidth + x].a > 16;
}

bool monster(const char *imagePath, const char *position, bool hoverEnabled) {
  bool dead = strcmp(imagePath, "assets/dead_sigrid.png") == 0;
  const char *hoverPath = dead
                              ? "assets/button_dead_sigrid.png"
                              : "assets/button_monster1.png";
  if (strcmp(imagePathLoaded, imagePath) != 0) {
    UnloadTexture(tex);
    if (texColors)
      UnloadImageColors(texColors);
    texColors = NULL;
    colorWidth = colorHeight = 0;
    tex = LoadTexture(imagePath);
    Image image = LoadImage(imagePath);
    if (image.data) {
      texColors = LoadImageColors(image);
      colorWidth = image.width;
      colorHeight = image.height;
      UnloadImage(image);
    }
    snprintf(imagePathLoaded, sizeof(imagePathLoaded), "%s", imagePath);
  }
  if (strcmp(hoverPathLoaded, hoverPath) != 0) {
    UnloadTexture(hoverTex);
    hoverTex = (Texture2D){0};
    snprintf(hoverPathLoaded, sizeof(hoverPathLoaded), "%s", hoverPath);
  }
  if (tex.id == 0)
    return false;
  if (strcmp(position, "middle") != 0)
    return false;

  float scale = 0.5f * GameUiScale();
  float maxWidth = (GetScreenWidth() - 2 * GameUiPixels(24)) * 0.9f / tex.width;
  float maxHeight =
      (GetScreenHeight() - 2 * GameUiPixels(24)) * 0.9f / tex.height;
  if (scale > maxWidth)
    scale = maxWidth;
  if (scale > maxHeight)
    scale = maxHeight;
  float bob = 0;
  if (!dead) {
    float t = fmodf((float)GetTime() * 10, 24);
    bob = (12 - fabsf(t - 12)) * GameUiScale();
  }

  float width = tex.width * scale;
  float height = tex.height * scale;
  float x = (GetScreenWidth() - width) / 2;
  float y = (GetScreenHeight() - height) / 2 + bob;

  Rectangle monster = {x, y, width, height};
  bool hovering = hoverEnabled &&
                  ContainsOpaquePixel(GetMousePosition(), monster);
  bool clicked = hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  UpdateButtonHoverSound(BUTTON_SOUND_SIGRID, hovering);
  if (clicked)
    PlayButtonClickSound();
  if (hovering && hoverTex.id == 0 && hoverPath[0])
    hoverTex = LoadTexture(hoverPath);
  if (hovering && hoverTex.id) {
    float overlayScale = monster.height / hoverTex.height;
    float w = hoverTex.width * overlayScale;
    float h = hoverTex.height * overlayScale;
    Rectangle source = {0, 0, (float)hoverTex.width, (float)hoverTex.height};
    Rectangle dest = {x + (monster.width - w) / 2,
                      y + monster.height * 0.013f, w, h};
    DrawTexturePro(hoverTex, source, dest, (Vector2){0, 0}, 0, WHITE);
  } else {
    DrawTextureEx(tex, (Vector2){x, y}, 0, scale, WHITE);
  }
  return clicked;
}

void UnloadMonster(void) {
  UnloadTexture(tex);
  UnloadTexture(hoverTex);
  if (texColors)
    UnloadImageColors(texColors);
  texColors = NULL;
  colorWidth = colorHeight = 0;
  tex = (Texture2D){0};
  hoverTex = (Texture2D){0};
  imagePathLoaded[0] = hoverPathLoaded[0] = '\0';
}