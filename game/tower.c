#include "tower.h"
#include "buttons/button_sounds.h"
#include "../loading_screen/loading_screen.h"
#include "game_zone/bullet hell/bullethell.h"
#include "game_zone/mirror/mirror.h"
#include "game_zone/rings/rings.h"
#include "game_zone/seals/seals.h"
#include "game_zone/balance.h"
#include "game_zone/zone.h"
#include "achievements/achievements.h"
#include "characters/characters.h"
#include "items/functionnal/functionnal.h"
#include "items/items.h"
#include "levels/levels.h"
#include "raylib.h"
#include "ui_scale.h"
#include <limits.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_LEN 8192
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
static char opponentPath[512], floorMusicPath[512], savedText[SAVE_LEN];
static Music floorMusic;
static bool floorMusicPausedForLoading;
static float gameTime;
static bool gameRewarded;
static TowerGameState resultState;
static int resultSelection;

static const char *gameNames[3] = {"bullethell", "rings", "mirror"};

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
    {"assets/bosses/Dame Marika, Celle qui Régna.png", "assets/musics/bosses/boss4.mp3"},
    {"assets/bosses/Malénia, Lame Consort.png", "assets/musics/bosses/the final battle.mp3"},
};

static int BossIndex(int floor) {
  if (floor >= 10 && floor <= 50 && floor % 10 == 0)
    return floor / 10 - 1;
  return -1;
}

static int Game(void) {
  if (BossIndex(currentFloor) >= 0)
    return 3;
  if (currentGame < 0)
    return 0;
  return currentGame;
}

static int GameIndex(const char *name) {
  for (int i = 0; i < 3; i++)
    if (strcmp(name, gameNames[i]) == 0)
      return i;
  return -1;
}

static void DifficultyRange(int floor, int *lo, int *hi) {
  int boss = BossIndex(floor);
  *lo = 1;
  *hi = 6;
  if (boss == 4) {
    *lo = 10;
    *hi = 10;
  } else if (boss >= 0) {
    *lo = 7;
    *hi = 9;
  } else if (floor <= 45 && floor % 10 == 5) {
    *lo = 7;
    *hi = 7;
  }
}

static void FormatSave(char *out) {
  const char *game = "";
  if (currentGame >= 0)
    game = gameNames[currentGame];
  snprintf(out, SAVE_LEN, "%d %d %d %d %d %llu 6 %d %d\n", currentFloor,
           tutorialSeen, currentDifficulty, LevelsLevel(), LevelsXP(),
           sigridSeen, playerCharacter, etris);
  size_t used = strlen(out);
  snprintf(out + used, SAVE_LEN - used, "%s\n%s\n%s\n",
           opponentPath, floorMusicPath, game);
  used = strlen(out);
  ItemsSave(out + used, SAVE_LEN - used);
  strncat(out, "\n", SAVE_LEN - strlen(out) - 1);
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
  return (int)(TowerRandom() % 3);
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
  int fields = 0, lo, hi;
  char opp[512] = "", mus[512] = "", game[32] = "";
  char itemData[SAVE_LEN] = "";
  char saveDirectory[SAVE_PATH_LEN], savePath[SAVE_PATH_LEN];
  bool complete = false;
  etris = 0;
  preparedFloor = 0;

  FILE *file = NULL;
  bool savePathsFound = BuildSavePaths(saveDirectory, sizeof(saveDirectory),
                                       savePath, sizeof(savePath));
  if (savePathsFound) {
    file = fopen(savePath, "r");
    if (!file && FileExists(savePath))
      TraceLog(LOG_WARNING, "Impossible de lire la sauvegarde: %s", savePath);
  } else {
    TraceLog(LOG_WARNING, "Impossible de trouver le dossier de sauvegarde");
  }
  if (!file && FileExists("save.txt")) {
    TraceLog(LOG_INFO, "Chargement de l'ancienne sauvegarde locale save.txt");
    file = fopen("save.txt", "r");
  }
  if (file) {
    char header[96];
    if (fgets(header, sizeof(header), file))
      fields = sscanf(header, "%d %d %d %d %d %llu %d %d %d", &floor, &seen,
                      &diff, &level, &xp, &savedSigrid, &version,
                      &savedCharacter, &savedEtris);
    if (fgets(opp, 512, file) && fgets(mus, 512, file) && fgets(game, 32, file))
      complete = true;
    if (fgets(itemData, sizeof(itemData), file))
      itemData[strcspn(itemData, "\r\n")] = '\0';
    fclose(file);
  }

  ItemsLoad(itemData);
  opp[strcspn(opp, "\r\n")] = '\0';
  mus[strcspn(mus, "\r\n")] = '\0';
  game[strcspn(game, "\r\n")] = '\0';

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
  opponentPath[0] = floorMusicPath[0] = savedText[0] = '\0';

  DifficultyRange(currentFloor, &lo, &hi);
  int gameIndex = GameIndex(game);
  bool gameOk = BossIndex(currentFloor) >= 0 ? game[0] == '\0' : gameIndex >= 0;

  if (known && characterValid && fields >= 3 && complete && gameOk &&
      diff >= lo && diff <= hi && FileExists(opp) && FileExists(mus)) {
    etris = version == 6 && fields >= 9 && savedEtris > 0 ? savedEtris : 0;
    currentDifficulty = diff;
    currentGame = gameIndex;
    preparedFloor = currentFloor;
    strcpy(opponentPath, opp);
    strcpy(floorMusicPath, mus);
    FormatSave(savedText);
  }
  etrisDisplay = (float)etris;
  return savedText[0] != '\0';
}

bool SaveTowerProgress(void) {
  char text[SAVE_LEN];
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

  FormatSave(text);
  if (!SaveFileText(savePath, text)) {
    TraceLog(LOG_ERROR, "Impossible d'ecrire la sauvegarde: %s", savePath);
    return false;
  }
  strcpy(savedText, text);
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
  opponentPath[0] = floorMusicPath[0] = savedText[0] = '\0';
  LevelsLoad(0, 0);
  ItemsReset();
  ResetTower();
}

bool TowerProgressNeedsSaving(void) {
  char now[SAVE_LEN];
  FormatSave(now);
  return strcmp(now, savedText) != 0;
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
  resultSelection = 0;
  switch (Game()) {
  case 0:
    BulletHellStart(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  case 1:
    RingsStart(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  case 3:
    SealsStart(currentFloor, FunctionalDifficulty(currentDifficulty));
    break;
  default:
    MirrorStart(currentFloor, FunctionalDifficulty(currentDifficulty));
  }
}

static void StartFloor(void) {
  ItemsClearRewards();
  PrepareFloor();
  int game = Game();
  const char *itemGame = game == 0 ? "BULLET_HELL"
                         : game == 1 ? "RING"
                         : game == 2 ? "MIRROR"
                         : game == 3 ? "SEALS"
                                     : NULL;
  ItemsApplyForGame(itemGame);
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
    SealsUpdate(zone);
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
    return SealsDamageCount();
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
  bool finalFloor = won && currentFloor >= 50;
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
      won && currentFloor < 50 ? "Étage suivant" : won ? "Terminer" : "Recommencer";
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
  if (won && currentFloor >= 50) {
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

  const char *bossText = BossIndex(currentFloor) >= 0 ? "  |  BOSS" : "";
  int hudSize = GameUiPixels(24);
  DrawText(TextFormat("ETAGE %d  |  DIFFICULTE %d%s", currentFloor,
                      currentDifficulty, bossText),
           GameUiPixels(24), GameUiPixels(24), hudSize, RAYWHITE);

  TowerGameState before = resultState != GAME_OFF ? resultState : FloorGameState();
  DrawGameZone(resultState == GAME_OFF ? UpdateFloorGame : NULL,
               resultState == GAME_OFF ? FloorGameDamageCount : NULL,
               opponentPath[0] ? opponentPath : NULL);
  TowerGameState after = resultState != GAME_OFF ? resultState : FloorGameState();
  if (before == GAME_PLAYING) {
    gameTime += GetFrameTime();
    if (!gameRewarded && (after == GAME_WON || after == GAME_LOST)) {
      resultState = after;
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
      if (after == GAME_WON && BossIndex(currentFloor) >= 0)
        AchievementsUnlock((AchievementId)(ACHIEVEMENT_FLOOR_10 +
                                           BossIndex(currentFloor)));
      if (after == GAME_WON && currentFloor == 10)
        AchievementsUnlockTitle(TITLE_FIRST_ASCENT);
      if (after == GAME_WON && currentFloor == 50)
        AchievementsUnlockTitle(TITLE_TOWER_CONQUEROR);
      if (after == GAME_WON && BossIndex(currentFloor) >= 0 &&
          FloorGameDamageCount() == 0)
        AchievementsUnlockTitle(TITLE_UNTOUCHED);
      if (after == GAME_WON && playerCharacter == 0) {
        if (currentFloor == 5)
          ItemsGrantQuestLetter();
        if (BossIndex(currentFloor) >= 0)
          ItemsGrantBossSeal(currentFloor);
      }
      LevelsShowReward(oldLevel, oldXP, gainedXP);
      gameRewarded = true;
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
    if (currentFloor >= 50)
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
  UnloadGameZone();
}

bool TowerTakeExitRequest(void) {
  bool requested = exitRequested;
  exitRequested = false;
  return requested;
}

bool TowerResultActive(void) { return resultState != GAME_OFF; }