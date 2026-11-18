#include "loading_screen.h"
#include "../game/ui_scale.h"
#include "../menu/update_screen.h"
#include "../updater.h"
#include "raylib.h"
#include <stddef.h>

#define FADE_IN 0.2f
#define LOADING 1.8f
#define FADE_OUT 0.3f
#define UPDATE_GATE_PROGRESS 0.75f

static Texture2D defaultBg, customBg;
static bool active, actionDone;
static bool updateCheckActive;
static bool updateRequired;
static LoadingScreenAction action;
static float timer;

static float UpdateGateTime(void) {
  return FADE_IN + LOADING * UPDATE_GATE_PROGRESS;
}

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
  if (t >= LOADING)
    return 1;
  return t / LOADING;
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
  updateCheckActive = false;
  updateRequired = false;
  action = onComplete;
  timer = 0;
}

void LoadingScreenStartWithUpdateCheck(LoadingScreenAction onComplete) {
  if (active)
    return;
  LoadingScreenStart(onComplete);
  updateCheckActive = true;
  if (!UpdaterHasPendingUpdate())
    UpdaterStartCheck();
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

  if (updateRequired) {
    timer = UpdateGateTime();
    return;
  }

  timer += GetFrameTime();

  if (updateCheckActive && timer >= UpdateGateTime()) {
    timer = UpdateGateTime();
    if (UpdaterHasPendingUpdate()) {
      updateRequired = true;
      updateCheckActive = false;
      return;
    }
    if (!UpdaterCheckFinished())
      return;

    if (UpdaterUpdateAvailable()) {
      UpdaterPersistUpdate(UpdaterLatestVersion());
      updateRequired = UpdaterHasPendingUpdate();
      updateCheckActive = false;
      if (updateRequired)
        return;
    }
    updateCheckActive = false;
  }

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

bool LoadingScreenDraw(void) {
  if (!active)
    return false;

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  unsigned char a = (unsigned char)(GetAlpha() * 255);

  Texture2D bg = customBg.id ? customBg : defaultBg;
  if (bg.id) {
    float screenAspect = (float)sw / sh;
    float textureAspect = (float)bg.width / bg.height;
    Rectangle source = {0, 0, (float)bg.width, (float)bg.height};
    if (textureAspect > screenAspect) {
      source.width = bg.height * screenAspect;
      source.x = (bg.width - source.width) / 2;
    } else {
      source.height = bg.width / screenAspect;
      source.y = (bg.height - source.height) / 2;
    }
    Rectangle dest = {0, 0, (float)sw, (float)sh};
    DrawTexturePro(bg, source, dest, (Vector2){0, 0}, 0, (Color){255, 255, 255, a});
  } else {
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, a});
  }

  float w = sw * 0.42f;
  float h = sh * 0.025f;
  float scale = GameUiScale();
  float x = (sw - w) / 2;
  float y = sh * 0.84f;

  Rectangle frame = {x - 3 * scale, y - 3 * scale,
                     w + 6 * scale, h + 6 * scale};
  Color gold = {235, 220, 190, a};

  DrawRectangleRec(frame, (Color){10, 10, 14, a});
  DrawRectangleLinesEx(frame, 2 * scale, gold);
  DrawRectangle((int)x, (int)y, (int)(w * GetProgress()), (int)h, (Color){200, 165, 100, a});

  const char *text = "CHARGEMENT";
  int size = GameUiPixels(20);
  DrawText(text, (sw - MeasureText(text, size)) / 2,
           (int)(y - 32 * scale), size, gold);

  if (updateRequired)
    return UpdateRequiredScreenDraw(UpdaterPendingVersion()) ==
           UPDATE_SCREEN_QUIT;
  return false;
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
  updateCheckActive = false;
  updateRequired = false;
  action = NULL;
}