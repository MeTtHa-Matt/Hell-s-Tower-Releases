#include "loading_screen.h"
#include "raylib.h"
#include <stddef.h>

#define FADE_IN 0.45f
#define LOADING 5.65f
#define FADE_OUT 0.5f

static Texture2D defaultBg, customBg;
static bool active, actionDone;
static LoadingScreenAction action;
static float timer;

static float GetAlpha(void) {
  if (timer < FADE_IN)
    return timer / FADE_IN;
  if (timer < FADE_IN + LOADING)
    return 1;
  return 1 - (timer - FADE_IN - LOADING) / FADE_OUT;
}

static float GetProgress(void) {
  float t = timer - FADE_IN;
  if (t <= 0)
    return 0;
  if (t < 3.0f)
    return 0.75f * t / 3.0f;
  if (t < 5.0f)
    return 0.75f;
  if (t < LOADING)
    return 0.75f + 0.25f * (t - 5.0f) / 0.65f;
  return 1;
}

static void UnloadCustomBg(void) {
  if (customBg.id)
    UnloadTexture(customBg);
  customBg = (Texture2D){0};
}

void LoadingScreenInit(void) {
  defaultBg = LoadTexture("assets/loading_screen.png");
}

void LoadingScreenStart(LoadingScreenAction onComplete) {
  if (active)
    return;
  UnloadCustomBg();
  active = true;
  actionDone = false;
  action = onComplete;
  timer = 0;
}

void LoadingScreenStartWithBackground(LoadingScreenAction onComplete,
                                      const char *backgroundPath) {
  if (active)
    return;
  LoadingScreenStart(onComplete);
  customBg = LoadTexture(backgroundPath);
}

void LoadingScreenUpdate(void) {
  if (!active)
    return;

  timer += GetFrameTime();

  if (!actionDone && timer >= FADE_IN + LOADING) {
    actionDone = true;
    if (action)
      action();
  }

  if (timer >= FADE_IN + LOADING + FADE_OUT) {
    active = false;
    UnloadCustomBg();
  }
}

void LoadingScreenDraw(void) {
  if (!active)
    return;

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  unsigned char a = (unsigned char)(GetAlpha() * 255);

  Texture2D bg = customBg.id ? customBg : defaultBg;
  if (bg.id) {
    Rectangle source = {0, 0, (float)bg.width, (float)bg.height};
    Rectangle dest = {0, 0, (float)sw, (float)sh};
    DrawTexturePro(bg, source, dest, (Vector2){0, 0}, 0, (Color){255, 255, 255, a});
  } else {
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, a});
  }

  float w = sw * 0.42f;
  float h = sh * 0.025f;
  float x = (sw - w) / 2;
  float y = sh * 0.84f;

  Rectangle frame = {x - 3, y - 3, w + 6, h + 6};
  Color gold = {235, 220, 190, a};

  DrawRectangleRec(frame, (Color){10, 10, 14, a});
  DrawRectangleLinesEx(frame, 2, gold);
  DrawRectangle((int)x, (int)y, (int)(w * GetProgress()), (int)h, (Color){200, 165, 100, a});

  const char *text = "CHARGEMENT";
  DrawText(text, (sw - MeasureText(text, 20)) / 2, (int)(y - 32), 20, gold);
}

bool LoadingScreenActive(void) {
  return active;
}

void LoadingScreenUnload(void) {
  if (defaultBg.id)
    UnloadTexture(defaultBg);
  defaultBg = (Texture2D){0};
  UnloadCustomBg();
  active = false;
  action = NULL;
}