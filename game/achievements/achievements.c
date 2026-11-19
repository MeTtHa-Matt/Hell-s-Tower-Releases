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

static const Achievement titles[TITLE_COUNT] = {
    {"changed_fate", "Celui qui a changé le destin",
     "Les victoires rapportent 25 % d'Etris supplémentaires."},
    {"found_truth", "Celui qui a découvert toute la vérité",
     "L'expérience obtenue est augmentée de 20 %."},
    {"first_ascent", "Pionnier de la tour",
     "Les avertissements des sceaux sont prolongés de 0,2 seconde."},
    {"tower_conqueror", "Conquérant de la tour",
     "Commence chaque mini-jeu avec une vie supplémentaire."},
    {"untouched", "Intouchable",
     "Une protection annule le premier dégât de chaque mini-jeu."},
};

static bool unlocked[ACHIEVEMENT_COUNT];
static int notificationQueue[ACHIEVEMENT_COUNT];
static int notificationHead, notificationCount;
static bool titleUnlocked[TITLE_COUNT];
static int titleQueue[TITLE_COUNT], titleHead, titleCount, activeTitle = -1;
static float notificationTime, achievementScroll;
static float titleAnimation;

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
  memset(titleUnlocked, 0, sizeof(titleUnlocked));
  titleHead = titleCount = 0;
  activeTitle = -1;
  notificationTime = achievementScroll = 0;
  titleAnimation = 0;
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
    if (strncmp(line, "title:", 6) == 0)
      for (int i = 0; i < TITLE_COUNT; i++)
        if (strcmp(line + 6, titles[i].key) == 0)
          titleUnlocked[i] = true;
  }
  fclose(file);
}

static bool AppendUnlock(const char *key) {
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
    written = fprintf(file, "%s\n", key) >= 0;
  if (written && fflush(file) != 0)
    written = false;
  if (fclose(file) != 0)
    written = false;
  if (!written) {
    TraceLog(LOG_ERROR, "Impossible d'enregistrer le succès: %s",
             key);
    return false;
  }
  return true;
}

bool AchievementsUnlock(AchievementId achievement) {
  if (achievement < 0 || achievement >= ACHIEVEMENT_COUNT) {
    TraceLog(LOG_WARNING, "Identifiant de succès invalide");
    return false;
  }
  if (unlocked[achievement] || !AppendUnlock(achievements[achievement].key))
    return false;

  unlocked[achievement] = true;
  int tail = (notificationHead + notificationCount) % ACHIEVEMENT_COUNT;
  notificationQueue[tail] = achievement;
  notificationCount++;
  return true;
}

bool AchievementsUnlockTitle(TitleId title) {
  if (title < 0 || title >= TITLE_COUNT) {
    TraceLog(LOG_WARNING, "Identifiant de titre invalide");
    return false;
  }
  if (titleUnlocked[title] ||
      !AppendUnlock(TextFormat("title:%s", titles[title].key)))
    return false;
  titleUnlocked[title] = true;
  titleQueue[(titleHead + titleCount) % TITLE_COUNT] = title;
  titleCount++;
  return true;
}

bool AchievementsTitleUnlocked(TitleId title) {
  return title >= 0 && title < TITLE_COUNT && titleUnlocked[title];
}

int AchievementsTitleCount(void) {
  int count = 0;
  for (int i = 0; i < TITLE_COUNT; i++)
    count += titleUnlocked[i];
  return count;
}

const char *AchievementsTitleName(int index) {
  for (int i = 0; i < TITLE_COUNT; i++)
    if (titleUnlocked[i] && index-- == 0)
      return titles[i].name;
  return "";
}

const char *AchievementsTitleDescription(int index) {
  for (int i = 0; i < TITLE_COUNT; i++)
    if (titleUnlocked[i] && index-- == 0)
      return titles[i].description;
  return "";
}

bool AchievementsTitleNotificationPending(void) {
  return titleCount > 0 || activeTitle >= 0;
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

void AchievementsDrawTitleNotification(void) {
  if (notificationCount > 0 || (titleCount == 0 && activeTitle < 0))
    return;
  if (activeTitle < 0) {
    activeTitle = titleQueue[titleHead];
    titleAnimation = 0;
  }
  titleAnimation = fminf(1, titleAnimation + GetFrameTime() / 0.55f);
  float eased = 1 - powf(1 - titleAnimation, 3);
  float scale = GameUiScale();
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  DrawRectangle(0, 0, sw, sh, Fade((Color){10, 9, 8, 245}, eased));
  float w = fminf(sw * 0.9f, 1050 * fmaxf(scale, 1));
  float h = fminf(sh * 0.56f, 430 * fmaxf(scale, 1));
  Rectangle panel = {(sw - w) / 2, (sh - h) / 2, w, h};
  DrawRectangleRounded(panel, 0.04f, 12,
                       Fade((Color){32, 27, 19, 255}, eased));
  DrawRectangleRoundedLinesEx(panel, 0.04f, 12, 3 * scale,
                              Fade((Color){243, 204, 112, 255}, eased));
  const char *header = "NOUVEAU TITRE";
  int headerSize = GameUiPixels(22);
  DrawText(header, (sw - MeasureText(header, headerSize)) / 2,
           (int)(panel.y + h * 0.2f), headerSize,
           Fade((Color){224, 192, 125, 255}, eased));
  float titleSize = fminf(sh * 0.085f, 78 * fmaxf(scale, 1)) *
                    (0.8f + eased * 0.2f);
  int size = (int)titleSize;
  while (size > GameUiPixels(12) &&
         MeasureText(titles[activeTitle].name, size) > w * 0.84f)
    size--;
  DrawText(titles[activeTitle].name,
           (sw - MeasureText(titles[activeTitle].name, size)) / 2,
           (int)(panel.y + h * 0.4f), size,
           Fade((Color){255, 236, 182, 255}, eased));
  int descSize = GameUiPixels(20);
  const char *description = titles[activeTitle].description;
  int descWidth = MeasureText(description, descSize);
  if (descWidth > w * 0.82f)
    descSize = (int)(descSize * w * 0.82f / descWidth);
  DrawText(description, (sw - MeasureText(description, descSize)) / 2,
           (int)(panel.y + h * 0.63f), descSize,
           Fade((Color){221, 214, 195, 255}, eased));
  const char *hint = "CLIQUEZ POUR CONTINUER";
  int hintSize = GameUiPixels(15);
  DrawText(hint, (sw - MeasureText(hint, hintSize)) / 2,
           (int)(panel.y + h * 0.84f), hintSize,
           Fade((Color){205, 184, 137, 255}, eased));
  if (titleAnimation >= 1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    titleHead = (titleHead + 1) % TITLE_COUNT;
    titleCount--;
    activeTitle = -1;
    titleAnimation = 0;
    PlayButtonClickSound();
  }
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
  int titleSize = GameUiPixels(24);
  int descriptionSize = GameUiPixels(17);
  int statusSize = GameUiPixels(16);
  DrawText(isUnlocked ? achievements[index].name : "???",
           (int)box.x + padding, (int)box.y + GameUiPixels(8), titleSize,
           isUnlocked ? (Color){255, 225, 145, 255} : (Color){205, 205, 210, 255});
  DrawText(isUnlocked ? achievements[index].description : "???",
           (int)box.x + padding, (int)box.y + GameUiPixels(44),
           descriptionSize, (Color){170, 172, 180, 255});
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
  float height = fminf(sh * 0.9f, 820 * scale);
  Rectangle panel = {(sw - width) / 2, (sh - height) / 2, width, height};
  Rectangle list = {panel.x + 14 * scale, panel.y + 96 * scale,
                    panel.width - 28 * scale, panel.height - 176 * scale};
  float rowHeight = 88 * scale;
  float maxScroll =
      fmaxf(0, ACHIEVEMENT_COUNT * rowHeight - 8 * scale - list.height);
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
           (int)panel.y + GameUiPixels(18), titleSize,
           (Color){245, 215, 145, 255});

  int count = 0;
  for (int i = 0; i < ACHIEVEMENT_COUNT; i++)
    count += unlocked[i];
  const char *progress =
      TextFormat("%d / %d débloqués", count, ACHIEVEMENT_COUNT);
  DrawText(progress, (sw - MeasureText(progress, GameUiPixels(15))) / 2,
           (int)panel.y + GameUiPixels(58), GameUiPixels(15),
           (Color){190, 188, 180, 255});

  BeginScissorMode((int)list.x, (int)list.y, (int)list.width,
                   (int)list.height);
  for (int i = 0; i < ACHIEVEMENT_COUNT; i++) {
    Rectangle row = {list.x, list.y + i * rowHeight - achievementScroll,
                     list.width, rowHeight - 8 * scale};
    DrawAchievementRow(i, row, scale);
  }
  EndScissorMode();

  float closeWidth = fminf(280 * scale, width - 48 * scale);
  Rectangle close = {panel.x + (width - closeWidth) / 2,
                     panel.y + height - 58 * scale, closeWidth, 40 * scale};
  bool hover = CheckCollisionPointRec(mouse, close);
  DrawRectangleRounded(close, 0.18f, 8,
                       hover ? (Color){115, 86, 42, 255}
                             : (Color){65, 53, 36, 255});
  DrawRectangleRoundedLinesEx(close, 0.18f, 8, scale,
                              (Color){218, 179, 91, 255});
  int closeSize = GameUiPixels(17);
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
