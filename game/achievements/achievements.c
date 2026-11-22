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
#define CHARACTER_COUNT 6
#define BOSS_COUNT 5
#define ALL_CHARACTER_BITS ((1u << CHARACTER_COUNT) - 1)
#define ALL_BOSS_BITS ((1u << BOSS_COUNT) - 1)

typedef struct {
  const char *key;
  const char *name;
  const char *description;
  bool hidden;
} Achievement;

static const Achievement achievements[ACHIEVEMENT_COUNT] = {
    {"game_completed", "Terminer le jeu", "Vaincre le dernier boss de la tour.",
     false},
    {"melina_ending", "Découvrir la fin secrète",
     "Découvrir l'ending de Melina.", true},
    {"legendary_item", "Obtenir un item légendaire",
     "Trouver un effet légendaire sur un item.", false},
    {"floor_10", "Vaincre le boss de l'étage 10",
     "Vaincre Béhémot, le démon de l'arrogance.", false},
    {"floor_20", "Vaincre le boss de l'étage 20",
     "Vaincre Séléna, la déesse déchue.", false},
    {"floor_30", "Vaincre le boss de l'étage 30",
     "Vaincre Agares, le démon de la vengeance.", false},
    {"floor_40", "Vaincre le boss de l'étage 40",
     "Vaincre Dame Marika, reine de la Cité des Cendres.", false},
    {"floor_50", "Vaincre le boss de l'étage 50",
     "Vaincre Malénia, la Lame Consort.", false},
    {"changed_fate", "Altérer le destin de la tour",
     "Choisir de réécrire le destin de la tour à son sommet.", false},
    {"first_victory", "Une première victoire",
     "Terminer votre premier étage.", false},
    {"floor_5", "Une étape de plus", "Terminer l'étage 5.", false},
    {"floor_15", "Toujours plus haut", "Terminer l'étage 15.", false},
    {"floor_25", "À mi-chemin", "Terminer l'étage 25.", false},
    {"floor_35", "Rien ne vous arrête", "Terminer l'étage 35.", false},
    {"floor_45", "Dernière ligne droite", "Terminer l'étage 45.", false},
    {"untouched_boss", "Pas une égratignure",
     "Vaincre un boss sans subir de dégât.", true},
    {"level_10", "L'expérience forge la légende",
     "Atteindre le niveau 10.", false},
    {"found_letter", "Une lettre oubliée",
     "Trouver la lettre cachée de Melina.", true},
    {"final_boss_perfect", "Fendre Malénia sans blessure",
     "Vaincre le boss de l'étage 50 sans subir de dégât.", true},
    {"all_bosses_perfect", "Les cinq couronnes intactes",
     "Vaincre sans dégâts les boss des étages 10, 20, 30, 40 et 50.", true},
    {"all_characters_complete", "Le destin des six",
     "Terminer la tour avec chacun des six personnages.", true},
    {"all_bosses_all_characters", "Chronique de trente victoires",
     "Vaincre chacun des cinq boss avec chacun des six personnages.", true},
};

static const Achievement titles[TITLE_COUNT] = {
    {"changed_fate", "Celui qui a changé le destin",
     "Les gains d'Etris sont augmentés de 25 %.", false},
    {"found_truth", "Celui qui a découvert toute la vérité",
     "L'expérience obtenue est augmentée de 20 %.", false},
    {"first_ascent", "Pionnier de la tour",
     "Gagne 10 % d'expérience supplémentaire.",
     false},
    {"tower_conqueror", "Conquérant de la tour",
     "Obtient un artefact supplémentaire à chaque niveau gagné.", false},
    {"untouched", "Intouchable",
     "Gagne 10 % d'Etris supplémentaire après chaque combat.", false},
    {"boss_hunter", "Chasseur de boss",
     "Gagne 15 % d'Etris supplémentaire après chaque combat.", false},
    {"vengeance", "Héritier de la vengeance",
     "L'expérience obtenue est augmentée de 10 %.", false},
    {"ashen_crown", "Couronne des cendres",
     "Gagne 5 % d'expérience supplémentaire.", false},
    {"fortune", "Favori de la fortune",
     "Obtient un artefact supplémentaire à chaque niveau gagné.", false},
    {"final_edge", "Lame de Malénia",
     "Gagne 10 % d'Etris supplémentaire après chaque combat.",
     false},
    {"perfect_crown", "Couronne intacte",
     "Les gains d'Etris sont augmentés de 20 %.", false},
    {"six_destinies", "Tisseur des six destins",
     "L'expérience obtenue est augmentée de 15 %.", false},
    {"boss_archivist", "Archiviste des couronnes",
     "Obtiens un objet supplémentaire à chaque niveau gagné.", false},
};

static bool unlocked[ACHIEVEMENT_COUNT];
static int notificationQueue[ACHIEVEMENT_COUNT];
static int notificationHead, notificationCount;
static bool titleUnlocked[TITLE_COUNT];
static int titleQueue[TITLE_COUNT], titleHead, titleCount, activeTitle = -1;
static unsigned int perfectBosses, completedCharacters;
static unsigned int bossCharacters[BOSS_COUNT];
static int equippedTitle = -1, titleSelection = -1;
static float notificationTime, achievementScroll, titleSelectionScroll;
static float titleAnimation;
static bool titleSelectionError;

static void RefreshChallengeUnlocks(void);

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
  perfectBosses = completedCharacters = 0;
  memset(bossCharacters, 0, sizeof(bossCharacters));
  titleHead = titleCount = 0;
  equippedTitle = titleSelection = -1;
  activeTitle = -1;
  notificationTime = achievementScroll = 0;
  titleSelectionScroll = 0;
  titleSelectionError = false;
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
    if (strncmp(line, "equipped:", 9) == 0) {
      equippedTitle = -1;
      if (strcmp(line + 9, "none") != 0)
        for (int i = 0; i < TITLE_COUNT; i++)
          if (strcmp(line + 9, titles[i].key) == 0)
            equippedTitle = i;
    }
    int floor, character, boss;
    if (sscanf(line, "perfect_boss:%d", &floor) == 1 &&
        floor >= 10 && floor <= 50 && floor % 10 == 0)
      perfectBosses |= 1u << (floor / 10 - 1);
    if (sscanf(line, "ending_character:%d", &character) == 1 &&
        character >= 0 && character < CHARACTER_COUNT)
      completedCharacters |= 1u << character;
    if (sscanf(line, "boss_character:%d:%d", &boss, &character) == 2 &&
        boss >= 0 && boss < BOSS_COUNT &&
        character >= 0 && character < CHARACTER_COUNT)
      bossCharacters[boss] |= 1u << character;
  }
  fclose(file);
  if (equippedTitle >= 0 && !titleUnlocked[equippedTitle])
    equippedTitle = -1;
  RefreshChallengeUnlocks();
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

static void RefreshChallengeUnlocks(void) {
  if ((perfectBosses & (1u << (BOSS_COUNT - 1))) != 0) {
    AchievementsUnlock(ACHIEVEMENT_FINAL_BOSS_PERFECT);
    AchievementsUnlockTitle(TITLE_FINAL_EDGE);
  }
  if ((perfectBosses & ALL_BOSS_BITS) == ALL_BOSS_BITS) {
    AchievementsUnlock(ACHIEVEMENT_ALL_BOSSES_PERFECT);
    AchievementsUnlockTitle(TITLE_PERFECT_CROWN);
  }
  if ((completedCharacters & ALL_CHARACTER_BITS) == ALL_CHARACTER_BITS) {
    AchievementsUnlock(ACHIEVEMENT_ALL_CHARACTERS_COMPLETE);
    AchievementsUnlockTitle(TITLE_SIX_DESTINIES);
  }
  bool allBossesWithAllCharacters = true;
  for (int boss = 0; boss < BOSS_COUNT; boss++)
    if ((bossCharacters[boss] & ALL_CHARACTER_BITS) != ALL_CHARACTER_BITS)
      allBossesWithAllCharacters = false;
  if (allBossesWithAllCharacters) {
    AchievementsUnlock(ACHIEVEMENT_ALL_BOSSES_ALL_CHARACTERS);
    AchievementsUnlockTitle(TITLE_BOSS_ARCHIVIST);
  }
}

bool AchievementsRecordBossVictory(int character, int floor, bool perfect) {
  int boss = floor >= 10 && floor <= 50 && floor % 10 == 0
                 ? floor / 10 - 1
                 : -1;
  if (character < 0 || character >= CHARACTER_COUNT || boss < 0) {
    TraceLog(LOG_WARNING, "Victoire de boss invalide pour les défis");
    return false;
  }

  unsigned int characterBit = 1u << character;
  if ((bossCharacters[boss] & characterBit) == 0) {
    if (!AppendUnlock(TextFormat("boss_character:%d:%d", boss, character)))
      return false;
    bossCharacters[boss] |= characterBit;
  }
  if (perfect && (perfectBosses & (1u << boss)) == 0) {
    if (!AppendUnlock(TextFormat("perfect_boss:%d", floor)))
      return false;
    perfectBosses |= 1u << boss;
  }
  RefreshChallengeUnlocks();
  return true;
}

bool AchievementsRecordEnding(int character) {
  if (character < 0 || character >= CHARACTER_COUNT) {
    TraceLog(LOG_WARNING, "Personnage invalide pour le défi des six destins");
    return false;
  }
  unsigned int characterBit = 1u << character;
  if ((completedCharacters & characterBit) == 0) {
    if (!AppendUnlock(TextFormat("ending_character:%d", character)))
      return false;
    completedCharacters |= characterBit;
  }
  RefreshChallengeUnlocks();
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

int AchievementsUnlockedTitleAt(int index) {
  for (int i = 0; i < TITLE_COUNT; i++)
    if (titleUnlocked[i] && index-- == 0)
      return i;
  return -1;
}

int AchievementsEquippedTitle(void) { return equippedTitle; }

bool AchievementsSetEquippedTitle(int title) {
  if (title < -1 || title >= TITLE_COUNT ||
      (title >= 0 && !titleUnlocked[title])) {
    TraceLog(LOG_WARNING, "Titre personnel invalide ou verrouillé");
    return false;
  }
  if (equippedTitle == title)
    return true;
  const char *key = title < 0 ? "equipped:none"
                              : TextFormat("equipped:%s", titles[title].key);
  if (!AppendUnlock(key))
    return false;
  equippedTitle = title;
  return true;
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

static void DrawFittedText(const char *text, Rectangle box, int size,
                           Color color) {
  int fittedSize = size;
  while (fittedSize > 10 && MeasureText(text, fittedSize) > box.width)
    fittedSize--;
  DrawText(text, (int)box.x, (int)(box.y + (box.height - fittedSize) / 2),
           fittedSize, color);
}

void AchievementsStartTitleSelection(void) {
  titleSelection = equippedTitle;
  if (titleSelection < 0 || !titleUnlocked[titleSelection])
    titleSelection = AchievementsUnlockedTitleAt(0);
  titleSelectionScroll = 0;
  titleSelectionError = false;
}

bool AchievementsDrawTitleSelection(void) {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  float scale = GameUiScale();
  bool compact = sw < 700;
  float width = fminf(sw * 0.94f, 1040 * scale);
  float height = fminf(sh * 0.92f, 820 * scale);
  Rectangle panel = {(sw - width) / 2, (sh - height) / 2, width, height};
  Rectangle list =
      compact ? (Rectangle){panel.x + 14 * scale, panel.y + height * 0.15f,
                            panel.width - 28 * scale, height * 0.46f}
              : (Rectangle){panel.x + 20 * scale, panel.y + height * 0.16f,
                            panel.width * 0.52f, height * 0.72f};
  Rectangle detail =
      compact
          ? (Rectangle){panel.x + 14 * scale, list.y + list.height + 12 * scale,
                        panel.width - 28 * scale, height * 0.19f}
          : (Rectangle){list.x + list.width + 18 * scale, list.y,
                        panel.width * 0.40f, list.height};
  float rowHeight = compact ? 54 * scale : 68 * scale;
  float maxScroll =
      fmaxf(0, AchievementsTitleCount() * rowHeight - list.height);
  Vector2 mouse = GetMousePosition();
  if (CheckCollisionPointRec(mouse, list))
    titleSelectionScroll -= GetMouseWheelMove() * rowHeight * 2;
  titleSelectionScroll = fmaxf(0, fminf(titleSelectionScroll, maxScroll));

  DrawRectangle(0, 0, sw, sh, BLACK);
  DrawRectangleRounded(panel, 0.035f, 12, (Color){18, 19, 24, 255});
  DrawRectangleRoundedLinesEx(panel, 0.035f, 12, 2 * scale,
                              (Color){218, 179, 91, 255});
  const char *heading = "CHOISISSEZ VOTRE TITRE";
  int headingSize = GameUiPixels(compact ? 24 : 32);
  DrawText(heading, (sw - MeasureText(heading, headingSize)) / 2,
           (int)(panel.y + height * 0.045f), headingSize,
           (Color){245, 215, 145, 255});
  const char *subheading = "Survolez un titre pour découvrir son effet.";
  int subheadingSize = GameUiPixels(compact ? 13 : 16);
  DrawText(subheading, (sw - MeasureText(subheading, subheadingSize)) / 2,
           (int)(panel.y + height * 0.105f), subheadingSize,
           (Color){190, 188, 180, 255});

  int hoveredTitle = -1;
  BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
  int count = AchievementsTitleCount();
  for (int i = 0; i < count; i++) {
    int title = AchievementsUnlockedTitleAt(i);
    Rectangle row = {list.x, list.y + i * rowHeight - titleSelectionScroll,
                     list.width, rowHeight - 7 * scale};
    bool hover =
        CheckCollisionPointRec(mouse, row) && CheckCollisionPointRec(mouse, list);
    bool selected = title == titleSelection;
    DrawRectangleRounded(row, 0.12f, 8,
                         selected ? (Color){72, 57, 31, 245}
                         : hover  ? (Color){40, 39, 36, 245}
                                  : (Color){28, 29, 34, 245});
    DrawRectangleRoundedLinesEx(row, 0.12f, 8, selected ? 2 * scale : scale,
                                selected ? (Color){235, 194, 105, 255}
                                         : (Color){80, 82, 91, 255});
    DrawFittedText(titles[title].name,
                   (Rectangle){row.x + 12 * scale, row.y,
                               row.width - 24 * scale, row.height},
                   GameUiPixels(compact ? 15 : 19),
                   (Color){255, 235, 190, 255});
    if (hover) {
      hoveredTitle = title;
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        titleSelection = title;
        titleSelectionError = false;
        PlayButtonClickSound();
      }
    }
  }
  EndScissorMode();

  int detailTitle = hoveredTitle >= 0 ? hoveredTitle : titleSelection;
  DrawRectangleRounded(detail, 0.08f, 8, (Color){28, 29, 34, 245});
  DrawRectangleRoundedLinesEx(detail, 0.08f, 8, scale,
                              (Color){115, 99, 67, 255});
  if (detailTitle >= 0) {
    int padding = GameUiPixels(14);
    int titleSize = GameUiPixels(compact ? 17 : 22);
    Rectangle titleBox = {detail.x + padding, detail.y + padding,
                          detail.width - 2 * padding, titleSize + 8 * scale};
    DrawFittedText(titles[detailTitle].name, titleBox, titleSize,
                   (Color){255, 225, 145, 255});
    int effectSize = GameUiPixels(compact ? 13 : 17);
    Rectangle effectBox = {detail.x + padding,
                           titleBox.y + titleBox.height + 4 * scale,
                           detail.width - 2 * padding,
                           detail.height - titleBox.height - 2 * padding};
    DrawFittedText(titles[detailTitle].description, effectBox, effectSize,
                   (Color){205, 200, 185, 255});
  }

  float buttonWidth = fminf(340 * scale, width - 48 * scale);
  Rectangle confirm = {panel.x + (width - buttonWidth) / 2,
                       panel.y + height - 60 * scale, buttonWidth, 42 * scale};
  bool canConfirm = titleSelection >= 0;
  bool hoverConfirm = canConfirm && CheckCollisionPointRec(mouse, confirm);
  DrawRectangleRounded(confirm, 0.18f, 8,
                       hoverConfirm ? (Color){115, 86, 42, 255}
                                    : (Color){65, 53, 36, 255});
  DrawRectangleRoundedLinesEx(confirm, 0.18f, 8, scale,
                              (Color){218, 179, 91, 255});
  const char *confirmText = "CHOISIR CE TITRE";
  int confirmSize = GameUiPixels(compact ? 15 : 18);
  DrawText(confirmText,
           (int)(confirm.x +
                 (confirm.width - MeasureText(confirmText, confirmSize)) / 2),
           (int)(confirm.y + (confirm.height - confirmSize) / 2), confirmSize,
           canConfirm ? RAYWHITE : (Color){130, 130, 130, 255});

  if (titleSelectionError) {
    const char *error = "Impossible d'enregistrer ce titre. Réessayez.";
    int errorSize = GameUiPixels(compact ? 11 : 14);
    DrawText(error, (sw - MeasureText(error, errorSize)) / 2,
             (int)(confirm.y - 24 * scale), errorSize, RED);
  }
  if (hoverConfirm && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (AchievementsSetEquippedTitle(titleSelection)) {
      PlayButtonClickSound();
      titleSelectionError = false;
      return true;
    }
    titleSelectionError = true;
  }
  return false;
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
  bool visible = isUnlocked || !achievements[index].hidden;
  DrawText(visible ? achievements[index].name : "???",
           (int)box.x + padding, (int)box.y + GameUiPixels(8), titleSize,
           isUnlocked ? (Color){255, 225, 145, 255}
                      : (Color){205, 205, 210, 255});
  DrawText(visible ? achievements[index].description : "???",
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
