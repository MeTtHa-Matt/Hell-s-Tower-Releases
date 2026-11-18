#include "update_screen.h"

#include "../game/ui_scale.h"
#include "raylib.h"
#include <stdio.h>

#define RELEASE_PAGE_URL \
  "https://github.com/MeTtHa-Matt/Hell-s-Tower-Releases/releases/latest"

#ifndef APP_VERSION
#define APP_VERSION "v0.0.0-dev"
#endif

static bool DrawUpdateButton(const char *label, Rectangle bounds) {
  bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);
  Color border = hover ? (Color){245, 220, 150, 255}
                       : (Color){200, 170, 110, 255};
  Color fill = hover ? (Color){55, 45, 28, 255} : (Color){20, 20, 24, 245};
  DrawRectangleRec(bounds, fill);
  DrawRectangleLinesEx(bounds, GameUiScale(), border);
  int fontSize = GameUiPixels(20);
  DrawText(label, (int)(bounds.x + (bounds.width - MeasureText(label, fontSize)) / 2),
           (int)(bounds.y + (bounds.height - fontSize) / 2), fontSize, border);
  return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

UpdateScreenAction UpdateRequiredScreenDraw(const char *latestVersion) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float scale = GameUiScale();
  float panelWidth = sw * 0.56f;
  float panelHeight = sh * 0.68f;
  Rectangle panel = {(sw - panelWidth) / 2, (sh - panelHeight) / 2,
                     panelWidth, panelHeight};

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 220});
  DrawRectangleRec(panel, (Color){14, 14, 19, 255});
  DrawRectangleLinesEx(panel, 2 * scale, (Color){200, 170, 110, 255});

  const char *title = "MISE À JOUR REQUISE";
  int titleSize = GameUiPixels(34);
  DrawText(title, (sw - MeasureText(title, titleSize)) / 2,
           (int)(panel.y + panel.height * 0.14f), titleSize,
           (Color){245, 220, 150, 255});

  char currentText[96];
  char latestText[96];
  snprintf(currentText, sizeof(currentText), "Version actuelle : %s", APP_VERSION);
  snprintf(latestText, sizeof(latestText), "Nouvelle version : %s",
           latestVersion ? latestVersion : "inconnue");
  int textSize = GameUiPixels(22);
  DrawText(currentText, (sw - MeasureText(currentText, textSize)) / 2,
           (int)(panel.y + panel.height * 0.34f), textSize, RAYWHITE);
  DrawText(latestText, (sw - MeasureText(latestText, textSize)) / 2,
           (int)(panel.y + panel.height * 0.43f), textSize, RAYWHITE);

  float buttonWidth = panelWidth * 0.72f;
  float buttonHeight = sh * 0.075f;
  float buttonX = (sw - buttonWidth) / 2;
  float gap = buttonHeight * 0.28f;
  float firstY = panel.y + panel.height * 0.62f;
  if (DrawUpdateButton("Mettre à jour",
                       (Rectangle){buttonX, firstY, buttonWidth, buttonHeight})) {
    OpenURL(RELEASE_PAGE_URL);
  }
  if (DrawUpdateButton("Quitter",
                       (Rectangle){buttonX, firstY + buttonHeight + gap,
                                   buttonWidth, buttonHeight}))
    return UPDATE_SCREEN_QUIT;
  return UPDATE_SCREEN_NONE;
}
