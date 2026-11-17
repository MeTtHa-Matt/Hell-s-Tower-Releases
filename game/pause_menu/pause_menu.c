#include "pause_menu.h"
#include "../buttons/button_sounds.h"
#include "../ui_scale.h"
#include "raylib.h"

static bool DrawPauseButton(const char *label, Rectangle r, float alpha,
                            bool enabled, ButtonSoundId soundId) {
  float scale = GameUiScale();
  bool hover = CheckCollisionPointRec(GetMousePosition(), r);
  UpdateButtonHoverSound(soundId, hover && enabled);
  Color gold = {245, 220, 150, 255};
  Color bg = {18, 18, 24, 235};
  if (hover)
    bg = (Color){55, 45, 35, 245};

  DrawRectangleRec(r, Fade(bg, alpha));
  DrawRectangleLinesEx(r, 2 * scale, Fade(gold, alpha));

  int size = GameUiPixels(28);
  int textX = (int)(r.x + (r.width - MeasureText(label, size)) / 2);
  int textY = (int)(r.y + (r.height - size) / 2);
  DrawText(label, textX, textY, size, Fade(gold, alpha));

  bool clicked = enabled && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (clicked)
    PlayButtonClickSound();
  return clicked;
}

HomePauseResult DrawBlackScreenPauseMenu(float *shadeAlpha) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float scale = GameUiScale();

  *shadeAlpha += GetFrameTime() * 500;
  if (*shadeAlpha > 210)
    *shadeAlpha = 210;

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, (unsigned char)*shadeAlpha});

  float menuAlpha = (*shadeAlpha - 35) / 165;
  if (menuAlpha < 0)
    menuAlpha = 0;
  if (menuAlpha > 1)
    menuAlpha = 1;

  int titleSize = GameUiPixels(48);
  DrawText("PAUSE", (sw - MeasureText("PAUSE", titleSize)) / 2,
           (int)(sh / 2 - 150 * scale), titleSize,
           Fade(RAYWHITE, menuAlpha));

  float width = 340 * scale, height = 58 * scale;
  float x = (sw - width) / 2.0f;
  float y = sh / 2.0f - 100 * scale;
  bool enabled = *shadeAlpha >= 200;

  if (DrawPauseButton("Reprendre", (Rectangle){x, y, width, height}, menuAlpha,
                      enabled, BUTTON_SOUND_PAUSE_RESUME))
    return HOME_PAUSE_RESUME;
  if (DrawPauseButton("Revenir au menu",
                      (Rectangle){x, y + 74 * scale, width, height},
                      menuAlpha, enabled, BUTTON_SOUND_PAUSE_MENU))
    return HOME_PAUSE_RETURN_TO_MENU;
  if (DrawPauseButton("Quitter le jeu",
                      (Rectangle){x, y + 148 * scale, width, height},
                      menuAlpha, enabled, BUTTON_SOUND_PAUSE_QUIT))
    return HOME_PAUSE_QUIT;

  return HOME_PAUSE_NONE;
}

UnsavedDialogResult DrawUnsavedSessionDialog(void) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float scale = GameUiScale();

  float w = sw * 0.72f;
  if (w > 620 * scale)
    w = 620 * scale;
  float h = 220 * scale;
  if (h > sh * 0.86f)
    h = sh * 0.86f;
  float x = (sw - w) / 2;
  float y = (sh - h) / 2;

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 190});

  Rectangle box = {x, y, w, h};
  DrawRectangleRec(box, (Color){18, 18, 24, 250});
  DrawRectangleLinesEx(box, 2 * scale, (Color){245, 180, 90, 255});

  const char *title = "Session non sauvegardée";
  int titleSize = GameUiPixels(32);
  DrawText(title, (sw - MeasureText(title, titleSize)) / 2,
           (int)y + GameUiPixels(36), titleSize, RAYWHITE);

  float inset = 24 * scale;
  float bw = (w - 3 * inset) / 2;
  float bh = 52 * scale;
  float by = y + h - 78 * scale;

  if (DrawPauseButton("Continuer", (Rectangle){x + inset, by, bw, bh}, 1, true,
                      BUTTON_SOUND_CONFIRM_CONTINUE))
    return UNSAVED_DIALOG_CONTINUE;
  if (DrawPauseButton("Annuler",
                      (Rectangle){x + 2 * inset + bw, by, bw, bh}, 1, true,
                      BUTTON_SOUND_CONFIRM_CANCEL))
    return UNSAVED_DIALOG_CANCEL;

  return UNSAVED_DIALOG_NONE;
}