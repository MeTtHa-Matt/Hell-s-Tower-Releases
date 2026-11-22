#include "tower.h"
#include "buttons/button_sounds.h"
#include "../loading_screen/loading_screen.h"
#include "game_zone/bullet hell/bullethell.h"
#include "game_zone/mirror/mirror.h"
#include "game_zone/rings/rings.h"
#include "game_zone/seals/seals.h"
#include "game_zone/spikes/spikes.h"
#include "game_zone/finalboss/finalboss.h"
#include "game_zone/boss4/boss4.h"
#include "game_zone/boss3/boss3.h"
#include "game_zone/boss2/boss2.h"
#include "game_zone/boss1/boss1.h"
#include "game_zone/balance.h"
#include "game_zone/modifiers.h"
#include "game_zone/zone.h"
#include "achievements/achievements.h"
#include "characters/characters.h"
#include "items/functionnal/functionnal.h"
#include "items/items.h"
#include "levels/levels.h"
#include "raylib.h"
#include "ui_scale.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_PATH_LEN 2048

typedef enum { GAME_OFF, GAME_PLAYING, GAME_WON, GAME_LOST } TowerGameState;

static int currentFloor, currentDifficulty, etris;
static uint64_t towerRandomState;
static float etrisDisplay = -1;
static bool etrisTextureTried;
static Texture2D etrisTexture;
static int currentGame = -1;
static int playerCharacter = -1;
static bool towerStarted, tutorialSeen;
static bool exitRequested, musicAttempted;
static int preparedFloor;
static unsigned long long sigridSeen;
static char opponentPath[512], floorMusicPath[512];
static char *savedText;
static Music floorMusic;
static bool floorMusicPausedForLoading;
static float gameTime;
static bool gameRewarded;
static TowerGameState resultState;
static int resultSelection, resultFloor;

static const char *gameNames[5] = {"bullethell", "rings", "mirror", "spikes",
                                   "seals"};
static int previousMinigameByFloor[50];
static bool minigameHistoryLoaded;

bool TowerBuildDataFilePath(const char *filename, char *directory,
                            size_t directorySize, char *filePath,
                            size_t filePathSize) {
  if (!filename || !filename[0] || !directory || !directorySize ||
      !filePath || !filePathSize || strchr(filename, '/') ||
      strchr(filename, '\\'))
    return false;
  const char *baseDirectory;
  int directoryLength;

#ifdef _WIN32
  baseDirectory = getenv("LOCALAPPDATA");
  if (!baseDirectory || !baseDirectory[0])
    baseDirectory = getenv("APPDATA");
  if (!baseDirectory || !baseDirectory[0])
    return false;
  directoryLength =
      snprintf(directory, directorySize, "%s/HellsTower", baseDirectory);
#elif __APPLE__
  baseDirectory = getenv("HOME");
  if (!baseDirectory || !baseDirectory[0])
    return false;
  directoryLength = snprintf(directory, directorySize,
                             "%s/Library/Application Support/Hells Tower",
                             baseDirectory);
#else
  baseDirectory = getenv("XDG_DATA_HOME");
  if (baseDirectory && baseDirectory[0] == '/') {
    directoryLength =
        snprintf(directory, directorySize, "%s/hells-tower", baseDirectory);
  } else {
    baseDirectory = getenv("HOME");
    if (!baseDirectory || !baseDirectory[0])
      return false;
    directoryLength = snprintf(directory, directorySize,
                               "%s/.local/share/hells-tower", baseDirectory);
  }
#endif

  if (directoryLength < 0 || (size_t)directoryLength >= directorySize)
    return false;

  int filePathLength =
      snprintf(filePath, filePathSize, "%s/%s", directory, filename);
  return filePathLength >= 0 && (size_t)filePathLength < filePathSize;
}

static bool BuildSavePaths(char *directory, size_t directorySize,
                           char *savePath, size_t savePathSize) {
  return TowerBuildDataFilePath("save.txt", directory, directorySize,
                                savePath, savePathSize);
}

static const struct {
  const char *image, *music;
} bosses[] = {
    {"assets/bosses/Béhémot, Démon de l'Arrogance.png", "assets/musics/bosses/boss1.mp3"},
    {"assets/bosses/Séléna, Déesse Déchue.png", "assets/musics/bosses/boss2.mp3"},
    {"assets/bosses/Agarès, Démon de la Vengeance.png", "assets/musics/bosses/boss3.mp3"},
    {"assets/bosses/Dame Marika, Reine de la Cité des Cendres.png", "assets/musics/bosses/boss4.mp3"},
    {"assets/bosses/Malénia, Lame Consort.png", "assets/musics/bosses/the final battle.mp3"},
};

static int BossIndex(int floor) {
  if (floor >= 10 && floor <= 50 && floor % 10 == 0)
    return floor / 10 - 1;
  return -1;
}

static int Game(void) {
  int boss = BossIndex(currentFloor);
  if (boss == 0)
    return 9;
  if (boss == 1)
    return 8;
  if (boss == 2)
    return 7;
  if (boss == 3)
    return 6;
  if (boss == 4)
    return 5;
  if (currentGame < 0)
    return 0;
  return currentGame;
}

static const char *ItemGameName(int game) {
  switch (game) {
  case 0:
    return "BULLET_HELL";
  case 1:
    return "RING";
  case 2:
    return "MIRROR";
  case 3:
    return "SPIKES";
  case 4:
    return "SEALS";
  case 5:
    return "FINALBOSS";
  case 6:
    return "BOSS4";
  case 7:
    return "BOSS3";
  case 8:
    return "BOSS2";
  case 9:
    return "BOSS1";
  default:
    return NULL;
  }
}

static int GameIndex(const char *name) {
  for (int i = 0; i < 5; i++)
    if (strcmp(name, gameNames[i]) == 0)
      return i;
  return -1;
}

static void DifficultyRange(int floor, int *lo, int *hi) {
  int boss = BossIndex(floor);
  *lo = boss >= 0 ? 10 : 8;
  *hi = 10;
  if (boss < 0 && floor <= 45 && floor % 10 == 5) {
    *lo = 9;
  }
}

static void LoadMinigameHistory(void) {
  if (minigameHistoryLoaded)
    return;
  for (int i = 0; i < 50; i++)
    previousMinigameByFloor[i] = -1;

  char directory[SAVE_PATH_LEN], path[SAVE_PATH_LEN];
  if (!TowerBuildDataFilePath("minigame_history.txt", directory,
                              sizeof(directory), path, sizeof(path))) {
    TraceLog(LOG_WARNING, "Impossible de trouver l'historique des mini-jeux");
    minigameHistoryLoaded = true;
    return;
  }

  FILE *file = fopen(path, "r");
  if (!file) {
    if (errno != ENOENT)
      TraceLog(LOG_WARNING, "Impossible de lire l'historique des mini-jeux: %s",
               path);
    minigameHistoryLoaded = true;
    return;
  }

  int floor, game, scanned;
  while ((scanned = fscanf(file, "%d %d", &floor, &game)) == 2) {
    if (floor >= 1 && floor <= 50 && game >= 0 && game < 5)
      previousMinigameByFloor[floor - 1] = game;
  }
  if (scanned != EOF || ferror(file))
    TraceLog(LOG_WARNING, "Historique des mini-jeux invalide: %s", path);
  fclose(file);
  minigameHistoryLoaded = true;
}

static void SaveMinigameHistory(void) {
  char directory[SAVE_PATH_LEN], path[SAVE_PATH_LEN];
  char temporaryPath[SAVE_PATH_LEN];
  if (!TowerBuildDataFilePath("minigame_history.txt", directory,
                              sizeof(directory), path, sizeof(path)) ||
      (!DirectoryExists(directory) && MakeDirectory(directory) != 0 &&
       !DirectoryExists(directory))) {
    TraceLog(LOG_ERROR, "Impossible de creer l'historique des mini-jeux");
    return;
  }

  int pathLength =
      snprintf(temporaryPath, sizeof(temporaryPath), "%s.tmp", path);
  if (pathLength < 0 || (size_t)pathLength >= sizeof(temporaryPath)) {
    TraceLog(LOG_ERROR, "Chemin de l'historique des mini-jeux trop long");
    return;
  }

  FILE *file = fopen(temporaryPath, "w");
  if (!file) {
    TraceLog(LOG_ERROR, "Impossible d'ecrire l'historique des mini-jeux: %s",
             path);
    return;
  }

  bool success = true;
  for (int i = 0; i < 50; i++) {
    if (previousMinigameByFloor[i] >= 0 &&
        fprintf(file, "%d %d\n", i + 1, previousMinigameByFloor[i]) < 0) {
      success = false;
      break;
    }
  }
  if (fclose(file) != 0)
    success = false;
#ifdef _WIN32
  if (success && remove(path) != 0 && errno != ENOENT)
    success = false;
#endif
  if (success && rename(temporaryPath, path) == 0)
    return;
  remove(temporaryPath);
  TraceLog(LOG_ERROR, "Impossible de finaliser l'historique des mini-jeux: %s",
           path);
}

static bool ReadSaveLine(char **cursor, char *line, size_t lineSize) {
  if (!cursor || !*cursor || !line || lineSize == 0 || !(*cursor)[0])
    return false;
  char *start = *cursor;
  char *end = strchr(start, '\n');
  if (!end)
    end = start + strlen(start);
  size_t length = (size_t)(end - start);
  if (length && start[length - 1] == '\r')
    length--;
  if (length >= lineSize)
    return false;
  memcpy(line, start, length);
  line[length] = '\0';
  *cursor = *end == '\n' ? end + 1 : end;
  return true;
}

static char *FormatSave(void) {
  const char *game = "";
  if (currentGame >= 0)
    game = gameNames[currentGame];
  char *itemData = ItemsSave();
  if (!itemData)
    return NULL;
  size_t itemDataSize = strlen(itemData);
  size_t size = 256;
  const char *parts[] = {opponentPath, floorMusicPath, game, itemData};
  for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
    size_t partSize = strlen(parts[i]);
    if (partSize > SIZE_MAX - size) {
      free(itemData);
      TraceLog(LOG_ERROR, "Sauvegarde de la tour trop volumineuse");
      return NULL;
    }
    size += partSize;
  }
  char *out = malloc(size);
  if (!out) {
    free(itemData);
    TraceLog(LOG_ERROR, "Impossible d'allouer la sauvegarde de la tour");
    return NULL;
  }
  int written = snprintf(out, size, "%d %d %d %d %d %llu 6 %d %d\n",
                         currentFloor, tutorialSeen, currentDifficulty,
                         LevelsLevel(), LevelsXP(), sigridSeen,
                         playerCharacter, etris);
  if (written < 0 || (size_t)written >= size)
    goto save_failed;
  size_t used = (size_t)written;
  written = snprintf(out + used, size - used, "%s\n%s\n%s\n",
                     opponentPath, floorMusicPath, game);
  if (written < 0 || (size_t)written >= size - used)
    goto save_failed;
  used += (size_t)written;
  memcpy(out + used, itemData, itemDataSize);
  used += itemDataSize;
  out[used++] = '\n';
  out[used] = '\0';
  free(itemData);
  return out;

save_failed:
  free(itemData);
  free(out);
  TraceLog(LOG_ERROR, "Impossible de formater la sauvegarde de la tour");
  return NULL;
}

static uint32_t TowerRandom(void) {
  if (towerRandomState == 0)
    towerRandomState = (uint64_t)(GetTime() * 1000003) ^ 0x9E3779B97F4A7C15ULL;
  towerRandomState ^= towerRandomState >> 12;
  towerRandomState ^= towerRandomState << 25;
  towerRandomState ^= towerRandomState >> 27;
  return (uint32_t)((towerRandomState * 0x2545F4914F6CDD1DULL) >> 32);
}

static int PickMinigame(void) {
  LoadMinigameHistory();
  int previous = currentFloor >= 1 && currentFloor <= 50
                     ? previousMinigameByFloor[currentFloor - 1]
                     : -1;
  int game = BalancePickMinigame(TowerRandom(), previous);
  if (currentFloor >= 1 && currentFloor <= 50) {
    previousMinigameByFloor[currentFloor - 1] = game;
    SaveMinigameHistory();
  }
  return game;
}

static void PickFile(const char *dir, const char *ext, char *out) {
  FilePathList files = LoadDirectoryFilesEx(dir, ext, false);
  if (files.count > 0)
    snprintf(out, 512, "%s", files.paths[GetRandomValue(0, (int)files.count - 1)]);
  else
    TraceLog(LOG_WARNING, "Aucun fichier %s dans %s", ext, dir);
  UnloadDirectoryFiles(files);
}

static void PrepareFloor(void) {
  int boss = BossIndex(currentFloor);

  if (preparedFloor != currentFloor)
    currentDifficulty = 0;
  if (currentDifficulty == 0) {
    int lo, hi;
    DifficultyRange(currentFloor, &lo, &hi);
    currentDifficulty = GetRandomValue(lo, hi);
  }

  if (boss >= 0) {
    snprintf(opponentPath, 512, "%s", bosses[boss].image);
    snprintf(floorMusicPath, 512, "%s", bosses[boss].music);
    currentGame = -1;
    preparedFloor = currentFloor;
    return;
  }

  if (preparedFloor != currentFloor || currentGame < 0) {
    currentGame = PickMinigame();
    preparedFloor = currentFloor;
  }
  if (!FileExists(opponentPath)) {
    opponentPath[0] = '\0';
    PickFile("assets/monsters", ".png", opponentPath);
  }
  if (!FileExists(floorMusicPath)) {
    floorMusicPath[0] = '\0';
    PickFile("assets/musics/fights", ".mp3", floorMusicPath);
  }
}

bool LoadTowerProgress(void) {
  int floor = 0, seen = 0, diff = 0, level = 0, xp = 0, oldSigrid = 0, version = 0;
  int savedCharacter = 0, savedEtris = 0;
  unsigned long long savedSigrid = 0;
  int fields = 0;
  char header[96] = "", opp[512] = "", mus[512] = "", game[32] = "";
  const char *itemData = "";
  char saveDirectory[SAVE_PATH_LEN], savePath[SAVE_PATH_LEN];
  bool complete = false;
  free(savedText);
  savedText = NULL;
  etris = 0;
  preparedFloor = 0;

  char *saveData = NULL;
  bool savePathsFound = BuildSavePaths(saveDirectory, sizeof(saveDirectory),
                                       savePath, sizeof(savePath));
  if (savePathsFound) {
    saveData = LoadFileText(savePath);
    if (!saveData && FileExists(savePath))
      TraceLog(LOG_WARNING, "Impossible de lire la sauvegarde: %s", savePath);
  } else {
    TraceLog(LOG_WARNING, "Impossible de trouver le dossier de sauvegarde");
  }
  if (!saveData && FileExists("save.txt")) {
    TraceLog(LOG_INFO, "Chargement de l'ancienne sauvegarde locale save.txt");
    saveData = LoadFileText("save.txt");
    if (!saveData)
      TraceLog(LOG_WARNING, "Impossible de lire l'ancienne sauvegarde save.txt");
  }
  if (saveData) {
    char *cursor = saveData;
    if (ReadSaveLine(&cursor, header, sizeof(header)))
      fields = sscanf(header, "%d %d %d %d %d %llu %d %d %d", &floor, &seen,
                      &diff, &level, &xp, &savedSigrid, &version,
                      &savedCharacter, &savedEtris);
    complete = ReadSaveLine(&cursor, opp, sizeof(opp)) &&
               ReadSaveLine(&cursor, mus, sizeof(mus)) &&
               ReadSaveLine(&cursor, game, sizeof(game));
    if (complete) {
      cursor[strcspn(cursor, "\r\n")] = '\0';
      itemData = cursor;
    }
  }

  ItemsLoad(itemData);
  UnloadFileText(saveData);

  bool known = fields >= 2 && floor > 0 && (seen == 0 || seen == 1);
  bool characterValid =
      version == 3 ? fields >= 8 && (savedCharacter == 1 || savedCharacter == 2)
      : version == 4 || version == 5 || version == 6
          ? fields >= 8 && savedCharacter >= 0 && savedCharacter <= 5
                     : true;
  if (fields == 6)
    oldSigrid = (int)savedSigrid;
  LevelsLoad(fields >= 5 ? level : 0, fields >= 5 ? xp : 0);
  playerCharacter = characterValid &&
                            (version == 3 || version == 4 || version == 5 ||
                             version == 6)
                        ? savedCharacter
                        : 1;
  currentFloor = known ? floor : 1;
  if (known && characterValid && playerCharacter == 0)
    ItemsMigrateQuestRewards(currentFloor);
  tutorialSeen = known && seen == 1;
  sigridSeen =
      fields >= 7 &&
              (version == 2 || version == 3 || version == 4 || version == 5 ||
               version == 6)
                   ? savedSigrid
               : fields == 6 && oldSigrid > 0 && oldSigrid < 64
                   ? 1ULL << oldSigrid
                   : 0;
  currentDifficulty = 0;
  currentGame = -1;
  opponentPath[0] = floorMusicPath[0] = '\0';

  if (known && characterValid && fields >= 3 && complete) {
    if (BossIndex(currentFloor) >= 0) {
      currentDifficulty = 0;
      currentGame = -1;
      preparedFloor = 0;
      PrepareFloor();
      if (FileExists(opponentPath) && FileExists(floorMusicPath)) {
        etris = version == 6 && fields >= 9 && savedEtris > 0 ? savedEtris : 0;
        savedText = FormatSave();
      }
    } else {
      int gameIndex = GameIndex(game);
      if (gameIndex >= 0 && diff >= 1 && diff <= 10 &&
          FileExists(opp) && FileExists(mus)) {
        etris = version == 6 && fields >= 9 && savedEtris > 0 ? savedEtris : 0;
        currentDifficulty = diff;
        currentGame = gameIndex;
        preparedFloor = currentFloor;
        strcpy(opponentPath, opp);
        strcpy(floorMusicPath, mus);
        savedText = FormatSave();
      }
    }
  }
  if (savedText)
    ItemsPrepareForFloor(currentFloor, ItemGameName(Game()),
                         BossIndex(currentFloor) >= 0);
  etrisDisplay = (float)etris;
  return savedText != NULL;
}

bool SaveTowerProgress(void) {
  char saveDirectory[SAVE_PATH_LEN], savePath[SAVE_PATH_LEN];

  if (playerCharacter < 0 || playerCharacter > 5) {
    TraceLog(LOG_ERROR, "Impossible de sauvegarder sans personnage choisi");
    return false;
  }

  PrepareFloor();
  if (!opponentPath[0] || !floorMusicPath[0]) {
    TraceLog(LOG_ERROR, "Impossible de sauvegarder l'etage %d", currentFloor);
    return false;
  }

  if (!BuildSavePaths(saveDirectory, sizeof(saveDirectory), savePath,
                      sizeof(savePath)) ||
      (!DirectoryExists(saveDirectory) && MakeDirectory(saveDirectory) != 0 &&
       !DirectoryExists(saveDirectory))) {
    TraceLog(LOG_ERROR, "Impossible de creer le dossier de sauvegarde");
    return false;
  }

  char *text = FormatSave();
  if (!text)
    return false;
  if (!SaveFileText(savePath, text)) {
    TraceLog(LOG_ERROR, "Impossible d'ecrire la sauvegarde: %s", savePath);
    free(text);
    return false;
  }
  free(savedText);
  savedText = text;
  return true;
}

bool DeleteTowerProgressFile(void) {
  char saveDirectory[SAVE_PATH_LEN], savePath[SAVE_PATH_LEN];
  if (!BuildSavePaths(saveDirectory, sizeof(saveDirectory), savePath,
                      sizeof(savePath))) {
    TraceLog(LOG_ERROR, "Impossible de trouver le dossier de sauvegarde");
    return false;
  }
  if (FileExists(savePath) && remove(savePath) != 0) {
    TraceLog(LOG_ERROR, "Impossible de supprimer la sauvegarde: %s", savePath);
    return false;
  }
  if (FileExists("save.txt") && remove("save.txt") != 0) {
    TraceLog(LOG_ERROR, "Impossible de supprimer l'ancienne sauvegarde save.txt");
    return false;
  }
  return true;
}

void ResetTowerProgress(void) {
  currentFloor = 1;
  etris = 0;
  etrisDisplay = 0;
  currentDifficulty = 0;
  currentGame = -1;
  preparedFloor = 0;
  playerCharacter = -1;
  towerStarted = tutorialSeen = false;
  resultState = GAME_OFF;
  exitRequested = false;
  sigridSeen = 0;
  opponentPath[0] = floorMusicPath[0] = '\0';
  free(savedText);
  savedText = NULL;
  LevelsLoad(0, 0);
  ItemsReset();
  ResetTower();
}

bool TowerProgressNeedsSaving(void) {
  char *now = FormatSave();
  if (!now)
    return true;
  bool needsSaving = !savedText || strcmp(now, savedText) != 0;
  free(now);
  return needsSaving;
}

bool SetTowerPlayerCharacter(int character) {
  if (character < 0 || character > 5)
    return false;
  playerCharacter = character;
  return true;
}

int TowerPlayerCharacter(void) { return playerCharacter; }

bool TowerTutorialSeen(void) { return tutorialSeen; }

void MarkTowerTutorialSeen(void) { tutorialSeen = true; }
int TowerCurrentFloor(void) { return currentFloor; }
bool TowerBossFloorCompleted(int floor) {
  return currentFloor > floor ||
         (currentFloor == floor && resultState == GAME_WON &&
          resultFloor == floor);
}
int TowerEtris(void) { return etris; }
void TowerAddEtris(long long amount) {
  if (amount > 0)
    etris = amount > (long long)INT_MAX - etris
                ? INT_MAX
                : etris + (int)amount;
}
bool TowerSpendEtris(int amount) {
  if (amount < 0 || etris < amount)
    return false;
  etris -= amount;
  return true;
}

void TowerDrawEtrisHud(bool alwaysVisible) {
  bool changing = fabsf((float)etris - etrisDisplay) >= 0.5f;
  if (!alwaysVisible && !changing) {
    etrisDisplay = (float)etris;
    return;
  }

  if (!etrisTextureTried) {
    etrisTextureTried = true;
    etrisTexture = LoadTexture("assets/Etris.png");
    if (etrisTexture.id == 0)
      TraceLog(LOG_ERROR, "Logo assets/Etris.png introuvable");
  }

  if (etrisDisplay < 0)
    etrisDisplay = (float)etris;
  else {
    float difference = (float)etris - etrisDisplay;
    if (fabsf(difference) < 0.5f)
      etrisDisplay = (float)etris;
    else
      etrisDisplay += difference * (1 - expf(-GetFrameTime() * 8));
  }

  int shown = (int)(etrisDisplay + (etrisDisplay >= 0 ? 0.5f : -0.5f));
  float logoSize = (float)GameUiPixels(40);
  int amountSize = GameUiPixels(24);
  int amountWidth = MeasureText(TextFormat("%d", shown), amountSize);
  float totalWidth = logoSize + GameUiPixels(10) + amountWidth;
  float x = GetScreenWidth() - totalWidth - GameUiPixels(28);
  float y = GameUiPixels(20);

  if (etrisTexture.id != 0) {
    Rectangle source = {0, 0, (float)etrisTexture.width,
                        (float)etrisTexture.height};
    Rectangle destination = {x, y, logoSize, logoSize};
    DrawTexturePro(etrisTexture, source, destination, (Vector2){0, 0}, 0,
                   WHITE);
  }
  DrawText(TextFormat("%d", shown),
           (int)(x + logoSize + GameUiPixels(10)),
           (int)(y + (logoSize - amountSize) / 2), amountSize,
           (Color){245, 220, 130, 255});
}
bool TowerSigridDialogSeen(int floor) {
  return floor > 0 && floor < 64 && (sigridSeen & (1ULL << floor));
}
void MarkTowerSigridDialogSeen(int floor) {
  if (floor > 0 && floor < 64)
    sigridSeen |= 1ULL << floor;
}

static void StopMusic(void) {
  if (floorMusic.stream.buffer) {
    StopMusicStream(floorMusic);
    UnloadMusicStream(floorMusic);
  }
  floorMusic = (Music){0};
  floorMusicPausedForLoading = false;
  musicAttempted = false;
}

static void StartMusic(void) {
  if (musicAttempted)
    return;
  musicAttempted = true;
  if (floorMusicPath[0])
    floorMusic = LoadMusicStream(floorMusicPath);
  if (floorMusic.stream.buffer)
    PlayMusicStream(floorMusic);
  else
    TraceLog(LOG_WARNING, "Musique introuvable: %s", floorMusicPath);
}

static void StartGame(void) {
  gameTime = 0;
  gameRewarded = false;
  resultState = GAME_OFF;
  resultFloor = currentFloor;
  resultSelection = 0;
  switch (Game()) {
  case 0:
    BulletHellStart(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  case 1:
    RingsStart(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  case 3:
    SpikesStart(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  case 4:
    SealsStart(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  case 5:
    FinalBossStart(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  case 6:
    Boss4Start(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  case 7:
    Boss3Start(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  case 8:
    Boss2Start(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  case 9:
    Boss1Start(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  default:
    MirrorStart(currentFloor, FunctionalDifficulty(currentDifficulty));
  }
}

static void StartFloor(void) {
  ItemsClearRewards();
  PrepareFloor();
  int game = Game();
  const char *itemGame = ItemGameName(game);
  ItemsPrepareForFloor(currentFloor, itemGame,
                       BossIndex(currentFloor) >= 0);
  ItemsApplyForGame(itemGame, BossIndex(currentFloor) >= 0);
  CharacterApplyForGame(playerCharacter);
  StopMusic();
  StartMusic();
  StartGame();
}

static TowerGameState FloorGameState(void);

static void UpdateFloorGame(Rectangle zone) {
  if (FloorGameState() != GAME_PLAYING)
    return;
  switch (Game()) {
  case 0:
    BulletHellUpdate(zone);
    break;
  case 1:
    RingsUpdate(zone);
    break;
  case 3:
    SpikesUpdate(zone);
    break;
  case 4:
    SealsUpdate(zone);
    break;
  case 5:
    FinalBossUpdate(zone);
    break;
  case 6:
    Boss4Update(zone);
    break;
  case 7:
    Boss3Update(zone);
    break;
  case 8:
    Boss2Update(zone);
    break;
  case 9:
    Boss1Update(zone);
    break;
  default:
    MirrorUpdate(zone);
  }
}

static TowerGameState FloorGameState(void) {
  switch (Game()) {
  case 0:
    switch (BulletHellState()) {
    case HELL_PLAYING:
      return GAME_PLAYING;
    case HELL_WON:
      return GAME_WON;
    case HELL_LOST:
      return GAME_LOST;
    default:
      return GAME_OFF;
    }
  case 1:
    switch (RingsState()) {
    case RINGS_PLAYING:
      return GAME_PLAYING;
    case RINGS_WON:
      return GAME_WON;
    case RINGS_LOST:
      return GAME_LOST;
    default:
      return GAME_OFF;
    }
  case 3:
    switch (SpikesState()) {
    case SPIKES_PLAYING:
      return GAME_PLAYING;
    case SPIKES_WON:
      return GAME_WON;
    case SPIKES_LOST:
      return GAME_LOST;
    default:
      return GAME_OFF;
    }
  case 4:
    switch (SealsState()) {
    case SEALS_PLAYING:
      return GAME_PLAYING;
    case SEALS_WON:
      return GAME_WON;
    case SEALS_LOST:
      return GAME_LOST;
    default:
      return GAME_OFF;
    }
  case 5:
    switch (FinalBossState()) {
    case FINALBOSS_PLAYING:
      return GAME_PLAYING;
    case FINALBOSS_WON:
      return GAME_WON;
    case FINALBOSS_LOST:
      return GAME_LOST;
    default:
      return GAME_OFF;
    }
  case 6:
    switch (Boss4State()) {
    case BOSS4_PLAYING:
      return GAME_PLAYING;
    case BOSS4_WON:
      return GAME_WON;
    case BOSS4_LOST:
      return GAME_LOST;
    default:
      return GAME_OFF;
    }
  case 7:
    switch (Boss3State()) {
    case BOSS3_PLAYING:
      return GAME_PLAYING;
    case BOSS3_WON:
      return GAME_WON;
    case BOSS3_LOST:
      return GAME_LOST;
    default:
      return GAME_OFF;
    }
  case 8:
    switch (Boss2State()) {
    case BOSS2_PLAYING:
      return GAME_PLAYING;
    case BOSS2_WON:
      return GAME_WON;
    case BOSS2_LOST:
      return GAME_LOST;
    default:
      return GAME_OFF;
    }
  case 9:
    switch (Boss1State()) {
    case BOSS1_PLAYING:
      return GAME_PLAYING;
    case BOSS1_WON:
      return GAME_WON;
    case BOSS1_LOST:
      return GAME_LOST;
    default:
      return GAME_OFF;
    }
  default:
    switch (MirrorState()) {
    case MIRROR_PLAYING:
      return GAME_PLAYING;
    case MIRROR_WON:
      return GAME_WON;
    case MIRROR_LOST:
      return GAME_LOST;
    default:
      return GAME_OFF;
    }
  }
}

static int FloorGameDamageCount(void) {
  switch (Game()) {
  case 0:
    return BulletHellDamageCount();
  case 1:
    return RingsDamageCount();
  case 3:
    return SpikesDamageCount();
  case 4:
    return SealsDamageCount();
  case 5:
    return FinalBossDamageCount();
  case 6:
    return Boss4DamageCount();
  case 7:
    return Boss3DamageCount();
  case 8:
    return Boss2DamageCount();
  case 9:
    return Boss1DamageCount();
  default:
    return MirrorDamageCount();
  }
}

static int DrawResultButton(const char *label, Rectangle box, int index) {
  float scale = GameUiScale();
  bool hover = CheckCollisionPointRec(GetMousePosition(), box);
  ButtonSoundId soundId =
      index == 0 ? BUTTON_SOUND_TOWER_RETRY : BUTTON_SOUND_TOWER_EXIT;
  UpdateButtonHoverSound(soundId, hover);
  if (hover)
    resultSelection = index;
  bool selected = resultSelection == index;

  Color fill = selected ? (Color){75, 58, 28, 255} : (Color){25, 25, 30, 245};
  Color edge = selected ? (Color){245, 205, 95, 255} : RAYWHITE;
  DrawRectangleRec(box, fill);
  DrawRectangleLinesEx(box, (selected ? 3 : 1) * scale, edge);
  int size = GameUiPixels(24);
  DrawText(label, (int)(box.x + (box.width - MeasureText(label, size)) / 2),
           (int)(box.y + (box.height - size) / 2), size, RAYWHITE);
  bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (clicked)
    PlayButtonClickSound();
  return clicked;
}

static int DrawResultMenu(TowerGameState state) {
  bool won = state == GAME_WON;
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  float scale = GameUiScale();
  float maxScale = scale > 1 ? scale : 1;
  float zoneW = sw * 0.58f, zoneH = sh * 0.56f;
  float w = zoneW * 0.9f, h = zoneH * 0.88f;
  if (w > 460 * maxScale)
    w = 460 * maxScale;
  if (h > 250 * maxScale)
    h = 250 * maxScale;
  float x = (sw - zoneW) / 2 + (zoneW - w) / 2;
  float y = sh * 0.94f - zoneH + (zoneH - h) / 2;
  Rectangle panel = {x, y, w, h};
  bool finalFloor = won && resultFloor >= 50;
  Rectangle first = finalFloor
                        ? (Rectangle){x + w * 0.09f, y + h * 0.52f,
                                      w * 0.82f, h * 0.24f}
                        : (Rectangle){x + w * 0.09f, y + h * 0.36f,
                                      w * 0.82f, h * 0.24f};
  Rectangle second = finalFloor
                         ? (Rectangle){0, 0, 0, 0}
                         : (Rectangle){x + w * 0.09f, y + h * 0.68f,
                                       w * 0.82f, h * 0.24f};
  const char *title = won ? "VICTOIRE" : "ÉCHEC";
  const char *firstLabel =
      won && resultFloor < 50 ? "Étage suivant" : won ? "Terminer" : "Recommencer";
  int titleSize = GameUiPixels(32), titleY = (int)(y + h * 0.09f);

  if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_RIGHT))
    resultSelection = 1;
  if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_LEFT))
    resultSelection = 0;

  DrawRectangleRec(panel, (Color){12, 12, 16, 245});
  DrawRectangleLinesEx(panel, 3 * scale, (Color){225, 185, 40, 255});
  DrawText(title, (int)(x + (w - MeasureText(title, titleSize)) / 2), titleY, titleSize,
           (Color){245, 220, 130, 255});

  int choice = -1;
  if (won && resultFloor >= 50) {
    if (DrawResultButton(firstLabel, first, 0))
      choice = 1;
  } else {
    if (DrawResultButton(firstLabel, first, 0))
      choice = 0;
  }
  if (DrawResultButton("Sortir de la tour", second, 1))
    choice = 1;
  if (choice < 0 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
    choice = resultSelection;
  return choice;
}

static void ExitTower(void) {
  if (resultState == GAME_WON && currentFloor <= 50) {
    PrepareFloor();
    ItemsPrepareForFloor(currentFloor, ItemGameName(Game()),
                         BossIndex(currentFloor) >= 0);
  }
  ItemsClearRewards();
  exitRequested = true;
  towerStarted = false;
  resultState = GAME_OFF;
  opponentPath[0] = floorMusicPath[0] = '\0';
  StopMusic();
}

void Tower(void) {
  if (!towerStarted) {
    if (currentFloor < 1)
      currentFloor = 1;
    towerStarted = true;
    if (resultState == GAME_OFF)
      StartFloor();
    else
      StartMusic();
  } else if (!musicAttempted) {
    StartMusic();
  }

  int displayedFloor = resultState != GAME_OFF ? resultFloor : currentFloor;
  const char *bossText = BossIndex(displayedFloor) >= 0 ? "  |  BOSS" : "";
  int hudSize = GameUiPixels(24);
  DrawText(TextFormat("ETAGE %d  |  DIFFICULTE %d%s", displayedFloor,
                      currentDifficulty, bossText),
           GameUiPixels(24), GameUiPixels(24), hudSize, RAYWHITE);
  GameZoneDrawActiveModifiers(
      (Rectangle){GameUiPixels(24), 0,
                  (float)GetScreenWidth() - 2 * GameUiPixels(24),
                  (float)GetScreenHeight()},
      GameUiPixels(54));

  TowerGameState before = resultState != GAME_OFF ? resultState : FloorGameState();
  DrawGameZone(resultState == GAME_OFF ? UpdateFloorGame : NULL,
               resultState == GAME_OFF ? FloorGameDamageCount : NULL,
               opponentPath[0] ? opponentPath : NULL);
  TowerGameState after = resultState != GAME_OFF ? resultState : FloorGameState();
  if (before == GAME_PLAYING) {
    gameTime += GetFrameTime();
    if (!gameRewarded && (after == GAME_WON || after == GAME_LOST)) {
      resultState = after;
      resultFloor = currentFloor;
      int oldLevel = LevelsLevel(), oldXP = LevelsXP();
      int multiplier = 1;
      if (after == GAME_WON)
        multiplier = BossIndex(currentFloor) >= 0 ? currentFloor : 3;
      double reward = (int)(gameTime + 0.5f) * (double)currentFloor *
                      currentDifficulty * multiplier *
                      CharacterExperienceMultiplier(playerCharacter);
      int gainedXP = reward > 2147483647 ? 2147483647 : (int)reward;
      LevelsAddXP(gainedXP);
      for (int i = oldLevel; i < LevelsLevel(); i++) {
        int itemCount = CharacterItemsPerLevel(playerCharacter);
        if (itemCount > 5)
          itemCount = 5;
        for (int j = 0; j < itemCount; j++)
          ItemsGrantForLevelUp();
        TowerAddEtris(25LL + ((long long)i + 1) * 20);
      }
      double attemptEtrisReward =
          BalanceTowerAttemptEtrisReward(gameTime, currentDifficulty) *
          FunctionalGet()->etrisMultiplier;
      if (after == GAME_WON)
        attemptEtrisReward *= CharacterEtrisMultiplier(playerCharacter);
      TowerAddEtris((long long)(attemptEtrisReward + 0.5));
      if (after == GAME_WON) {
        ItemsConsumeEquippedUses();
        double etrisReward = (20.0 + (double)currentFloor * 5) *
                             (0.75 + currentDifficulty * 0.25);
        if (BossIndex(currentFloor) >= 0)
          etrisReward *= 4;
        etrisReward *= CharacterEtrisMultiplier(playerCharacter) *
                       FunctionalGet()->etrisMultiplier;
        TowerAddEtris((long long)etrisReward);
      }
      if (after == GAME_WON && BossIndex(currentFloor) >= 0) {
        AchievementsUnlock((AchievementId)(ACHIEVEMENT_FLOOR_10 +
                                           BossIndex(currentFloor)));
        AchievementsRecordBossVictory(playerCharacter, currentFloor,
                                      FloorGameDamageCount() == 0);
      }
      if (after == GAME_WON) {
        switch (currentFloor) {
        case 1:
          AchievementsUnlock(ACHIEVEMENT_FIRST_VICTORY);
          break;
        case 5:
          AchievementsUnlock(ACHIEVEMENT_FLOOR_5);
          break;
        case 15:
          AchievementsUnlock(ACHIEVEMENT_FLOOR_15);
          break;
        case 20:
          AchievementsUnlockTitle(TITLE_BOSS_HUNTER);
          break;
        case 25:
          AchievementsUnlock(ACHIEVEMENT_FLOOR_25);
          break;
        case 30:
          AchievementsUnlockTitle(TITLE_VENGEANCE);
          break;
        case 35:
          AchievementsUnlock(ACHIEVEMENT_FLOOR_35);
          break;
        case 40:
          AchievementsUnlockTitle(TITLE_ASHEN_CROWN);
          break;
        case 45:
          AchievementsUnlock(ACHIEVEMENT_FLOOR_45);
          break;
        }
      }
      if (LevelsLevel() >= 10)
        AchievementsUnlock(ACHIEVEMENT_LEVEL_10);
      if (after == GAME_WON && currentFloor == 10)
        AchievementsUnlockTitle(TITLE_FIRST_ASCENT);
      if (after == GAME_WON && currentFloor == 50)
        AchievementsUnlockTitle(TITLE_TOWER_CONQUEROR);
      if (after == GAME_WON && BossIndex(currentFloor) >= 0 &&
          FloorGameDamageCount() == 0) {
        AchievementsUnlock(ACHIEVEMENT_UNTOUCHED_BOSS);
        AchievementsUnlockTitle(TITLE_UNTOUCHED);
      }
      if (after == GAME_WON && playerCharacter == 0) {
        if (currentFloor == 5) {
          ItemsGrantQuestLetter();
          AchievementsUnlock(ACHIEVEMENT_FOUND_LETTER);
        }
        if (BossIndex(currentFloor) >= 0)
          ItemsGrantBossSeal(currentFloor);
      }
      LevelsShowReward(oldLevel, oldXP, gainedXP);
      gameRewarded = true;
      if (after == GAME_WON && currentFloor < INT_MAX) {
        currentFloor++;
        bool nextIsBoss = BossIndex(currentFloor) >= 0;
        ItemsPrepareForFloor(
            currentFloor, nextIsBoss ? ItemGameName(Game()) : NULL,
            nextIsBoss);
      }
    }
  }
  LevelsDrawNotification();

  if (resultState == GAME_OFF) {
    ItemsDrawReward();
    return;
  }

  int choice = DrawResultMenu(resultState);
  if (choice == 1) {
    resultState = GAME_OFF;
    ExitTower();
  } else if (choice == 0 && resultState == GAME_WON) {
    if (resultFloor >= 50)
      ExitTower();
    else
      StartFloor();
  } else if (choice == 0 && resultState == GAME_LOST) {
    StartGame();
  }
  ItemsDrawReward();
}

void UpdateTowerMusic(void) {
  if (!floorMusic.stream.buffer)
    return;

  if (LoadingScreenActive()) {
    if (!floorMusicPausedForLoading)
      PauseMusicStream(floorMusic);
    floorMusicPausedForLoading = true;
  } else {
    if (floorMusicPausedForLoading)
      ResumeMusicStream(floorMusic);
    floorMusicPausedForLoading = false;
    UpdateMusicStream(floorMusic);
  }
}

void ResetTower(void) {
  StopMusic();
  ItemsClearRewards();
  FunctionalReset();
  towerStarted = false;
  resultState = GAME_OFF;
  exitRequested = false;
  resultSelection = 0;
  BulletHellUnload();
  MirrorUnload();
  RingsUnload();
  SealsUnload();
  Boss1Unload();
  Boss2Unload();
  Boss3Unload();
  SpikesUnload();
  FinalBossUnload();
  Boss4Unload();
  UnloadGameZone();
}

bool TowerTakeExitRequest(void) {
  bool requested = exitRequested;
  exitRequested = false;
  return requested;
}

bool TowerResultActive(void) { return resultState != GAME_OFF; }