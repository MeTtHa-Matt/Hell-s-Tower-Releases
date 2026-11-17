#include "buttons.h"
#include "../../game/ui_scale.h"
#include "../../game/buttons/button_sounds.h"
#include "../../loading_screen/loading_screen.h"
#include <raylib.h>
#include <string.h>

static bool startPlayingRequested;

static bool DrawHomeButton(const char *label, Rectangle r, ButtonSoundId soundId) {
  float scale = GameUiScale();
  bool hover = CheckCollisionPointRec(GetMousePosition(), r);
  bool enabled = !LoadingScreenActive();
  UpdateButtonHoverSound(soundId, hover && enabled);
  if (hover)
    r.x += 10 * scale;

  Color gold = {200, 170, 110, 255};
  Color bg = {15, 15, 18, 220};
  Color textColor = RAYWHITE;
  if (hover) {
    gold = (Color){245, 220, 150, 255};
    bg = (Color){45, 38, 25, 240};
    textColor = gold;
  }

  DrawRectangleRec(r, bg);
  DrawRectangleLinesEx(r, scale, gold);
  if (hover)
    DrawRectangle((int)r.x + GameUiPixels(12),
                   (int)(r.y + r.height / 2 - GameUiPixels(4)),
                   GameUiPixels(8), GameUiPixels(8), gold);

  int size = (int)r.height * 2 / 3;
  int textX = (int)(r.x + (r.width - MeasureText(label, size)) / 2);
  int textY = (int)(r.y + (r.height - size) / 2);
  DrawText(label, textX, textY, size, textColor);

  bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (clicked && enabled)
    PlayButtonClickSound();
  return clicked && enabled;
}

void StartPlayingButton(void) {
  startPlayingRequested = true;
}

void OptionsButton(void) {
}

void QuitButton(void) {
  game_running = false;
}

bool StartPlayingRequested(void) {
  return startPlayingRequested;
}

void ClearStartPlayingRequest(void) {
  startPlayingRequested = false;
}

void Button(const char *name) {
  int i = -1;
  if (strcmp(name, "Jouer") == 0)
    i = 0;
  else if (strcmp(name, "Options") == 0)
    i = 1;
  else if (strcmp(name, "Quitter") == 0)
    i = 2;
  if (i < 0)
    return;

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  int w = (int)(sw * 0.22f);
  int h = (int)(sh * 0.055f);
  int gap = (int)(h * 0.35f);
  int top = (int)(sh * 0.68f) - (h * 3 + gap * 2) / 2;

  Rectangle r = {(sw - w) / 2.0f, (float)(top + i * (h + gap)), (float)w, (float)h};

  static const ButtonSoundId soundIds[] = {
      BUTTON_SOUND_MENU_PLAY,
      BUTTON_SOUND_MENU_OPTIONS,
      BUTTON_SOUND_MENU_QUIT,
  };
  if (DrawHomeButton(name, r, soundIds[i])) {
    if (i == 0)
      StartPlayingButton();
    else if (i == 1)
      OptionsButton();
    else
      QuitButton();
  }
}