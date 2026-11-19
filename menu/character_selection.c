#include "character_selection.h"
#include "../game/buttons/button_sounds.h"
#include "../game/ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHARACTER_COUNT 2

static const char *names[CHARACTER_COUNT] = {"Elyssia", "Kaelen"};
static const char *storyPaths[CHARACTER_COUNT] = {
    "assets/player1.txt", "assets/player2.txt"};
static Texture2D portraits[CHARACTER_COUNT];
static char *stories[CHARACTER_COUNT];
static Texture2D buttonTexture;
static Font font;
static bool fontLoaded;
static int selected, scrollFrom, scrollTo, scrollDirection;
static float entryProgress, scrollProgress = 1, detailProgress, confirmProgress;
static bool detailTarget, confirming, saveError;

static float Clamp(float value, float min, float max) {
  return value < min ? min : value > max ? max : value;
}

static void DrawTextCentered(const char *text, float centerX, float y,
                             float size, Color color) {
  float spacing = size * 0.05f;
  Vector2 textSize = MeasureTextEx(font, text, size, spacing);
  DrawTextEx(font, text, (Vector2){centerX - textSize.x / 2, y}, size, spacing,
             color);
}

static void DrawWrappedText(const char *text, Rectangle box, float size,
                            Color color) {
  char line[512] = "";
  float spacing = size * 0.06f;
  float lineHeight = size * 1.4f;
  float y = box.y;
  const char *word = text;

  while (*word && y + lineHeight <= box.y + box.height) {
    if (*word == '\n' || *word == '\r') {
      if (line[0]) {
        DrawTextEx(font, line, (Vector2){box.x, y}, size, spacing, color);
        line[0] = '\0';
        y += lineHeight;
      } else {
        y += lineHeight * 0.5f;
      }
      if (*word == '\r' && word[1] == '\n')
        word++;
      word++;
      continue;
    }
    if (*word == ' ' || *word == '\t') {
      word++;
      continue;
    }

    const char *end = word;
    while (*end && *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r')
      end++;
    size_t wordLength = (size_t)(end - word);
    char candidate[512];
    snprintf(candidate, sizeof(candidate), "%s%s%.*s", line, line[0] ? " " : "",
             (int)wordLength, word);
    if (line[0] && MeasureTextEx(font, candidate, size, spacing).x > box.width) {
      DrawTextEx(font, line, (Vector2){box.x, y}, size, spacing, color);
      line[0] = '\0';
      y += lineHeight;
      continue;
    }
    snprintf(line, sizeof(line), "%s", candidate);
    word = end;
  }
  if (line[0] && y + lineHeight <= box.y + box.height)
    DrawTextEx(font, line, (Vector2){box.x, y}, size, spacing, color);
}

static void DrawPortrait(int index, float centerX, float centerY, float maxWidth,
                         float maxHeight, float alpha) {
  Texture2D texture = portraits[index];
  if (texture.id == 0)
    return;

  float scale = fminf(maxWidth / texture.width, maxHeight / texture.height);
  float width = texture.width * scale;
  float height = texture.height * scale;
  Rectangle source = {0, 0, (float)texture.width, (float)texture.height};
  Rectangle dest = {centerX - width / 2, centerY - height / 2, width, height};
  Color tint = WHITE;
  tint.a = (unsigned char)(255 * Clamp(alpha, 0, 1));
  DrawTexturePro(texture, source, dest, (Vector2){0, 0}, 0, tint);
}

static bool DrawButton(const char *label, Rectangle box, ButtonSoundId soundId) {
  bool hover = CheckCollisionPointRec(GetMousePosition(), box);
  UpdateButtonHoverSound(soundId, hover);
  if (hover)
    box.x += 10 * GameUiScale();

  if (buttonTexture.id != 0) {
    Rectangle source = {0, 0, (float)buttonTexture.width,
                        (float)buttonTexture.height};
    DrawTexturePro(buttonTexture, source, box, (Vector2){0, 0}, 0, WHITE);
  }

  float size = box.height * 0.58f;
  float spacing = size * 0.05f;
  Vector2 textSize = MeasureTextEx(font, label, size, spacing);
  if (textSize.x > box.width * 0.84f) {
    size *= box.width * 0.84f / textSize.x;
    spacing = size * 0.05f;
    textSize = MeasureTextEx(font, label, size, spacing);
  }
  Vector2 position = {box.x + (box.width - textSize.x) / 2,
                      box.y + (box.height - size) / 2};
  DrawTextEx(font, label, position, size, spacing,
             hover ? (Color){255, 230, 160, 255} : RAYWHITE);

  bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (clicked)
    PlayButtonClickSound();
  return clicked;
}

static bool DrawArrow(bool right, Rectangle box, ButtonSoundId soundId) {
  bool hover = CheckCollisionPointRec(GetMousePosition(), box);
  UpdateButtonHoverSound(soundId, hover);

  Vector2 center = {box.x + box.width / 2, box.y + box.height / 2};
  Color color = hover ? (Color){255, 230, 160, 255}
                      : (Color){225, 185, 105, 255};
  DrawCircleV(center, box.width * 0.48f, (Color){12, 12, 18, 210});
  DrawCircleLines((int)center.x, (int)center.y, box.width * 0.48f, color);
  float half = box.width * 0.18f;
  if (right) {
    DrawTriangle((Vector2){center.x - half / 2, center.y - half},
                 (Vector2){center.x + half / 2, center.y},
                 (Vector2){center.x - half / 2, center.y + half}, color);
  } else {
    DrawTriangle((Vector2){center.x + half / 2, center.y - half},
                 (Vector2){center.x - half / 2, center.y},
                 (Vector2){center.x + half / 2, center.y + half}, color);
  }

  bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (clicked)
    PlayButtonClickSound();
  return clicked;
}

static void BeginScroll(int direction) {
  scrollFrom = selected;
  scrollTo = (selected + direction + CHARACTER_COUNT) % CHARACTER_COUNT;
  scrollDirection = direction;
  scrollProgress = 0;
}

void InitCharacterSelection(void) {
  buttonTexture = LoadTexture("assets/menu_button.png");
  if (buttonTexture.id == 0)
    TraceLog(LOG_ERROR, "Bouton du menu introuvable");
  else
    SetTextureFilter(buttonTexture, TEXTURE_FILTER_BILINEAR);

  for (int i = 0; i < CHARACTER_COUNT; i++) {
    portraits[i] = LoadTexture(TextFormat("assets/player%d.png", i + 1));
    if (portraits[i].id == 0)
      TraceLog(LOG_ERROR, "Portrait du personnage introuvable: %d", i + 1);
    else
      SetTextureFilter(portraits[i], TEXTURE_FILTER_BILINEAR);
    stories[i] = LoadFileText(storyPaths[i]);
    if (!stories[i])
      TraceLog(LOG_ERROR, "Histoire du personnage introuvable: %s", storyPaths[i]);
  }

  int codepoints[224];
  for (int i = 0; i < 224; i++)
    codepoints[i] = i + 32;
  font = LoadFontEx("assets/Cinzel.ttf", 64, codepoints, 224);
  fontLoaded = font.texture.id != 0;
  if (!fontLoaded) {
    TraceLog(LOG_ERROR, "Police Cinzel introuvable");
    font = GetFontDefault();
  } else {
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
  }
}

void StartCharacterSelection(void) {
  selected = scrollFrom = scrollTo = 0;
  scrollProgress = entryProgress = detailProgress = confirmProgress = 0;
  detailTarget = confirming = saveError = false;
}

int CharacterSelectionPlayer(void) { return selected + 1; }

void CharacterSelectionSaveFailed(void) {
  confirming = false;
  confirmProgress = 0;
  saveError = true;
}

void UnloadCharacterSelection(void) {
  for (int i = 0; i < CHARACTER_COUNT; i++) {
    if (portraits[i].id != 0)
      UnloadTexture(portraits[i]);
    if (stories[i])
      UnloadFileText(stories[i]);
    portraits[i] = (Texture2D){0};
    stories[i] = NULL;
  }
  if (buttonTexture.id != 0)
    UnloadTexture(buttonTexture);
  if (fontLoaded)
    UnloadFont(font);
  buttonTexture = (Texture2D){0};
  font = (Font){0};
  fontLoaded = false;
}

bool DrawCharacterSelection(void) {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  bool portraitLayout = sh > sw;
  float shortSide = fminf((float)sw, (float)sh);
  float dt = GetFrameTime();
  float centerX = sw / 2.0f;
  float centerY = sh * (portraitLayout ? 0.32f : 0.46f);
  float arrowSize = fmaxf(shortSide * 0.11f, 24 * GameUiScale());
  float arrowMargin = fmaxf(shortSide * 0.055f, sw * 0.03f);
  float sideOffset = sw * (portraitLayout ? 0.31f : 0.27f);
  float sideWidth = sw * (portraitLayout ? 0.22f : 0.15f);
  float sideHeight = sh * (portraitLayout ? 0.26f : 0.35f);
  Rectangle leftArrow = {arrowMargin, centerY - arrowSize / 2,
                         arrowSize, arrowSize};
  Rectangle rightArrow = {sw - arrowMargin - arrowSize,
                          centerY - arrowSize / 2, arrowSize, arrowSize};
  Rectangle chooseButton =
      {sw * (portraitLayout ? 0.25f : 0.38f),
       sh * (portraitLayout ? 0.84f : 0.81f),
       sw * (portraitLayout ? 0.5f : 0.24f), sh * 0.075f};
  Rectangle confirmButton =
      {sw * (portraitLayout ? 0.06f : 0.53f),
       sh * (portraitLayout ? 0.87f : 0.78f),
       sw * (portraitLayout ? 0.4f : 0.19f),
       sh * (portraitLayout ? 0.07f : 0.075f)};
  Rectangle cancelButton =
      {sw * (portraitLayout ? 0.54f : 0.75f),
       sh * (portraitLayout ? 0.87f : 0.78f),
       sw * (portraitLayout ? 0.4f : 0.19f),
       sh * (portraitLayout ? 0.07f : 0.075f)};

  entryProgress = Clamp(entryProgress + dt / 0.35f, 0, 1);
  if (scrollProgress < 1) {
    scrollProgress = Clamp(scrollProgress + dt / 0.34f, 0, 1);
    if (scrollProgress == 1)
      selected = scrollTo;
  }
  detailProgress = Clamp(detailProgress +
                             (detailTarget ? dt / 0.42f : -dt / 0.42f),
                         0, 1);
  if (confirming)
    confirmProgress = Clamp(confirmProgress + dt / 0.6f, 0, 1);

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 178});
  DrawTextCentered("CHOISISSEZ VOTRE PERSONNAGE", centerX, sh * 0.075f,
                   sh * 0.045f, (Color){245, 215, 145, 255});

  bool inDetails = detailProgress > 0;
  float portraitX = portraitLayout
                        ? centerX
                        : centerX + (sw * 0.25f - centerX) * detailProgress;
  float portraitWidth = portraitLayout
                            ? sw * (inDetails ? 0.42f : 0.43f)
                            : sw * (0.27f + 0.08f * detailProgress);
  float portraitHeight = portraitLayout
                             ? sh * (inDetails ? 0.3f : 0.38f)
                             : sh * (0.5f + 0.13f * detailProgress);
  float portraitY =
      portraitLayout ? sh * (0.32f - 0.05f * detailProgress) : centerY;

  if (!inDetails && scrollProgress < 1) {
    float progress = scrollProgress * scrollProgress * (3 - 2 * scrollProgress);
    DrawPortrait(scrollFrom, centerX - scrollDirection * sideOffset * progress,
                 centerY, portraitWidth + (sideWidth - portraitWidth) * progress,
                 portraitHeight + (sideHeight - portraitHeight) * progress,
                 1 - 0.35f * progress);
    DrawPortrait(scrollTo, centerX + scrollDirection * sideOffset * (1 - progress),
                 centerY, sideWidth + (portraitWidth - sideWidth) * progress,
                 sideHeight + (portraitHeight - sideHeight) * progress,
                 0.65f + 0.35f * progress);
  } else {
    int adjacent = (selected + 1) % CHARACTER_COUNT;
    float sideAlpha = 0.75f * (1 - detailProgress);
    DrawPortrait(adjacent, centerX - sideOffset,
                 centerY + sh * (portraitLayout ? 0.02f : 0.035f),
                 sideWidth, sideHeight, sideAlpha);
    DrawPortrait(adjacent, centerX + sideOffset,
                 centerY + sh * (portraitLayout ? 0.02f : 0.035f),
                 sideWidth, sideHeight, sideAlpha);
    DrawPortrait(selected, portraitX, portraitY, portraitWidth, portraitHeight, 1);
    if (!inDetails)
      DrawTextCentered(names[selected], centerX,
                       sh * (portraitLayout ? 0.46f : 0.735f),
                       sh * 0.04f, (Color){245, 215, 145, 255});
  }

  if (!inDetails) {
    if (scrollProgress >= 1) {
      int adjacent = (selected + 1) % CHARACTER_COUNT;
      float labelY = sh * (portraitLayout ? 0.43f : 0.69f);
      DrawTextCentered(names[adjacent], centerX - sideOffset, labelY,
                       sh * 0.027f, (Color){200, 185, 150, 210});
      DrawTextCentered(names[adjacent], centerX + sideOffset, labelY,
                       sh * 0.027f, (Color){200, 185, 150, 210});
    }

    bool leftClicked = DrawArrow(false, leftArrow, BUTTON_SOUND_TRAINING_CANCEL);
    bool rightClicked = DrawArrow(true, rightArrow, BUTTON_SOUND_TRAINING_START);
    if (detailProgress == 0 && scrollProgress == 1) {
      if (IsKeyPressed(KEY_LEFT))
        BeginScroll(-1);
      else if (IsKeyPressed(KEY_RIGHT))
        BeginScroll(1);
      else if (DrawButton("CHOISIR", chooseButton, BUTTON_SOUND_MENU_PLAY)) {
        detailTarget = true;
        saveError = false;
      } else if (leftClicked) {
        BeginScroll(-1);
      } else if (rightClicked) {
        BeginScroll(1);
      }
    } else {
      DrawButton("CHOISIR", chooseButton, BUTTON_SOUND_MENU_PLAY);
    }
  } else {
    float panelX = portraitLayout
                       ? sw * 0.08f
                       : sw * 0.52f + sw * 0.06f * (1 - detailProgress);
    float panelAlpha = detailProgress;
    Color textColor = (Color){245, 230, 190, (unsigned char)(255 * panelAlpha)};
    float nameSize = portraitLayout
                         ? fminf(sh * 0.055f, shortSide * 0.1f)
                         : sh * 0.055f;
    float nameY = sh * (portraitLayout ? 0.42f : 0.25f);
    DrawTextEx(font, names[selected], (Vector2){panelX, nameY}, nameSize,
               nameSize * 0.06f, textColor);

    Rectangle storyBox =
        portraitLayout
            ? (Rectangle){sw * 0.08f, sh * 0.49f, sw * 0.84f, sh * 0.35f}
            : (Rectangle){panelX, sh * 0.35f, sw * 0.4f, sh * 0.34f};
    Color storyColor = (Color){235, 230, 215, (unsigned char)(255 * panelAlpha)};
    if (stories[selected])
      DrawWrappedText(stories[selected], storyBox,
                      portraitLayout ? fminf(sh * 0.027f, shortSide * 0.04f)
                                     : sh * 0.027f,
                      storyColor);

    if (detailProgress == 1 && !confirming) {
      if (DrawButton("CONFIRMER", confirmButton, BUTTON_SOUND_CONFIRM_CONTINUE)) {
        confirming = true;
        confirmProgress = 0;
        saveError = false;
      } else if (DrawButton("ANNULER", cancelButton,
                            BUTTON_SOUND_CONFIRM_CANCEL)) {
        detailTarget = false;
        saveError = false;
      }
    } else {
      DrawButton("CONFIRMER", confirmButton, BUTTON_SOUND_CONFIRM_CONTINUE);
      DrawButton("ANNULER", cancelButton, BUTTON_SOUND_CONFIRM_CANCEL);
    }
  }

  if (saveError)
    DrawTextCentered("Échec de l'enregistrement. Réessayez.", centerX,
                     sh * 0.91f, sh * 0.025f, RED);

  if (confirming)
    DrawRectangle(0, 0, sw, sh,
                  (Color){0, 0, 0, (unsigned char)(255 * confirmProgress)});
  if (entryProgress < 1)
    DrawRectangle(0, 0, sw, sh,
                  (Color){0, 0, 0, (unsigned char)(255 * (1 - entryProgress))});

  return confirming && confirmProgress >= 1;
}
