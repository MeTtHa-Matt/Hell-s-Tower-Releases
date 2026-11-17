#include "pause_menu.h"
#include "../buttons/button_sounds.h"
#include "raylib.h"

static bool DrawPauseButton(const char *label, Rectangle r, float alpha,
                            bool enabled, ButtonSoundId soundId) {
  bool hover = CheckCollisionPointRec(GetMousePosition(), r);
  UpdateButtonHoverSound(soundId, hover && enabled);
  Color gold = {245, 220, 150, 255};
  Color bg = {18, 18, 24, 235};
  if (hover)
    bg = (Color){55, 45, 35, 245};

  DrawRectangleRec(r, Fade(bg, alpha));
  DrawRectangleLinesEx(r, 2, Fade(gold, alpha));

  int textX = (int)(r.x + (r.width - MeasureText(label, 28)) / 2);
  int textY = (int)(r.y + (r.height - 28) / 2);
  DrawText(label, textX, textY, 28, Fade(gold, alpha));

  bool clicked = enabled && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (clicked)
    PlayButtonClickSound();
  return clicked;
}

HomePauseResult DrawBlackScreenPauseMenu(float *shadeAlpha) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();

  *shadeAlpha += GetFrameTime() * 500;
  if (*shadeAlpha > 210)
    *shadeAlpha = 210;

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, (unsigned char)*shadeAlpha});

  float menuAlpha = (*shadeAlpha - 35) / 165;
  if (menuAlpha < 0)
    menuAlpha = 0;
  if (menuAlpha > 1)
    menuAlpha = 1;

  DrawText("PAUSE", (sw - MeasureText("PAUSE", 48)) / 2, sh / 2 - 150, 48,
           Fade(RAYWHITE, menuAlpha));

  float x = (sw - 340) / 2.0f;
  float y = sh / 2.0f - 100;
  bool enabled = *shadeAlpha >= 200;

  if (DrawPauseButton("Reprendre", (Rectangle){x, y, 340, 58}, menuAlpha,
                      enabled, BUTTON_SOUND_PAUSE_RESUME))
    return HOME_PAUSE_RESUME;
  if (DrawPauseButton("Revenir au menu", (Rectangle){x, y + 74, 340, 58},
                      menuAlpha, enabled, BUTTON_SOUND_PAUSE_MENU))
    return HOME_PAUSE_RETURN_TO_MENU;
  if (DrawPauseButton("Quitter le jeu", (Rectangle){x, y + 148, 340, 58},
                      menuAlpha, enabled, BUTTON_SOUND_PAUSE_QUIT))
    return HOME_PAUSE_QUIT;

  return HOME_PAUSE_NONE;
}

UnsavedDialogResult DrawUnsavedSessionDialog(void) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();

  float w = sw * 0.72f;
  if (w > 620)
    w = 620;
  float h = 220;
  float x = (sw - w) / 2;
  float y = (sh - h) / 2;

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 190});

  Rectangle box = {x, y, w, h};
  DrawRectangleRec(box, (Color){18, 18, 24, 250});
  DrawRectangleLinesEx(box, 2, (Color){245, 180, 90, 255});

  const char *title = "Session non sauvegardée";
  DrawText(title, (sw - MeasureText(title, 32)) / 2, (int)y + 36, 32, RAYWHITE);

  float bw = (w - 72) / 2;
  float bh = 52;
  float by = y + h - 78;

  if (DrawPauseButton("Continuer", (Rectangle){x + 24, by, bw, bh}, 1, true,
                      BUTTON_SOUND_CONFIRM_CONTINUE))
    return UNSAVED_DIALOG_CONTINUE;
  if (DrawPauseButton("Annuler", (Rectangle){x + 48 + bw, by, bw, bh}, 1, true,
                      BUTTON_SOUND_CONFIRM_CANCEL))
    return UNSAVED_DIALOG_CANCEL;

  return UNSAVED_DIALOG_NONE;
}