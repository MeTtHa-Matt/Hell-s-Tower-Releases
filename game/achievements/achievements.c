#include "achievements.h"
#include "../tower.h"
#include "../ui_scale.h"
#include "../buttons/button_sounds.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define DATA_PATH_LEN 2048
#define NOTIFICATION_DURATION 5.0f

typedef struct {
  const char *key;
  const char *name;
  const char *description;
} Achievement;

static const Achievement achievements[ACHIEVEMENT_COUNT] = {
    {"game_completed", "Terminer le jeu", "Vaincre le dernier boss de la tour."},
    {"melina_ending", "Découvrir la fin secrète",
     "Découvrir l'ending de Melina."},
    {"legendary_item", "Obtenir un item légendaire",
     "Trouver un effet légendaire sur un item."},
    {"floor_10", "Terminer l'étage 10", "Vaincre le boss du dixième étage."},
    {"floor_20", "Terminer l'étage 20", "Vaincre le boss du vingtième étage."},
    {"floor_30", "Terminer l'étage 30", "Vaincre le boss du trentième étage."},
    {"floor_40", "Terminer l'étage 40", "Vaincre le boss du quarantième étage."},
    {"floor_50", "Terminer l'étage 50", "Vaincre le boss du cinquantième étage."},
};

static bool unlocked[ACHIEVEMENT_COUNT];
static int notificationQueue[ACHIEVEMENT_COUNT];
static int notificationHead, notificationCount;
static float notificationTime, achievementScroll;

static bool AchievementPaths(char *directory, char *path) {
  if (!TowerBuildDataFilePath("succes.txt", directory, DATA_PATH_LEN, path,
                              DATA_PATH_LEN)) {
    TraceLog(LOG_ERROR, "Impossible de trouver le fichier des succès");
    return false;
  }
  return true;
}

void AchievementsInit(void) {
  memset(unlocked, 0, sizeof(unlocked));
  notificationHead = notificationCount = 0;
  notificationTime = achievementScroll = 0;
  char directory[DATA_PATH_LEN], path[DATA_PATH_LEN], line[128];
  if (!AchievementPaths(directory, path))
    return;
  FILE *file = fopen(path, "r");
  if (!file) {
    if (FileExists(path))
      TraceLog(LOG_ERROR, "Impossible de lire %s", path);
    return;
  }
  while (fgets(line, sizeof(line), file)) {
    line[strcspn(line, "\r\n")] = '\0';
    for (int i = 0; i < ACHIEVEMENT_COUNT; i++)
      if (strcmp(line, achievements[i].key) == 0)
        unlocked[i] = true;
  }
  fclose(file);
}

bool AchievementsUnlock(AchievementId achievement) {
  if (achievement < 0 || achievement >= ACHIEVEMENT_COUNT) {
    TraceLog(LOG_WARNING, "Identifiant de succès invalide");
    return false;
  }
  if (unlocked[achievement])
    return false;

  char directory[DATA_PATH_LEN], path[DATA_PATH_LEN];
  if (!AchievementPaths(directory, path))
    return false;
  if (!DirectoryExists(directory) && MakeDirectory(directory) != 0 &&
      !DirectoryExists(directory)) {
    TraceLog(LOG_ERROR, "Impossible de créer le dossier des succès: %s",
             directory);
    return false;
  }
  FILE *file = fopen(path, "a+b");
  if (!file) {
    TraceLog(LOG_ERROR, "Impossible d'ajouter un succès à %s", path);
    return false;
  }
  bool written = fseek(file, 0, SEEK_END) == 0;
  long end = written ? ftell(file) : -1;
  if (end > 0) {
    written = fseek(file, -1, SEEK_END) == 0;
    int last = written ? fgetc(file) : EOF;
    written = last != EOF && fseek(file, 0, SEEK_END) == 0;
    if (written && last != '\n')
      written = fputc('\n', file) != EOF;
  } else if (end < 0) {
    written = false;
  }
  if (written)
    written = fprintf(file, "%s\n", achievements[achievement].key) >= 0;
  if (written && fflush(file) != 0)
    written = false;
  if (fclose(file) != 0)
    written = false;
  if (!written) {
    TraceLog(LOG_ERROR, "Impossible d'enregistrer le succès: %s",
             achievements[achievement].key);
    return false;
  }

  unlocked[achievement] = true;
  int tail = (notificationHead + notificationCount) % ACHIEVEMENT_COUNT;
  notificationQueue[tail] = achievement;
  notificationCount++;
  return true;
}

void AchievementsDrawNotifications(void) {
  if (notificationCount == 0)
    return;
  notificationTime += GetFrameTime();
  if (notificationTime >= NOTIFICATION_DURATION) {
    notificationHead = (notificationHead + 1) % ACHIEVEMENT_COUNT;
    notificationCount--;
    notificationTime = 0;
    if (notificationCount == 0)
      return;
  }

  int id = notificationQueue[notificationHead];
  int sw = GetScreenWidth();
  float scale = GameUiScale();
  float margin = 20 * scale;
  float width = fminf(430 * scale, sw * 0.9f);
  float height = 118 * scale;
  float x = sw - margin - width;
  float y = 18 * scale;
  float enter = fminf(1, notificationTime / 0.35f);
  float exit = fminf(1, (NOTIFICATION_DURATION - notificationTime) / 0.35f);
  float alpha = fminf(enter, exit);
  x += (width + margin) * (1 - enter);
  Rectangle box = {x, y, width, height};
  Color gold = Fade((Color){225, 185, 75, 255}, alpha);
  DrawRectangleRounded(box, 0.12f, 10,
                       Fade((Color){22, 20, 17, 248}, alpha));
  DrawRectangleRoundedLinesEx(box, 0.12f, 10, 2 * scale, gold);
  int padding = GameUiPixels(14);
  int headerSize = GameUiPixels(15);
  int titleSize = GameUiPixels(21);
  DrawText("SUCCÈS DÉBLOQUÉ", (int)box.x + padding,
           (int)box.y + GameUiPixels(12), headerSize, gold);
  DrawText(achievements[id].name, (int)box.x + padding,
           (int)box.y + GameUiPixels(43), titleSize,
           Fade((Color){255, 242, 205, 255}, alpha));
}

static void DrawAchievementRow(int index, Rectangle box, float scale) {
  bool isUnlocked = unlocked[index];
  Color fill = isUnlocked ? (Color){72, 57, 31, 240}
                          : (Color){28, 29, 34, 235};
  Color edge = isUnlocked ? (Color){200, 160, 68, 255}
                          : (Color){80, 82, 91, 255};
  DrawRectangleRounded(box, 0.12f, 8, fill);
  DrawRectangleRoundedLinesEx(box, 0.12f, 8, scale, edge);
  int padding = GameUiPixels(12);
  int titleSize = GameUiPixels(17);
  int descriptionSize = GameUiPixels(12);
  int statusSize = GameUiPixels(13);
  DrawText(achievements[index].name, (int)box.x + padding,
           (int)box.y + GameUiPixels(4), titleSize,
           isUnlocked ? (Color){255, 225, 145, 255} : (Color){205, 205, 210, 255});
  DrawText(achievements[index].description, (int)box.x + padding,
           (int)box.y + GameUiPixels(25), descriptionSize,
           (Color){170, 172, 180, 255});
  const char *status = isUnlocked ? "DÉBLOQUÉ" : "VERROUILLÉ";
  int statusX = (int)(box.x + box.width - padding -
                      MeasureText(status, statusSize));
  DrawText(status, statusX, (int)(box.y + (box.height - statusSize) / 2),
           statusSize, isUnlocked ? (Color){235, 195, 95, 255}
                                  : (Color){125, 128, 138, 255});
}

bool AchievementsDrawScreen(void) {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  float scale = GameUiScale();
  float width = fminf(sw * 0.92f, 760 * scale);
  float height = fminf(sh * 0.9f, 680 * scale);
  Rectangle panel = {(sw - width) / 2, (sh - height) / 2, width, height};
  Rectangle list = {panel.x + 14 * scale, panel.y + 72 * scale,
                    panel.width - 28 * scale, panel.height - 132 * scale};
  float rowHeight = 54 * scale;
  float maxScroll =
      fmaxf(0, ACHIEVEMENT_COUNT * rowHeight - list.height);
  Vector2 mouse = GetMousePosition();
  if (CheckCollisionPointRec(mouse, list))
    achievementScroll -= GetMouseWheelMove() * rowHeight * 2;
  achievementScroll = fmaxf(0, fminf(achievementScroll, maxScroll));

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 175});
  DrawRectangleRounded(panel, 0.035f, 12, (Color){18, 19, 24, 248});
  DrawRectangleRoundedLinesEx(panel, 0.035f, 12, 2 * scale,
                              (Color){218, 179, 91, 255});
  int titleSize = GameUiPixels(32);
  const char *title = "SUCCÈS";
  DrawText(title, (sw - MeasureText(title, titleSize)) / 2,
           (int)panel.y + GameUiPixels(15), titleSize,
           (Color){245, 215, 145, 255});

  int count = 0;
  for (int i = 0; i < ACHIEVEMENT_COUNT; i++)
    count += unlocked[i];
  const char *progress =
      TextFormat("%d / %d débloqués", count, ACHIEVEMENT_COUNT);
  DrawText(progress, (sw - MeasureText(progress, GameUiPixels(15))) / 2,
           (int)panel.y + GameUiPixels(51), GameUiPixels(15),
           (Color){190, 188, 180, 255});

  BeginScissorMode((int)list.x, (int)list.y, (int)list.width,
                   (int)list.height);
  for (int i = 0; i < ACHIEVEMENT_COUNT; i++) {
    Rectangle row = {list.x, list.y + i * rowHeight - achievementScroll,
                     list.width, rowHeight - 4 * scale};
    DrawAchievementRow(i, row, scale);
  }
  EndScissorMode();

  float closeWidth = fminf(210 * scale, width * 0.6f);
  Rectangle close = {panel.x + (width - closeWidth) / 2,
                     panel.y + height - 46 * scale, closeWidth, 32 * scale};
  bool hover = CheckCollisionPointRec(mouse, close);
  DrawRectangleRounded(close, 0.18f, 8,
                       hover ? (Color){115, 86, 42, 255}
                             : (Color){65, 53, 36, 255});
  DrawRectangleRoundedLinesEx(close, 0.18f, 8, scale,
                              (Color){218, 179, 91, 255});
  int closeSize = GameUiPixels(15);
  const char *closeText = "RETOUR";
  DrawText(closeText,
           (int)(close.x + (close.width - MeasureText(closeText, closeSize)) / 2),
           (int)(close.y + (close.height - closeSize) / 2), closeSize,
           RAYWHITE);

  if (IsKeyPressed(KEY_ESCAPE)) {
    achievementScroll = 0;
    return true;
  }
  if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    PlayButtonClickSound();
    achievementScroll = 0;
    return true;
  }
  return false;
}
