#include "buttons.h"
#include "../../game/ui_scale.h"
#include "../../game/buttons/button_sounds.h"
#include "../../loading_screen/loading_screen.h"
#include <raylib.h>
#include <string.h>

static bool startPlayingRequested;
static bool successesRequested;
static Texture2D homeButtonTexture;
static Font homeButtonFont;
static bool homeButtonFontLoaded;

void InitHomeButtons(void) {
  homeButtonTexture = LoadTexture("assets/menu_button.png");
  if (homeButtonTexture.id == 0)
    TraceLog(LOG_ERROR, "Bouton du menu introuvable");
  else
    SetTextureFilter(homeButtonTexture, TEXTURE_FILTER_BILINEAR);

  homeButtonFont = LoadFontEx("assets/Cinzel.ttf", 64, NULL, 0);
  homeButtonFontLoaded = homeButtonFont.texture.id != 0;
  if (!homeButtonFontLoaded) {
    TraceLog(LOG_ERROR, "Police Cinzel introuvable");
    homeButtonFont = GetFontDefault();
  } else {
    SetTextureFilter(homeButtonFont.texture, TEXTURE_FILTER_BILINEAR);
  }
}

void UnloadHomeButtons(void) {
  if (homeButtonTexture.id != 0)
    UnloadTexture(homeButtonTexture);
  if (homeButtonFontLoaded)
    UnloadFont(homeButtonFont);
  homeButtonTexture = (Texture2D){0};
  homeButtonFont = (Font){0};
  homeButtonFontLoaded = false;
}

static bool DrawHomeButton(const char *label, Rectangle r, ButtonSoundId soundId) {
  float scale = GameUiScale();
  bool hover = CheckCollisionPointRec(GetMousePosition(), r);
  bool enabled = !LoadingScreenActive();
  UpdateButtonHoverSound(soundId, hover && enabled);
  if (hover)
    r.x += 10 * scale;

  Color gold = {200, 170, 110, 255};
  Color textColor = RAYWHITE;
  if (hover) {
    gold = (Color){245, 220, 150, 255};
    textColor = gold;
  }

  if (homeButtonTexture.id != 0) {
    Rectangle source = {0, 0, (float)homeButtonTexture.width,
                        (float)homeButtonTexture.height};
    DrawTexturePro(homeButtonTexture, source, r, (Vector2){0, 0}, 0, WHITE);
  }

  float size = r.height * 0.68f;
  float spacing = size * 0.06f;
  Vector2 textSize = MeasureTextEx(homeButtonFont, label, size, spacing);
  Vector2 textPosition = {r.x + (r.width - textSize.x) / 2,
                          r.y + (r.height - size) / 2};
  DrawTextEx(homeButtonFont, label, textPosition, size, spacing, textColor);

  bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (clicked && enabled)
    PlayButtonClickSound();
  return clicked && enabled;
}

void StartPlayingButton(void) {
  startPlayingRequested = true;
}

void SuccessesButton(void) { successesRequested = true; }

void OptionsButton(void) {
}

void QuitButton(void) {
  game_running = false;
}

bool StartPlayingRequested(void) {
  return startPlayingRequested;
}

bool TakeSuccessesRequest(void) {
  bool requested = successesRequested;
  successesRequested = false;
  return requested;
}

void ClearStartPlayingRequest(void) {
  startPlayingRequested = false;
}

void Button(const char *name) {
  int i = -1;
  if (strcmp(name, "Jouer") == 0)
    i = 0;
  else if (strcmp(name, "Succes") == 0)
    i = 1;
  else if (strcmp(name, "Options") == 0)
    i = 2;
  else if (strcmp(name, "Quitter") == 0)
    i = 3;
  if (i < 0)
    return;

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  int w = (int)(sw * 0.22f);
  int h = (int)(sh * 0.055f);
  int gap = (int)(h * 0.35f);
  int top = (int)(sh * 0.68f) - (h * 4 + gap * 3) / 2;

  Rectangle r = {(sw - w) / 2.0f, (float)(top + i * (h + gap)), (float)w, (float)h};

  static const ButtonSoundId soundIds[] = {
      BUTTON_SOUND_MENU_PLAY,
      BUTTON_SOUND_MENU_OPTIONS,
      BUTTON_SOUND_MENU_OPTIONS,
      BUTTON_SOUND_MENU_QUIT,
  };
  if (DrawHomeButton(name, r, soundIds[i])) {
    if (i == 0)
      StartPlayingButton();
    else if (i == 1)
      SuccessesButton();
    else if (i == 2)
      OptionsButton();
    else
      QuitButton();
  }
}