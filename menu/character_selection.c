#include "character_selection.h"
#include "../game/buttons/button_sounds.h"
#include "../game/characters/characters.h"
#include "../game/ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHARACTER_COUNT 6

static const char *names[CHARACTER_COUNT] = {
    "Melina", "Elyssia", "Kaelen", "Gorran", "Ysolde", "Thaddeus"};
static const char *storyPaths[CHARACTER_COUNT] = {
    "assets/player0.txt", "assets/player1.txt", "assets/player2.txt",
    "assets/player3.txt", "assets/player4.txt", "assets/player5.txt"};
static Texture2D portraits[CHARACTER_COUNT];
static char *stories[CHARACTER_COUNT];
static Texture2D buttonTexture;
static Texture2D leftArrowTexture, rightArrowTexture;
static Font font, storyFont;
static bool fontLoaded, storyFontLoaded;
static int selected, scrollFrom, scrollTo, scrollDirection;
static Vector2 dragStart;
static float entryProgress, scrollProgress = 1, detailProgress, confirmProgress;
static float storyScroll[CHARACTER_COUNT], storyMaxScroll[CHARACTER_COUNT];
static bool detailTarget, confirming, saveError, dragging, dragMoved;

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

static float DrawWrappedText(const char *text, Rectangle box, float size,
                             Color color, float scroll) {
  char line[512] = "";
  float spacing = size * 0.03f;
  float lineHeight = size * 1.35f;
  float y = box.y - scroll;
  const char *word = text;

  BeginScissorMode((int)box.x, (int)box.y, (int)box.width, (int)box.height);
  while (*word) {
    if (*word == '\n' || *word == '\r') {
      if (line[0]) {
        if (y + lineHeight > box.y && y < box.y + box.height)
          DrawTextEx(storyFont, line, (Vector2){box.x, y}, size, spacing, color);
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
    if (line[0] &&
        MeasureTextEx(storyFont, candidate, size, spacing).x > box.width) {
      if (y + lineHeight > box.y && y < box.y + box.height)
        DrawTextEx(storyFont, line, (Vector2){box.x, y}, size, spacing, color);
      line[0] = '\0';
      y += lineHeight;
      continue;
    }
    snprintf(line, sizeof(line), "%s", candidate);
    word = end;
  }
  if (line[0] && y + lineHeight > box.y && y < box.y + box.height)
    DrawTextEx(storyFont, line, (Vector2){box.x, y}, size, spacing, color);
  EndScissorMode();
  return fmaxf(0, y + lineHeight - (box.y - scroll));
}

static Rectangle PortraitBounds(int index, float centerX, float centerY,
                                float maxWidth, float maxHeight) {
  Texture2D texture = portraits[index];
  if (texture.id == 0)
    return (Rectangle){0};

  float scale = fminf(maxWidth / texture.width, maxHeight / texture.height);
  float width = texture.width * scale;
  float height = texture.height * scale;
  return (Rectangle){centerX - width / 2, centerY - height / 2, width, height};
}

static void DrawPortrait(int index, float centerX, float centerY, float maxWidth,
                         float maxHeight, float alpha) {
  Texture2D texture = portraits[index];
  if (texture.id == 0)
    return;

  Rectangle source = {0, 0, (float)texture.width, (float)texture.height};
  Rectangle dest =
      PortraitBounds(index, centerX, centerY, maxWidth, maxHeight);
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

static bool DrawArrow(Texture2D texture, Rectangle box, ButtonSoundId soundId) {
  bool hover = CheckCollisionPointRec(GetMousePosition(), box);
  UpdateButtonHoverSound(soundId, hover);

  if (texture.id != 0) {
    float width = box.height * texture.width / texture.height;
    Rectangle source = {0, 0, (float)texture.width, (float)texture.height};
    Rectangle dest = {box.x + (box.width - width) / 2, box.y, width, box.height};
    DrawTexturePro(texture, source, dest, (Vector2){0, 0}, 0,
                   hover ? (Color){255, 245, 210, 255} : WHITE);
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

static void UpdateCarouselMouse(bool enabled, Rectangle area, Rectangle leftArrow,
                                Rectangle rightArrow, Rectangle leftPortrait,
                                Rectangle rightPortrait, float threshold) {
  Vector2 mouse = GetMousePosition();
  bool released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  if (!dragging && enabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
      CheckCollisionPointRec(mouse, area) &&
      !CheckCollisionPointRec(mouse, leftArrow) &&
      !CheckCollisionPointRec(mouse, rightArrow)) {
    dragging = true;
    dragMoved = false;
    dragStart = mouse;
  }

  if (dragging && (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || released)) {
    float delta = mouse.x - dragStart.x;
    if (fabsf(delta) >= threshold) {
      dragMoved = true;
      if (enabled) {
        BeginScroll(delta < 0 ? 1 : -1);
        dragStart.x = mouse.x;
      }
    }
  }

  if (!released || !dragging)
    return;
  if (enabled && !dragMoved) {
    if (CheckCollisionPointRec(dragStart, leftPortrait))
      BeginScroll(-1);
    else if (CheckCollisionPointRec(dragStart, rightPortrait))
      BeginScroll(1);
  }
  dragging = false;
}

void InitCharacterSelection(void) {
  buttonTexture = LoadTexture("assets/menu_button.png");
  if (buttonTexture.id == 0)
    TraceLog(LOG_ERROR, "Bouton du menu introuvable");
  else
    SetTextureFilter(buttonTexture, TEXTURE_FILTER_BILINEAR);

  leftArrowTexture = LoadTexture("assets/arrow_left.png");
  rightArrowTexture = LoadTexture("assets/arrow_right.png");
  if (leftArrowTexture.id == 0)
    TraceLog(LOG_ERROR, "Flèche gauche introuvable");
  else
    SetTextureFilter(leftArrowTexture, TEXTURE_FILTER_BILINEAR);
  if (rightArrowTexture.id == 0)
    TraceLog(LOG_ERROR, "Flèche droite introuvable");
  else
    SetTextureFilter(rightArrowTexture, TEXTURE_FILTER_BILINEAR);

  for (int i = 0; i < CHARACTER_COUNT; i++) {
    portraits[i] = LoadTexture(TextFormat("assets/player%d.png", i));
    if (portraits[i].id == 0)
      TraceLog(LOG_ERROR, "Portrait du personnage introuvable: %d", i);
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
  storyFont = LoadFontEx("assets/NotoSerif-Light.ttf", 64, codepoints, 224);
  storyFontLoaded = storyFont.texture.id != 0;
  if (!storyFontLoaded) {
    TraceLog(LOG_ERROR, "Police des histoires introuvable");
    storyFont = GetFontDefault();
  } else {
    SetTextureFilter(storyFont.texture, TEXTURE_FILTER_BILINEAR);
  }
}

void StartCharacterSelection(void) {
  selected = scrollFrom = scrollTo = 0;
  scrollProgress = entryProgress = detailProgress = confirmProgress = 0;
  detailTarget = confirming = saveError = false;
  dragging = dragMoved = false;
  memset(storyScroll, 0, sizeof(storyScroll));
  memset(storyMaxScroll, 0, sizeof(storyMaxScroll));
}

int CharacterSelectionPlayer(void) { return selected; }

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
  if (leftArrowTexture.id != 0)
    UnloadTexture(leftArrowTexture);
  if (rightArrowTexture.id != 0)
    UnloadTexture(rightArrowTexture);
  if (fontLoaded)
    UnloadFont(font);
  if (storyFontLoaded)
    UnloadFont(storyFont);
  buttonTexture = (Texture2D){0};
  leftArrowTexture = rightArrowTexture = (Texture2D){0};
  font = storyFont = (Font){0};
  fontLoaded = false;
  storyFontLoaded = false;
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
  float arrowWidth = arrowSize * 0.65f;
  float sideOffset = sw * (portraitLayout ? 0.31f : 0.27f);
  float sideWidth = sw * (portraitLayout ? 0.22f : 0.15f);
  float sideHeight = sh * (portraitLayout ? 0.26f : 0.35f);
  Rectangle leftArrow = {arrowMargin, centerY - arrowSize / 2,
                         arrowWidth, arrowSize};
  Rectangle rightArrow = {sw - arrowMargin - arrowWidth,
                          centerY - arrowSize / 2, arrowWidth, arrowSize};
  Rectangle chooseButton =
      {sw * (portraitLayout ? 0.25f : 0.38f),
       sh * (portraitLayout ? 0.84f : 0.81f),
       sw * (portraitLayout ? 0.5f : 0.24f), sh * 0.075f};
  Rectangle confirmButton =
      {sw * (portraitLayout ? 0.54f : 0.75f),
       sh * (portraitLayout ? 0.87f : 0.78f),
       sw * (portraitLayout ? 0.4f : 0.19f),
       sh * (portraitLayout ? 0.07f : 0.075f)};
  Rectangle cancelButton =
      {sw * (portraitLayout ? 0.06f : 0.53f),
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
  int leftAdjacent = (selected + CHARACTER_COUNT - 1) % CHARACTER_COUNT;
  int rightAdjacent = (selected + 1) % CHARACTER_COUNT;
  float sideY = centerY + sh * (portraitLayout ? 0.02f : 0.035f);
  Rectangle leftPortrait =
      PortraitBounds(leftAdjacent, centerX - sideOffset, sideY, sideWidth,
                     sideHeight);
  Rectangle rightPortrait =
      PortraitBounds(rightAdjacent, centerX + sideOffset, sideY, sideWidth,
                     sideHeight);
  Rectangle carouselArea = {arrowMargin, centerY - portraitHeight / 2,
                            sw - arrowMargin * 2, portraitHeight};

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
    float sideAlpha = 0.75f * (1 - detailProgress);
    DrawPortrait(leftAdjacent, centerX - sideOffset, sideY, sideWidth, sideHeight,
                 sideAlpha);
    DrawPortrait(rightAdjacent, centerX + sideOffset, sideY, sideWidth,
                 sideHeight, sideAlpha);
    DrawPortrait(selected, portraitX, portraitY, portraitWidth, portraitHeight, 1);
    if (!inDetails)
      DrawTextCentered(names[selected], centerX,
                       sh * (portraitLayout ? 0.46f : 0.735f),
                       sh * 0.04f, (Color){245, 215, 145, 255});
  }

  if (!inDetails) {
    if (scrollProgress >= 1) {
      float labelY = sh * (portraitLayout ? 0.43f : 0.69f);
      DrawTextCentered(names[leftAdjacent], centerX - sideOffset, labelY,
                       sh * 0.027f, (Color){200, 185, 150, 210});
      DrawTextCentered(names[rightAdjacent], centerX + sideOffset, labelY,
                       sh * 0.027f, (Color){200, 185, 150, 210});
    }

    bool leftClicked =
        DrawArrow(leftArrowTexture, leftArrow, BUTTON_SOUND_TRAINING_CANCEL);
    bool rightClicked =
        DrawArrow(rightArrowTexture, rightArrow, BUTTON_SOUND_TRAINING_START);
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

    float detailsX = portraitLayout ? sw * 0.08f : panelX;
    float detailsWidth = portraitLayout ? sw * 0.84f : sw * 0.4f;
    float detailsLabelSize = sh * 0.021f;
    float detailsTextSize = sh * 0.018f;
    float skillLabelY = sh * (portraitLayout ? 0.47f : 0.32f);
    float skillTextY = sh * (portraitLayout ? 0.5f : 0.35f);
    float titleLabelY = sh * (portraitLayout ? 0.565f : 0.42f);
    float titleTextY = sh * (portraitLayout ? 0.595f : 0.45f);
    DrawTextEx(font, "COMPÉTENCE", (Vector2){detailsX, skillLabelY},
               detailsLabelSize, detailsLabelSize * 0.05f, textColor);
    Rectangle skillBox = {detailsX, skillTextY, detailsWidth,
                          sh * (portraitLayout ? 0.055f : 0.06f)};
    DrawWrappedText(TextFormat("%s — %s", CharacterSkillName(selected),
                               CharacterSkillDescription(selected)),
                    skillBox, detailsTextSize, textColor, 0);
    DrawTextEx(font, "TITRE DU PERSONNAGE",
               (Vector2){detailsX, titleLabelY},
               detailsLabelSize, detailsLabelSize * 0.05f, textColor);
    Rectangle titleBox = {detailsX, titleTextY, detailsWidth,
                          sh * (portraitLayout ? 0.055f : 0.06f)};
    DrawWrappedText(TextFormat("%s — %s", CharacterTitleName(selected),
                               CharacterTitleDescription(selected)),
                    titleBox, detailsTextSize, textColor, 0);
    DrawTextEx(font, "TITRES DU JOUEUR",
               (Vector2){detailsX, sh * (portraitLayout ? 0.65f : 0.515f)},
               detailsLabelSize, detailsLabelSize * 0.05f, textColor);
    char playerTitles[512];
    CharacterPlayerTitlesText(playerTitles, sizeof(playerTitles));
    Rectangle playerTitlesBox =
        {detailsX, sh * (portraitLayout ? 0.68f : 0.54f), detailsWidth,
         sh * 0.06f};
    DrawWrappedText(playerTitles, playerTitlesBox, sh * 0.013f, textColor, 0);
    Rectangle storyBox =
        portraitLayout
            ? (Rectangle){sw * 0.08f, sh * 0.75f, sw * 0.84f, sh * 0.09f}
            : (Rectangle){panelX, sh * 0.61f, sw * 0.4f, sh * 0.14f};
    float storySize =
        portraitLayout ? fminf(sh * 0.022f, shortSide * 0.032f) : sh * 0.022f;
    Rectangle storyTextBox = storyBox;
    storyTextBox.width -= 14 * GameUiScale();
    Color storyColor = (Color){235, 230, 215, (unsigned char)(255 * panelAlpha)};
    if (stories[selected]) {
      if (detailProgress == 1 && !confirming &&
          CheckCollisionPointRec(GetMousePosition(), storyBox))
        storyScroll[selected] -= GetMouseWheelMove() * storySize * 3;
      storyScroll[selected] =
          Clamp(storyScroll[selected], 0, storyMaxScroll[selected]);
      storyMaxScroll[selected] =
          DrawWrappedText(stories[selected], storyTextBox, storySize, storyColor,
                         storyScroll[selected]) -
          storyTextBox.height;
      storyMaxScroll[selected] = fmaxf(0, storyMaxScroll[selected]);
      storyScroll[selected] =
          Clamp(storyScroll[selected], 0, storyMaxScroll[selected]);
      if (storyMaxScroll[selected] > 0) {
        float trackWidth = 3 * GameUiScale();
        float thumbHeight =
            fmaxf(trackWidth * 3,
                  storyBox.height * storyBox.height /
                      (storyBox.height + storyMaxScroll[selected]));
        float thumbY = storyBox.y +
                       (storyBox.height - thumbHeight) * storyScroll[selected] /
                           storyMaxScroll[selected];
        DrawRectangleRec((Rectangle){storyBox.x + storyBox.width - trackWidth,
                                     storyBox.y, trackWidth, storyBox.height},
                         (Color){100, 85, 60, 170});
        DrawRectangleRec((Rectangle){storyBox.x + storyBox.width - trackWidth,
                                     thumbY, trackWidth, thumbHeight},
                         (Color){245, 215, 145, 230});
      }
    }

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

  UpdateCarouselMouse(!detailTarget && detailProgress == 0 &&
                          scrollProgress == 1 && !confirming,
                      carouselArea, leftArrow, rightArrow, leftPortrait,
                      rightPortrait,
                      fmaxf(shortSide * 0.08f, 24 * GameUiScale()));

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
