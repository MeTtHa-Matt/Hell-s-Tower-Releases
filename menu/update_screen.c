#include "update_screen.h"

#include "../game/ui_scale.h"
#include "raylib.h"
#include <stdio.h>

#define RELEASE_PAGE_URL \
  "https://github.com/MeTtHa-Matt/Hell-s-Tower-Releases/releases/latest"

#ifndef APP_VERSION
#define APP_VERSION "v0.0.0-dev"
#endif

static bool DrawUpdateButton(const char *label, Rectangle bounds, bool primary) {
  bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);
  Color fill = primary
                   ? (hover ? (Color){250, 221, 155, 255}
                            : (Color){220, 182, 104, 255})
                   : (hover ? (Color){58, 60, 68, 255}
                            : (Color){35, 37, 44, 255});
  Color text = primary ? (Color){30, 27, 24, 255} : RAYWHITE;
  DrawRectangleRounded(bounds, 0.16f, 8, fill);
  if (!primary)
    DrawRectangleRoundedLinesEx(bounds, 0.16f, 8, GameUiScale(),
                                (Color){90, 92, 102, 255});
  int fontSize = GameUiPixels(19);
  DrawText(label, (int)(bounds.x + (bounds.width - MeasureText(label, fontSize)) / 2),
           (int)(bounds.y + (bounds.height - fontSize) / 2), fontSize, text);
  return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

UpdateScreenAction UpdateRequiredScreenDraw(const char *latestVersion) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float scale = GameUiScale();
  float panelWidth = sw * 0.86f;
  if (panelWidth > 900 * scale)
    panelWidth = 900 * scale;
  float panelHeight = sh * 0.82f;
  if (panelHeight > 500 * scale)
    panelHeight = 500 * scale;
  Rectangle panel = {(sw - panelWidth) / 2, (sh - panelHeight) / 2,
                     panelWidth, panelHeight};

  DrawRectangle(0, 0, sw, sh, (Color){5, 6, 10, 205});
  DrawRectangleRounded(panel, 0.06f, 12, (Color){18, 19, 25, 255});
  DrawRectangleRoundedLinesEx(panel, 0.06f, 12, 2 * scale,
                              (Color){104, 88, 61, 255});

  Rectangle accent = {panel.x + 2 * scale, panel.y + 2 * scale,
                      panel.width - 4 * scale, 5 * scale};
  DrawRectangleRounded(accent, 0.8f, 8, (Color){220, 182, 104, 255});

  float contentX = panel.x + panel.width * 0.09f;
  float contentWidth = panel.width * 0.82f;
  float badgeSize = GameUiPixels(17);
  const char *badge = "MISE À JOUR OBLIGATOIRE";
  DrawText(badge, (int)contentX,
           (int)(panel.y + panel.height * 0.14f), badgeSize,
           (Color){225, 190, 122, 255});

  const char *title = "Une nouvelle version vous attend";
  int titleSize = GameUiPixels(32);
  DrawText(title, (int)contentX, (int)(panel.y + panel.height * 0.25f),
           titleSize, RAYWHITE);
  const char *subtitle = "Mettez le jeu à jour pour continuer votre aventure.";
  int subtitleSize = GameUiPixels(18);
  DrawText(subtitle, (int)contentX,
           (int)(panel.y + panel.height * 0.39f), subtitleSize,
           (Color){174, 177, 187, 255});

  float infoY = panel.y + panel.height * 0.51f;
  float infoHeight = panel.height * 0.19f;
  float gap = contentWidth * 0.035f;
  float infoWidth = (contentWidth - gap) / 2;
  Rectangle currentBox = {contentX, infoY, infoWidth, infoHeight};
  Rectangle latestBox = {contentX + infoWidth + gap, infoY,
                         infoWidth, infoHeight};
  DrawRectangleRounded(currentBox, 0.12f, 8, (Color){27, 29, 36, 255});
  DrawRectangleRounded(latestBox, 0.12f, 8, (Color){36, 34, 27, 255});

  int labelSize = GameUiPixels(14);
  int versionSize = GameUiPixels(22);
  const char *currentLabel = "VERSION INSTALLÉE";
  const char *latestLabel = "VERSION DISPONIBLE";
  char currentVersion[64];
  snprintf(currentVersion, sizeof(currentVersion), "%s", APP_VERSION);
  const char *releaseVersion = latestVersion ? latestVersion : "inconnue";
  DrawText(currentLabel, (int)(currentBox.x + infoWidth * 0.08f),
           (int)(currentBox.y + infoHeight * 0.22f), labelSize,
           (Color){147, 151, 162, 255});
  DrawText(currentVersion,
           (int)(currentBox.x + infoWidth * 0.08f),
           (int)(currentBox.y + infoHeight * 0.52f), versionSize, RAYWHITE);
  DrawText(latestLabel, (int)(latestBox.x + infoWidth * 0.08f),
           (int)(latestBox.y + infoHeight * 0.22f), labelSize,
           (Color){190, 159, 104, 255});
  DrawText(releaseVersion,
           (int)(latestBox.x + infoWidth * 0.08f),
           (int)(latestBox.y + infoHeight * 0.52f), versionSize,
           (Color){245, 215, 153, 255});

  float buttonY = panel.y + panel.height * 0.78f;
  float buttonHeight = panel.height * 0.14f;
  float buttonGap = contentWidth * 0.035f;
  float buttonWidth = (contentWidth - buttonGap) / 2;
  Rectangle updateButton = {contentX, buttonY, buttonWidth, buttonHeight};
  Rectangle quitButton = {contentX + buttonWidth + buttonGap, buttonY,
                          buttonWidth, buttonHeight};
  if (DrawUpdateButton("Télécharger la mise à jour", updateButton, true)) {
    OpenURL(RELEASE_PAGE_URL);
  }
  if (DrawUpdateButton("Quitter le jeu", quitButton, false))
    return UPDATE_SCREEN_QUIT;
  return UPDATE_SCREEN_NONE;
}
