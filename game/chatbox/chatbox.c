#include "chatbox.h"
#include "../ui_scale.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

#define MAX_LEN 512
#define MAX_DONE 64

static Texture2D tex;
static Font dialogueFont;
static bool dialogueFontReady, dialogueFontLoaded;
static char current[MAX_LEN];
static char done[MAX_DONE][MAX_LEN];
static int doneCount;
static float progress;
static bool inputConsumed;
static int scrollBack;

static bool Completed(const char *text) {
  for (int i = 0; i < doneCount; i++) {
    if (strcmp(done[i], text) == 0)
      return true;
  }
  return false;
}

bool IsChatboxFinished(const char *text) {
  if (!text)
    return false;
  return Completed(text);
}

bool IsChatboxInputConsumed(void) {
  if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsKeyDown(KEY_SPACE))
    inputConsumed = false;
  return inputConsumed;
}

static bool Click(Rectangle bounds) {
  if (IsChatboxInputConsumed())
    return false;

  bool mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                      CheckCollisionPointRec(GetMousePosition(), bounds);
  bool pressed = mousePressed || IsKeyPressed(KEY_SPACE);
  if (!pressed)
    return false;

  inputConsumed = true;
  return true;
}

static int Utf8Length(const char *text) {
  int count = 0;
  for (const unsigned char *byte = (const unsigned char *)text; *byte; byte++)
    if ((*byte & 0xc0) != 0x80)
      count++;
  return count;
}

static int Utf8PrefixBytes(const char *text, int characters) {
  int bytes = 0;
  while (text[bytes] && characters-- > 0) {
    bytes++;
    while ((text[bytes] & 0xc0) == 0x80)
      bytes++;
  }
  return bytes;
}

static void DrawChatLine(Font font, const char *text, int x, int y,
                         float fontSize, float spacing) {
  DrawTextEx(font, text, (Vector2){x + 1, y + 1}, fontSize, spacing,
             (Color){0, 0, 0, 210});
  DrawTextEx(font, text, (Vector2){x, y}, fontSize, spacing, RAYWHITE);
}

static int DrawChatText(const char *text, int count, float x, float y,
                        float maxWidth, int firstRow, Font font,
                        float fontSize, float spacing, int lineHeight) {
  char buffer[MAX_LEN];
  char line[MAX_LEN] = "";
  int row = 0;

  snprintf(buffer, MAX_LEN, "%.*s", count, text);

  char *word = strtok(buffer, " ");
  while (word) {
    char test[MAX_LEN];
    size_t lineLength = strlen(line);
    size_t wordLength = strlen(word);
    size_t separatorLength = lineLength ? 1 : 0;
    size_t testLength = lineLength + separatorLength + wordLength;

    if (testLength >= sizeof(test)) {
      if (line[0]) {
        if (row >= firstRow)
          DrawChatLine(font, line, (int)x,
                       (int)y + (row - firstRow) * lineHeight, fontSize,
                       spacing);
        row++;
      }
      memcpy(line, word, wordLength + 1);
    } else {
      memcpy(test, line, lineLength);
      if (separatorLength)
        test[lineLength] = ' ';
      memcpy(test + lineLength + separatorLength, word, wordLength + 1);

      if (line[0] && MeasureTextEx(font, test, fontSize, spacing).x > maxWidth) {
        if (row >= firstRow)
          DrawChatLine(font, line, (int)x,
                       (int)y + (row - firstRow) * lineHeight, fontSize,
                       spacing);
        row++;
        memcpy(line, word, wordLength + 1);
      } else {
        memcpy(line, test, testLength + 1);
      }
    }
    word = strtok(NULL, " ");
  }

  if (line[0]) {
    if (row >= firstRow)
      DrawChatLine(font, line, (int)x,
                   (int)y + (row - firstRow) * lineHeight, fontSize, spacing);
    row++;
  }
  return row;
}

void chatbox(const char *text, const char *position) {
  if (!text || !position)
    return;

  bool high = strcmp(position, "high") == 0;
  bool low = strcmp(position, "low") == 0;
  if (!high && !low)
    return;

  if (tex.id == 0)
    tex = LoadTexture("assets/dialoguebox1.png");
  if (tex.id == 0)
    return;
  if (!dialogueFontReady) {
    dialogueFont = LoadFontEx("assets/Cinzel.ttf", 64, NULL, 0);
    dialogueFontLoaded = dialogueFont.texture.id != 0;
    dialogueFontReady = true;
    if (!dialogueFontLoaded) {
      TraceLog(LOG_ERROR, "Police Cinzel introuvable");
      dialogueFont = GetFontDefault();
    } else {
      SetTextureFilter(dialogueFont.texture, TEXTURE_FILTER_BILINEAR);
    }
  }
  if (Completed(text))
    return;

  if (strcmp(current, text) != 0) {
    if (current[0] && !Completed(current))
      return;
    snprintf(current, MAX_LEN, "%s", text);
    progress = 0;
    scrollBack = 0;
  }

  float length = (float)Utf8Length(current);

  float w = GetScreenWidth() * 0.55f;
  float h = w * tex.height / tex.width;
  float maxHeight = GetScreenHeight() * 0.25f;
  if (h > maxHeight) {
    h = maxHeight;
    w = h * tex.width / tex.height;
  }
  float x = (GetScreenWidth() - w) / 2;
  float margin = GameUiPixels(20);
  float y = high ? margin : GetScreenHeight() - h - margin;
  float fontSize = GameUiPixels(40);
  float spacing = fontSize * 0.01f;
  int lineHeight = GameUiPixels(52);
  Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
  Rectangle dest = {x, y, w, h};

  if (Click(dest)) {
    if (progress < length) {
      progress = length;
    } else {
      if (doneCount < MAX_DONE) {
        snprintf(done[doneCount], MAX_LEN, "%s", current);
        doneCount++;
      }
      return;
    }
  }

  progress += GetFrameTime() * 35.0f;
  if (progress > length)
    progress = length;

  DrawTexturePro(tex, source, dest, (Vector2){0, 0}, 0, WHITE);

  float textX = x + w * 0.06f, textY = y + h * 0.15f;
  float textWidth = w * 0.88f, textHeight = h * 0.72f;
  int visibleRows = (int)(textHeight / lineHeight);
  if (visibleRows < 1)
    visibleRows = 1;
  int visibleBytes = Utf8PrefixBytes(current, (int)progress);
  int rows = DrawChatText(current, visibleBytes, textX, textY, textWidth,
                          MAX_LEN, dialogueFont, fontSize, spacing, lineHeight);
  int maxScroll = rows > visibleRows ? rows - visibleRows : 0;
  if (CheckCollisionPointRec(GetMousePosition(), dest)) {
    float wheel = GetMouseWheelMove();
    if (wheel > 0)
      scrollBack++;
    else if (wheel < 0)
      scrollBack--;
  }
  if (scrollBack < 0)
    scrollBack = 0;
  if (scrollBack > maxScroll)
    scrollBack = maxScroll;
  int firstRow = maxScroll - scrollBack;
  BeginScissorMode((int)textX, (int)textY, (int)textWidth, (int)textHeight);
  DrawChatText(current, visibleBytes, textX, textY, textWidth, firstRow,
               dialogueFont, fontSize, spacing, lineHeight);
  EndScissorMode();
}

void ResetChatbox(void) {
  current[0] = '\0';
  doneCount = 0;
  progress = 0;
  inputConsumed = false;
  scrollBack = 0;
}

void UnloadChatbox(void) {
  ResetChatbox();
  UnloadTexture(tex);
  tex = (Texture2D){0};
  if (dialogueFontLoaded)
    UnloadFont(dialogueFont);
  dialogueFont = (Font){0};
  dialogueFontReady = dialogueFontLoaded = false;
}