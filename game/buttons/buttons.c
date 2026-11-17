#include "buttons.h"
#include "../game_zone/bullet hell/bullethell.h"
#include "../game_zone/mirror/mirror.h"
#include "../game_zone/rings/rings.h"
#include "../game_zone/seals/seals.h"
#include "../game_zone/zone.h"
#include "../inventory/inventory.h"
#include "../items/functionnal/functionnal.h"
#include "../items/items.h"
#include "../pause_menu/pause_menu.h"
#include "../tower.h"
#include "../../loading_screen/loading_screen.h"
#include "button_sounds.h"
#include "raylib.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

extern bool game_running;

static bool menuRequested, subscreenActive, trainingActive, trainingSelectionActive;
static bool inventoryActive;
static bool quitConfirmation, returnConfirmation, saveOk;
static bool musicPausedForLoading;
static float saveTime;
static Font font;
static Texture2D bandTex;
static Music music;
static char monsterPath[512];
static int trainingGame;

static const char *trainingNames[] = {"BULLET HELL", "RINGS", "MIRROR", "SEALS"};

typedef struct {
  const char *label;
  void (*action)(void);
  float inward;
} Button;

static bool PickFile(const char *dir, const char *ext, char *out) {
  FilePathList files = LoadDirectoryFilesEx(dir, ext, false);
  bool found = files.count > 0;
  if (found)
    snprintf(out, 512, "%s", files.paths[GetRandomValue(0, (int)files.count - 1)]);
  UnloadDirectoryFiles(files);
  return found;
}

static void StopMusic(void) {
  if (music.stream.buffer) {
    StopMusicStream(music);
    UnloadMusicStream(music);
  }
  music = (Music){0};
  musicPausedForLoading = false;
}

static void Quit(void) {
  quitConfirmation = TowerProgressNeedsSaving();
  if (!quitConfirmation)
    game_running = false;
}

static void Save(void) {
  saveOk = SaveTowerProgress();
  saveTime = 3;
}

static void OpenInventory(void) { inventoryActive = true; }
static void NewGame(void);

static void EnterTower(void) {
  trainingActive = false;
  ResetTower();
  subscreenActive = true;
}

static void ExitTower(void) { subscreenActive = false; }

static void Subscreen(void) {
  if (TowerCurrentFloor() > 50)
    LoadingScreenStartWithBackground(NewGame, "assets/ending.png");
  else
    LoadingScreenStartWithBackground(EnterTower, "assets/tower.png");
}

static void ReturnToMenu(void) {
  menuRequested = true;
  subscreenActive = false;
  trainingActive = false;
}

static void NewGame(void) {
  if (!DeleteTowerProgressFile())
    return;
  ResetTowerProgress();
  ReturnToMenu();
}

static void Menu(void) {
  if (TowerProgressNeedsSaving())
    returnConfirmation = true;
  else
    LoadingScreenStart(ReturnToMenu);
}

static void StartTraining(void) {
  char song[512] = "";
  monsterPath[0] = '\0';

  bool found = GetRandomValue(1, 100) == 1 &&
               PickFile("assets/easter_eggs", ".png", monsterPath);
  if (!found)
    found = PickFile("assets/monsters", ".png", monsterPath);
  if (!found)
    found = PickFile("assets/easter_eggs", ".png", monsterPath);
  if (!found)
    TraceLog(LOG_WARNING, "Aucun PNG de monstre trouve");

  StopMusic();
  if (PickFile("assets/musics/fights", ".mp3", song)) {
    music = LoadMusicStream(song);
    if (music.stream.buffer)
      PlayMusicStream(music);
  } else {
    TraceLog(LOG_WARNING, "Aucune musique MP3 trouvee");
  }

  const char *gameName = trainingGame == 0 ? "BULLET_HELL"
                         : trainingGame == 1 ? "RING"
                         : trainingGame == 2 ? "MIRROR"
                                             : "SEALS";
  ItemsApplyForGame(gameName);
  int difficulty = FunctionalDifficulty(GetRandomValue(8, 10));
  switch (trainingGame) {
  case 0:
    BulletHellStart(difficulty);
    break;
  case 1:
    RingsStart(difficulty);
    break;
  case 2:
    MirrorStart(difficulty);
    break;
  default:
    SealsStart(difficulty);
    break;
  }
  trainingSelectionActive = false;
  trainingActive = true;
  subscreenActive = false;
}

static void Training(void) {
  trainingGame = 0;
  trainingSelectionActive = true;
}

static void StartSelectedTraining(void) {
  trainingSelectionActive = false;
  LoadingScreenStart(StartTraining);
}

static bool DrawTrainingOption(Rectangle box, const char *label, bool selected) {
  bool hover = CheckCollisionPointRec(GetMousePosition(), box);
  if (hover)
    selected = true;

  Color fill = selected ? (Color){75, 58, 28, 255} : (Color){25, 25, 30, 245};
  Color edge = selected ? (Color){245, 205, 95, 255} : RAYWHITE;
  DrawRectangleRec(box, fill);
  DrawRectangleLinesEx(box, selected ? 3 : 1, edge);
  int size = (int)(box.height * 0.48f);
  DrawText(label, (int)(box.x + (box.width - MeasureText(label, size)) / 2),
           (int)(box.y + (box.height - size) / 2), size, RAYWHITE);

  return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void DrawTrainingSelection(void) {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  float panelW = sw * 0.88f, panelH = sh * 0.86f;
  if (panelW > 640)
    panelW = 640;
  if (panelH > 540)
    panelH = 540;
  Rectangle panel = {(sw - panelW) / 2, (sh - panelH) / 2, panelW, panelH};
  float rowHeight = panelH * 0.105f;
  float rowGap = panelH * 0.025f;
  float rowX = panel.x + panelW * 0.12f;
  float rowW = panelW * 0.76f;
  float rowY = panel.y + panelH * 0.19f;

  if (IsKeyPressed(KEY_ESCAPE)) {
    trainingSelectionActive = false;
    return;
  }
  if (IsKeyPressed(KEY_DOWN))
    trainingGame = (trainingGame + 1) % 4;
  if (IsKeyPressed(KEY_UP))
    trainingGame = (trainingGame + 3) % 4;

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 185});
  DrawRectangleRec(panel, (Color){15, 15, 20, 250});
  DrawRectangleLinesEx(panel, 3, (Color){225, 185, 40, 255});

  int titleSize = (int)(panelH * 0.075f);
  DrawText("COMBAT D'ENTRAINEMENT",
           (int)(panel.x + (panelW - MeasureText("COMBAT D'ENTRAINEMENT",
                                                  titleSize)) / 2),
           (int)(panel.y + panelH * 0.06f), titleSize,
           (Color){245, 220, 130, 255});

  for (int i = 0; i < 4; i++) {
    Rectangle row = {rowX, rowY + i * (rowHeight + rowGap), rowW, rowHeight};
    if (DrawTrainingOption(row, trainingNames[i], trainingGame == i))
      trainingGame = i;
  }

  DrawText("Difficulte aleatoire : 8 a 10",
           (int)(panel.x + panelW * 0.12f),
           (int)(panel.y + panelH * 0.70f), (int)(panelH * 0.04f),
           (Color){190, 190, 200, 255});

  float buttonY = panel.y + panelH * 0.76f;
  float buttonH = panelH * 0.10f;
  Rectangle back = {rowX, buttonY, rowW * 0.44f, buttonH};
  Rectangle start = {rowX + rowW * 0.56f, buttonY, rowW * 0.44f, buttonH};
  bool backClicked = DrawTrainingOption(back, "RETOUR", false);
  bool startClicked = DrawTrainingOption(start, "LANCER", true);
  if (backClicked) {
    trainingSelectionActive = false;
    PlayButtonClickSound();
  } else if (startClicked || IsKeyPressed(KEY_ENTER)) {
    PlayButtonClickSound();
    StartSelectedTraining();
  }
}

static void UpdateTrainingGame(Rectangle zone) {
  switch (trainingGame) {
  case 0:
    BulletHellUpdate(zone);
    break;
  case 1:
    RingsUpdate(zone);
    break;
  case 2:
    MirrorUpdate(zone);
    break;
  default:
    SealsUpdate(zone);
    break;
  }
}

static int TrainingDamageCount(void) {
  switch (trainingGame) {
  case 0:
    return BulletHellDamageCount();
  case 1:
    return RingsDamageCount();
  case 2:
    return MirrorDamageCount();
  default:
    return SealsDamageCount();
  }
}

static bool TrainingFinished(void) {
  switch (trainingGame) {
  case 0:
    return BulletHellState() == HELL_OFF;
  case 1:
    return RingsState() == RINGS_OFF;
  case 2:
    return MirrorState() == MIRROR_OFF;
  default:
    return SealsState() == SEALS_OFF;
  }
}

static Button buttons[6] = {
    {"Sauvegarder", Save, 0.045f},
    {"Menu principal", Menu, 0.0f},
    {"Quitter", Quit, 0.09f},
    {"Combat d'entrainement", Training, 0.045f},
    {"Monter la tour", Subscreen, 0.0f},
    {"Inventaire", OpenInventory, 0.09f},
};

static void DrawButton(int i, bool acceptInput) {
  float sw = (float)GetScreenWidth();
  float sh = (float)GetScreenHeight();
  float w = sw * 0.24f;
  float h = sh * 0.07f;
  float offset = sw * buttons[i].inward;

  Rectangle r;
  if (i < 3)
    r.x = sw * 0.02f + offset;
  else
    r.x = sw - w - sw * 0.02f - offset;
  r.y = (sh - h) / 2 + (i % 3 - 1) * h * 1.5f;
  r.width = w;
  r.height = h;

  float cx = r.x + w / 2;
  float cy = r.y + h / 2;
  bool hover = CheckCollisionPointRec(GetMousePosition(), r);
  bool enabled = acceptInput && !inventoryActive && !LoadingScreenActive();
  UpdateButtonHoverSound((ButtonSoundId)(BUTTON_SOUND_PEACE_SAVE + i),
                         hover && enabled);

  if (hover && bandTex.id) {
    float bandW = w * 1.125f;
    float bandH = bandW * bandTex.height / bandTex.width;
    Rectangle source = {0, 0, (float)bandTex.width, (float)bandTex.height};
    Rectangle dest = {cx - bandW / 2, cy - bandH * 0.46f, bandW, bandH};
    DrawTexturePro(bandTex, source, dest, (Vector2){0, 0}, 0, WHITE);
  }

  const char *label = buttons[i].label;
  if (i == 4 && TowerCurrentFloor() > 50)
    label = "Altérer le destin";
  const char *text = TextToUpper(label);
  float fontSize = h * 0.33f;
  Vector2 size = MeasureTextEx(font, text, fontSize, 2);
  Vector2 pos = {(int)(cx - size.x / 2), (int)(cy - size.y / 2)};

  Color textColor = {205, 200, 190, 255};
  if (hover)
    textColor = (Color){255, 248, 230, 255};

  DrawTextEx(font, text, (Vector2){pos.x + 1, pos.y + 1}, fontSize, 2, (Color){0, 0, 0, 140});
  DrawTextEx(font, text, pos, fontSize, 2, textColor);

  bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && enabled &&
                 buttons[i].action;
  if (clicked) {
    PlayButtonClickSound();
    buttons[i].action();
  }
}

void UpdateGameSaveAlert(void) {
  if (saveTime > 0) {
    saveTime -= GetFrameTime();
    if (saveTime < 0)
      saveTime = 0;
  }
}

bool GameSaveAlertActive(void) { return saveTime > 0; }

void DrawGameSaveAlert(void) {
  if (!GameSaveAlertActive())
    return;

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float w = sw * 0.48f;
  float h = sh * 0.09f;
  Rectangle box = {(sw - w) / 2, sh * 0.04f, w, h};

  const char *message = "Echec de la sauvegarde";
  Color accent = {245, 180, 90, 255};
  if (saveOk) {
    message = "Progression sauvegardee";
    accent = (Color){150, 220, 160, 255};
  }

  int size = (int)(h * 0.27f);
  DrawRectangleRec(box, (Color){18, 18, 24, 235});
  DrawRectangleLinesEx(box, 2, accent);

  int textX = (int)(box.x + (box.width - MeasureText(message, size)) / 2);
  int textY = (int)(box.y + (h - size) / 2);
  DrawText(message, textX, textY, size, accent);
}

void DrawGameButtons(bool acceptInput) {
  bool inventoryWasActive = inventoryActive;

  if (font.texture.id == 0) {
    bandTex = LoadTexture("assets/button.png");
    SetTextureFilter(bandTex, TEXTURE_FILTER_BILINEAR);
    if (FileExists("assets/font.ttf"))
      font = LoadFontEx("assets/font.ttf", 64, NULL, 0);
    else
      font = GetFontDefault();
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
  }

  if (trainingSelectionActive) {
    DrawTrainingSelection();
  } else if (trainingActive) {
    const char *monster = NULL;
    if (monsterPath[0])
      monster = monsterPath;
    DrawGameZone(UpdateTrainingGame, TrainingDamageCount, monster);

    if (TrainingFinished() && !LoadingScreenActive())
      LoadingScreenStart(ResetGameButtons);
  } else if (subscreenActive) {
    Tower();
    if (TowerTakeExitRequest())
      LoadingScreenStart(ExitTower);
  } else if (returnConfirmation) {
    UnsavedDialogResult result = DrawUnsavedSessionDialog();
    if (result == UNSAVED_DIALOG_CONTINUE) {
      returnConfirmation = false;
      LoadingScreenStart(ReturnToMenu);
    } else if (result == UNSAVED_DIALOG_CANCEL) {
      returnConfirmation = false;
    }
  } else if (quitConfirmation) {
    UnsavedDialogResult result = DrawUnsavedSessionDialog();
    if (result == UNSAVED_DIALOG_CONTINUE)
      game_running = false;
    else if (result == UNSAVED_DIALOG_CANCEL)
      quitConfirmation = false;
  } else {
    for (int i = 0; i < 6; i++)
      DrawButton(i, acceptInput);

    if (inventoryActive)
      inventoryActive = DrawInventory(inventoryWasActive);
  }
}

void UpdateGameButtonsMusic(void) {
  if (LoadingScreenActive()) {
    if (music.stream.buffer && !musicPausedForLoading)
      PauseMusicStream(music);
    musicPausedForLoading = true;
  } else {
    if (music.stream.buffer && musicPausedForLoading)
      ResumeMusicStream(music);
    musicPausedForLoading = false;
    if (trainingActive && music.stream.buffer)
      UpdateMusicStream(music);
  }
  if (subscreenActive)
    UpdateTowerMusic();
}

bool GameReturnToMenuRequested(void) {
  bool requested = menuRequested;
  menuRequested = false;
  return requested;
}

bool GameSubscreenActive(void) {
  return subscreenActive;
}

bool GameTrainingActive(void) {
  return trainingActive;
}

bool GameInventoryActive(void) { return inventoryActive; }
bool GameResultActive(void) { return subscreenActive && TowerResultActive(); }
bool GameReturnConfirmationActive(void) { return returnConfirmation; }

void ResetGameButtons(void) {
  menuRequested = false;
  subscreenActive = false;
  trainingActive = false;
  trainingSelectionActive = false;
  inventoryActive = false;
  quitConfirmation = false;
  returnConfirmation = false;
  monsterPath[0] = '\0';
  saveTime = 0;
  StopMusic();
  ResetTower();
}

void UnloadGameButtons(void) {
  UnloadTexture(bandTex);
  UnloadInventory();
  ItemsUnload();
  if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id)
    UnloadFont(font);
  bandTex = (Texture2D){0};
  font = (Font){0};
  ResetGameButtons();
}