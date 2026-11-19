#include "loading_screen.h"
#include "../game/ui_scale.h"
#include "../menu/update_screen.h"
#include "../updater.h"
#include "raylib.h"
#include <stddef.h>

#define SCENE_FADE_OUT 0.2f
#define LOADING_FADE_IN 0.2f
#define LOADING 1.8f
#define LOADING_FADE_OUT 0.3f
#define SCENE_FADE_IN 0.3f
#define UPDATE_GATE_PROGRESS 0.75f
#define LOADING_START (SCENE_FADE_OUT + LOADING_FADE_IN)
#define LOADING_END (LOADING_START + LOADING)
#define LOADING_FINISH (LOADING_END + LOADING_FADE_OUT + SCENE_FADE_IN)

static Texture2D defaultBg, customBg;
static bool active, actionDone;
static bool showProgress = true;
static bool updateCheckActive;
static bool updateRequired;
static LoadingScreenAction action;
static float timer;

static float UpdateGateTime(void) {
  return LOADING_START + LOADING * UPDATE_GATE_PROGRESS;
}

static float GetAlpha(void) {
  if (timer < LOADING_START)
    return (timer - SCENE_FADE_OUT) / LOADING_FADE_IN;
  if (timer < LOADING_END)
    return 1;
  if (timer < LOADING_END + LOADING_FADE_OUT)
    return 1 - (timer - LOADING_END) / LOADING_FADE_OUT;
  return 0;
}

static float GetProgress(void) {
  float t = timer - LOADING_START;
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
  showProgress = true;
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

void LoadingScreenStartWithBackgroundNoProgress(
    LoadingScreenAction onComplete, const char *backgroundPath) {
  if (active)
    return;
  LoadingScreenStart(onComplete);
  showProgress = false;
  customBg = LoadTexture(backgroundPath);
  if (customBg.id == 0)
    TraceLog(LOG_ERROR, "Image d'ecran de chargement introuvable: %s",
             backgroundPath);
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

  if (!actionDone && timer >= LOADING_END) {
    actionDone = true;
    if (action)
      action();
  }

  if (timer >= LOADING_FINISH) {
    active = false;
    UnloadCustomBg();
  }
}

bool LoadingScreenDraw(void) {
  if (!active)
    return false;

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  if (timer < SCENE_FADE_OUT) {
    DrawRectangle(0, 0, sw, sh,
                  (Color){0, 0, 0,
                          (unsigned char)(255 * timer / SCENE_FADE_OUT)});
    return false;
  }
  if (timer >= LOADING_END + LOADING_FADE_OUT) {
    float alpha =
        1 - (timer - LOADING_END - LOADING_FADE_OUT) / SCENE_FADE_IN;
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, (unsigned char)(255 * alpha)});
    return false;
  }

  DrawRectangle(0, 0, sw, sh, BLACK);
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

  if (!showProgress)
    return false;

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

bool LoadingScreenUpdatePopupActive(void) {
  return active && updateRequired;
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