#include "chatbox.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

#define MAX_LEN 512
#define MAX_DONE 64

static Texture2D tex;
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

static int DrawChatText(const char *text, int count, float x, float y,
                        float maxWidth, int firstRow) {
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
          DrawText(line, (int)x, (int)y + (row - firstRow) * 26, 20, RAYWHITE);
        row++;
      }
      memcpy(line, word, wordLength + 1);
    } else {
      memcpy(test, line, lineLength);
      if (separatorLength)
        test[lineLength] = ' ';
      memcpy(test + lineLength + separatorLength, word, wordLength + 1);

      if (line[0] && MeasureText(test, 20) > maxWidth) {
        if (row >= firstRow)
          DrawText(line, (int)x, (int)y + (row - firstRow) * 26, 20, RAYWHITE);
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
      DrawText(line, (int)x, (int)y + (row - firstRow) * 26, 20, RAYWHITE);
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
  if (Completed(text))
    return;

  if (strcmp(current, text) != 0) {
    if (current[0] && !Completed(current))
      return;
    snprintf(current, MAX_LEN, "%s", text);
    progress = 0;
    scrollBack = 0;
  }

  float length = (float)strlen(current);

  float w = GetScreenWidth() * 0.425f;
  float h = w * tex.height / tex.width;
  float x = (GetScreenWidth() - w) / 2;
  float y = high ? 20 : GetScreenHeight() - h - 20;
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

  float textX = x + w * 0.06f, textY = y + h * 0.25f;
  float textWidth = w * 0.88f, textHeight = h * 0.57f;
  int visibleRows = (int)(textHeight / 26);
  if (visibleRows < 1)
    visibleRows = 1;
  int rows = DrawChatText(current, (int)progress, textX, textY, textWidth, MAX_LEN);
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
  DrawChatText(current, (int)progress, textX, textY, textWidth, firstRow);
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
}