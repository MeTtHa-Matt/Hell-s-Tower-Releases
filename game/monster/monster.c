#include "monster.h"
#include "../buttons/button_sounds.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static Texture2D tex, hoverTex;
static char imagePathLoaded[512], hoverPathLoaded[512];

bool monster(const char *imagePath, const char *position, bool hoverEnabled) {
  bool dead = strcmp(imagePath, "assets/dead_sigrid.png") == 0;
  const char *hoverPath = dead
                              ? "assets/button_dead_sigrid.png"
                              : "assets/button_monster1.png";
  if (strcmp(imagePathLoaded, imagePath) != 0) {
    UnloadTexture(tex);
    tex = LoadTexture(imagePath);
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

  float bob = 0;
  if (!dead) {
    float t = fmodf((float)GetTime() * 10, 24);
    bob = 12 - fabsf(t - 12);
  }

  float x = (GetScreenWidth() - tex.width / 2.0f) / 2;
  float y = (GetScreenHeight() - tex.height / 2.0f) / 2 + (int)bob;

  Rectangle monster = {x, y, tex.width * 0.5f, tex.height * 0.5f};
  bool hovering = hoverEnabled && CheckCollisionPointRec(GetMousePosition(), monster);
  bool clicked = hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  UpdateButtonHoverSound(BUTTON_SOUND_SIGRID, hovering);
  if (clicked)
    PlayButtonClickSound();
  if (hovering && hoverTex.id == 0 && hoverPath[0])
    hoverTex = LoadTexture(hoverPath);
  if (hovering && hoverTex.id) {
    float scale = monster.height / hoverTex.height;
    float w = hoverTex.width * scale, h = hoverTex.height * scale;
    Rectangle source = {0, 0, (float)hoverTex.width, (float)hoverTex.height};
    Rectangle dest = {x + (monster.width - w) / 2, y + monster.height * 0.013f, w, h};
    DrawTexturePro(hoverTex, source, dest, (Vector2){0, 0}, 0, WHITE);
  } else {
    DrawTextureEx(tex, (Vector2){x, y}, 0, 0.5f, WHITE);
  }
  return clicked;
}

void UnloadMonster(void) {
  UnloadTexture(tex);
  UnloadTexture(hoverTex);
  tex = (Texture2D){0};
  hoverTex = (Texture2D){0};
  imagePathLoaded[0] = hoverPathLoaded[0] = '\0';
}