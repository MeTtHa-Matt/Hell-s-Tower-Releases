#include "buttons.h"
#include "../game_zone/bullet hell/bullethell.h"
#include "../game_zone/mirror/mirror.h"
#include "../game_zone/rings/rings.h"
#include "../game_zone/seals/seals.h"
#include "../game_zone/spikes/spikes.h"
#include "../game_zone/modifiers.h"
#include "../game_zone/zone.h"
#include "../inventory/inventory.h"
#include "../items/functionnal/functionnal.h"
#include "../items/items.h"
#include "../characters/characters.h"
#include "../achievements/achievements.h"
#include "../pause_menu/pause_menu.h"
#include "../shop/shop.h"
#include "../forge/forge.h"
#include "../tower.h"
#include "../ui_scale.h"
#include "../../loading_screen/loading_screen.h"
#include "button_sounds.h"
#include "raylib.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

extern bool game_running;

static bool menuRequested, subscreenActive, trainingActive, trainingSelectionActive;
static bool inventoryActive;
static bool sigridEndingUnlocked;
static bool quitConfirmation, returnConfirmation, saveOk;
static bool musicPausedForLoading;
static bool melinaEndingRequested;
static bool destinyReflectionRequested;
static float saveTime;
static Font font;
static Texture2D bandTex;
static Music music;
static char monsterPath[512];
static int trainingGame, trainingDifficulty = 8;

#define TRAINING_GAME_COUNT 5

static const char *trainingNames[] = {
    "BULLET HELL", "RINGS", "MIRROR", "SEALS", "SPIKES"};
static const ButtonSoundId trainingSounds[] = {
    BUTTON_SOUND_TRAINING_BULLET_HELL,
    BUTTON_SOUND_TRAINING_RINGS,
    BUTTON_SOUND_TRAINING_MIRROR,
    BUTTON_SOUND_TRAINING_SEALS,
    BUTTON_SOUND_TRAINING_SPIKES,
};
static const char *trainingDescriptions[] = {
    "Esquivez les projectiles.",
    "Traversez les anneaux.",
    "Affrontez votre reflet.",
    "Brisez les sceaux.",
    "Evitez les boules a pointes.",
};
static const Color trainingAccents[] = {
    {230, 105, 95, 255},
    {90, 195, 220, 255},
    {175, 140, 235, 255},
    {235, 190, 85, 255},
    {180, 190, 205, 255},
};

typedef struct {
  const char *label;
  void (*action)(void);
  int side;
  int row;
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
static void OpenShop(void) { ShopOpen(); }
static void OpenForge(void) { ForgeOpen(); }
static void PrepareDestinyEnding(void);

static void EnterTower(void) {
  trainingActive = false;
  ResetTower();
  subscreenActive = true;
}

static void ExitTower(void) { subscreenActive = false; }

static void Subscreen(void) {
  if (TowerCurrentFloor() > 50 && !sigridEndingUnlocked)
    return;
  if (TowerCurrentFloor() > 50 &&
      TowerPlayerCharacter() == 0 &&
      ItemsMelinaLetterRead()) {
    melinaEndingRequested = true;
  } else if (TowerCurrentFloor() > 50) {
    LoadingScreenStartWithBackground(PrepareDestinyEnding, "assets/ending.png");
  } else {
    LoadingScreenStartWithBackground(EnterTower, "assets/tower.png");
  }
}

static void ReturnToMenu(void) {
  menuRequested = true;
  subscreenActive = false;
  trainingActive = false;
}

static void PrepareDestinyEnding(void) {
  if (!DeleteTowerProgressFile())
    return;
  AchievementsUnlock(ACHIEVEMENT_COMPLETE_GAME);
  AchievementsUnlockTitle(TITLE_CHANGED_FATE);
  ResetTowerProgress();
  destinyReflectionRequested = true;
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
                         : trainingGame == 3 ? "SEALS"
                                             : "SPIKES";
  ItemsApplyForGame(gameName);
  CharacterApplyForGame(TowerPlayerCharacter());
  int difficulty = FunctionalDifficulty(trainingDifficulty);
  switch (trainingGame) {
  case 0:
    BulletHellStart(50, difficulty);
    break;
  case 1:
    RingsStart(50, difficulty);
    break;
  case 2:
    MirrorStart(50, difficulty);
    break;
  case 3:
    SealsStart(50, difficulty);
    break;
  default:
    SpikesStart(50, difficulty);
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

static void DrawTrainingText(const char *text, float centerX, float y,
                             float maxWidth, float size, Color color) {
  float spacing = GameUiScale();
  Vector2 textSize = MeasureTextEx(font, text, size, spacing);
  if (textSize.x > maxWidth) {
    size *= maxWidth / textSize.x;
    textSize = MeasureTextEx(font, text, size, spacing);
  }
  DrawTextEx(font, text, (Vector2){centerX - textSize.x / 2, y}, size, spacing,
             color);
}

static bool DrawTrainingOption(Rectangle box, const char *label, bool selected,
                               ButtonSoundId soundId) {
  bool hover = CheckCollisionPointRec(GetMousePosition(), box);
  float scale = GameUiScale();
  UpdateButtonHoverSound(soundId, hover);

  Color fill = selected ? (Color){86, 62, 29, 255}
                        : hover ? (Color){51, 48, 41, 255}
                                : (Color){25, 27, 34, 255};
  Color edge = selected ? (Color){245, 205, 105, 255}
                        : hover ? (Color){174, 151, 106, 255}
                                : (Color){91, 94, 105, 255};
  DrawRectangleRounded(box, 0.18f, 8, fill);
  DrawRectangleRoundedLinesEx(box, 0.18f, 8, (selected ? 2 : 1) * scale, edge);
  float size = fmaxf(10, box.height * 0.38f);
  DrawTrainingText(label, box.x + box.width / 2, box.y + (box.height - size) / 2,
                   box.width - 12 * scale, size, RAYWHITE);

  bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (clicked)
    PlayButtonClickSound();
  return clicked;
}

static void DrawTrainingIcon(int index, Vector2 center, float radius,
                             Color accent) {
  DrawCircleV(center, radius * 1.25f,
              (Color){accent.r, accent.g, accent.b, 28});
  if (index == 0) {
    static const float points[][2] = {
        {-0.68f, -0.42f}, {0, -0.78f}, {0.68f, -0.42f},
        {-0.68f, 0.42f},  {0, 0.78f},  {0.68f, 0.42f},
    };
    for (int i = 0; i < 6; i++)
      DrawCircleV((Vector2){center.x + points[i][0] * radius,
                            center.y + points[i][1] * radius},
                  radius * 0.13f, accent);
    DrawCircleV(center, radius * 0.19f, RAYWHITE);
  } else if (index == 1) {
    DrawCircleLines((int)center.x, (int)center.y, radius * 0.78f, accent);
    DrawCircleLines((int)center.x, (int)center.y, radius * 0.4f, accent);
    DrawCircleV((Vector2){center.x + radius * 0.76f, center.y},
                radius * 0.13f, RAYWHITE);
  } else if (index == 2) {
    Vector2 top = {center.x, center.y - radius};
    Vector2 left = {center.x - radius * 0.7f, center.y};
    Vector2 right = {center.x + radius * 0.7f, center.y};
    Vector2 bottom = {center.x, center.y + radius};
    DrawTriangle(top, left, center, accent);
    DrawTriangle(center, right, bottom,
                 (Color){accent.r, accent.g, accent.b, 145});
  } else if (index == 3) {
    DrawCircleLines((int)center.x, (int)center.y, radius * 0.8f, accent);
    DrawCircleLines((int)center.x, (int)center.y, radius * 0.4f, accent);
    DrawLineEx((Vector2){center.x - radius * 0.58f, center.y},
               (Vector2){center.x + radius * 0.58f, center.y}, 2, RAYWHITE);
    DrawLineEx((Vector2){center.x, center.y - radius * 0.58f},
               (Vector2){center.x, center.y + radius * 0.58f}, 2, RAYWHITE);
  } else {
    for (int i = 0; i < 8; i++) {
      float angle = i * 2 * PI / 8;
      Vector2 tip = {center.x + cosf(angle) * radius,
                     center.y + sinf(angle) * radius};
      Vector2 left = {center.x + cosf(angle - 0.22f) * radius * 0.56f,
                      center.y + sinf(angle - 0.22f) * radius * 0.56f};
      Vector2 right = {center.x + cosf(angle + 0.22f) * radius * 0.56f,
                       center.y + sinf(angle + 0.22f) * radius * 0.56f};
      DrawTriangle(tip, left, right, accent);
      DrawTriangle(tip, right, left, accent);
    }
    DrawCircleV(center, radius * 0.42f, (Color){55, 55, 65, 255});
    DrawCircleLines((int)center.x, (int)center.y, radius * 0.42f, RAYWHITE);
  }
}

static bool DrawTrainingCard(Rectangle box, int index, bool selected,
                             float scale) {
  bool hover = CheckCollisionPointRec(GetMousePosition(), box);
  Color accent = trainingAccents[index];
  Color fill = selected ? (Color){34, 31, 29, 255}
                        : hover ? (Color){29, 31, 39, 255}
                                : (Color){19, 21, 28, 245};
  Color edge = selected ? accent
                        : hover ? (Color){143, 139, 128, 255}
                                : (Color){66, 70, 81, 255};
  DrawRectangleRounded(box, 0.1f, 10, fill);
  DrawRectangleRoundedLinesEx(box, 0.1f, 10, (selected ? 2.5f : 1) * scale,
                              edge);
  if (selected)
    DrawRectangleRounded((Rectangle){box.x + 2 * scale, box.y + box.height * 0.2f,
                                     3 * scale, box.height * 0.6f},
                         0.8f, 6, accent);

  bool shortCard = box.height < 78 * fmaxf(scale, 0.75f);
  bool narrowCard = box.width < 220 * fmaxf(scale, 0.75f);
  Vector2 iconCenter;
  float radius;
  if (shortCard) {
    iconCenter = (Vector2){box.x + box.width * 0.17f,
                           box.y + box.height * 0.5f};
    radius = fminf(box.width * 0.1f, box.height * 0.23f);
    DrawTrainingIcon(index, iconCenter, radius, accent);
    float titleSize = fmaxf(10, box.height * 0.3f);
    DrawTrainingText(trainingNames[index], box.x + box.width * 0.62f,
                     box.y + (box.height - titleSize) / 2, box.width * 0.66f,
                     titleSize, RAYWHITE);
  } else if (narrowCard) {
    iconCenter = (Vector2){box.x + box.width * 0.5f,
                           box.y + box.height * 0.36f};
    radius = fminf(box.width * 0.19f, box.height * 0.17f);
    DrawTrainingIcon(index, iconCenter, radius, accent);
    float titleSize = fmaxf(10, box.height * 0.17f);
    DrawTrainingText(trainingNames[index], box.x + box.width / 2,
                     box.y + box.height * 0.7f, box.width * 0.88f, titleSize,
                     RAYWHITE);
  } else {
    iconCenter = (Vector2){box.x + box.width * 0.16f,
                           box.y + box.height * 0.5f};
    radius = fminf(box.width * 0.09f, box.height * 0.24f);
    DrawTrainingIcon(index, iconCenter, radius, accent);
    float titleSize = fmaxf(10, fminf(25 * scale, box.height * 0.2f));
    DrawTrainingText(trainingNames[index], box.x + box.width * 0.61f,
                     box.y + box.height * 0.3f, box.width * 0.7f, titleSize,
                     RAYWHITE);
    DrawTrainingText(trainingDescriptions[index], box.x + box.width * 0.61f,
                     box.y + box.height * 0.58f, box.width * 0.7f,
                     fmaxf(11 * scale, box.height * 0.11f),
                     (Color){184, 187, 196, 255});
  }
  UpdateButtonHoverSound(trainingSounds[index], hover);
  bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (clicked)
    PlayButtonClickSound();
  return clicked;
}

static void DrawTrainingSelection(void) {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  float scale = GameUiScale();
  float maxScale = scale > 1 ? scale : 1;
  float panelW = fminf(sw * 0.94f, 980 * maxScale);
  float panelH = fminf(sh * 0.94f, 900 * maxScale);
  Rectangle panel = {(sw - panelW) / 2, (sh - panelH) / 2, panelW, panelH};
  float contentX = panel.x + panelW * 0.08f;
  float contentW = panelW * 0.84f;
  float cardGap = panelW * 0.035f;
  float cardW = (contentW - cardGap) / 2;
  float cardH = panelH * 0.135f;
  float cardY = panel.y + panelH * 0.22f;
  float cardRowGap = panelH * 0.014f;
  float difficultyY = panel.y + panelH * 0.67f;
  float difficultyH = panelH * 0.12f;
  float footerY = panel.y + panelH * 0.84f;
  float footerH = panelH * 0.1f;
  float footerGap = panelW * 0.035f;
  float footerW = (contentW - footerGap) / 2;

  if (IsKeyPressed(KEY_ESCAPE)) {
    trainingSelectionActive = false;
    return;
  }
  if (IsKeyPressed(KEY_DOWN))
    trainingGame = (trainingGame + 1) % TRAINING_GAME_COUNT;
  if (IsKeyPressed(KEY_UP))
    trainingGame = (trainingGame + TRAINING_GAME_COUNT - 1) %
                   TRAINING_GAME_COUNT;
  if (IsKeyPressed(KEY_LEFT) && trainingDifficulty > 1)
    trainingDifficulty--;
  if (IsKeyPressed(KEY_RIGHT) && trainingDifficulty < 10)
    trainingDifficulty++;

  DrawRectangle(0, 0, sw, sh, (Color){5, 6, 10, 205});
  Rectangle shadow = {panel.x + 5 * scale, panel.y + 7 * scale, panel.width,
                      panel.height};
  DrawRectangleRounded(shadow, 0.045f, 12, (Color){0, 0, 0, 145});
  DrawRectangleRounded(panel, 0.045f, 12, (Color){17, 19, 25, 250});
  DrawRectangleRoundedLinesEx(panel, 0.045f, 12, 2 * scale,
                              (Color){145, 119, 70, 255});
  Rectangle accent = {panel.x + panelW * 0.08f, panel.y + 2 * scale,
                      panelW * 0.84f, 4 * scale};
  DrawRectangleRounded(accent, 0.8f, 8, (Color){223, 184, 103, 255});

  DrawTrainingText("COMBAT D'ENTRAINEMENT", panel.x + panelW / 2,
                   panel.y + panelH * 0.065f, panelW * 0.86f,
                   panelH * 0.055f, (Color){245, 222, 169, 255});
  DrawTrainingText("CHOISISSEZ VOTRE EPREUVE", panel.x + panelW / 2,
                   panel.y + panelH * 0.15f, panelW * 0.8f,
                   fmaxf(10, panelH * 0.022f),
                   (Color){166, 169, 180, 255});

  for (int i = 0; i < TRAINING_GAME_COUNT; i++) {
    int row = i / 2;
    int column = i % 2;
    float x = contentX + column * (cardW + cardGap);
    if (i == TRAINING_GAME_COUNT - 1)
      x = panel.x + (panelW - cardW) / 2;
    Rectangle card = {x, cardY + row * (cardH + cardRowGap), cardW, cardH};
    if (DrawTrainingCard(card, i, trainingGame == i, scale))
      trainingGame = i;
  }

  DrawTrainingText("DIFFICULTE", panel.x + panelW / 2,
                   difficultyY + difficultyH * 0.04f, panelW * 0.6f,
                   fmaxf(10, difficultyH * 0.27f),
                   (Color){171, 174, 184, 255});
  float controlH = difficultyH * 0.48f;
  float controlW = panelW * 0.1f;
  Rectangle easier = {panel.x + panelW * 0.22f,
                      difficultyY + difficultyH * 0.34f, controlW, controlH};
  Rectangle harder = {panel.x + panelW * 0.68f,
                      difficultyY + difficultyH * 0.34f, controlW, controlH};
  if (DrawTrainingOption(easier, "-", false, BUTTON_SOUND_TRAINING_EASIER) &&
      trainingDifficulty > 1)
    trainingDifficulty--;
  if (DrawTrainingOption(harder, "+", false, BUTTON_SOUND_TRAINING_HARDER) &&
      trainingDifficulty < 10)
    trainingDifficulty++;
  char difficultyLabel[24];
  snprintf(difficultyLabel, sizeof(difficultyLabel), "%02d / 10",
           trainingDifficulty);
  DrawTrainingText(difficultyLabel, panel.x + panelW / 2,
                   difficultyY + difficultyH * 0.36f, panelW * 0.3f,
                   fmaxf(10, difficultyH * 0.34f),
                   (Color){245, 220, 150, 255});
  float meterW = panelW * 0.3f;
  float meterGap = 3 * scale;
  float meterX = panel.x + (panelW - meterW) / 2;
  float meterY = difficultyY + difficultyH * 0.82f;
  float meterH = 3 * scale;
  float meterStepW = (meterW - meterGap * 9) / 10;
  for (int i = 0; i < 10; i++) {
    Rectangle step = {meterX + i * (meterStepW + meterGap), meterY, meterStepW,
                      meterH};
    DrawRectangleRounded(step, 0.8f, 4,
                         i < trainingDifficulty
                             ? (Color){223, 184, 103, 255}
                             : (Color){62, 65, 75, 255});
  }

  Rectangle back = {contentX, footerY, footerW, footerH};
  Rectangle start = {contentX + footerW + footerGap, footerY, footerW, footerH};
  bool backClicked =
      DrawTrainingOption(back, "ANNULER", false, BUTTON_SOUND_TRAINING_CANCEL);
  bool startClicked =
      DrawTrainingOption(start, "JOUER", true, BUTTON_SOUND_TRAINING_START);
  if (backClicked) {
    trainingSelectionActive = false;
  } else if (startClicked || IsKeyPressed(KEY_ENTER)) {
    if (!startClicked)
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
  case 3:
    SealsUpdate(zone);
    break;
  default:
    SpikesUpdate(zone);
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
  case 3:
    return SealsDamageCount();
  default:
    return SpikesDamageCount();
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
  case 3:
    return SealsState() == SEALS_OFF;
  default:
    return SpikesState() == SPIKES_OFF;
  }
}

static Button buttons[8] = {
    {"Sauvegarder", Save, 0, 0, 0.075f},
    {"Menu principal", Menu, 0, 1, 0.0f},
    {"Quitter", Quit, 0, 2, 0.065f},
    {"Combat d'entrainement", Training, 1, 3, 0.035f},
    {"Monter la tour", Subscreen, 1, 2, 0.0f},
    {"Inventaire", OpenInventory, 1, 4, 0.075f},
    {"Boutique", OpenShop, 1, 0, 0.075f},
    {"Forger des items", OpenForge, 1, 1, 0.04f},
};

static void DrawButton(int i, bool acceptInput) {
  float sw = (float)GetScreenWidth();
  float sh = (float)GetScreenHeight();
  float w = sw * 0.24f;
  float h = sh * 0.07f;
  float offset = sw * buttons[i].inward;
  int count = buttons[i].side == 0 ? 3 : 5;
  float rowSpacing = buttons[i].side == 0 ? 1.55f : 1.3f;

  Rectangle r;
  if (buttons[i].side == 0)
    r.x = sw * 0.02f + offset;
  else
    r.x = sw - w - sw * 0.02f - offset;
  r.y = sh / 2 + (buttons[i].row - (count - 1) / 2.0f) * h * rowSpacing;
  r.width = w;
  r.height = h;

  float cx = r.x + w / 2;
  float cy = r.y + h / 2;
  bool hover = CheckCollisionPointRec(GetMousePosition(), r);
  bool enabled = acceptInput && !inventoryActive && !ShopActive() &&
                 !ForgeActive() &&
                 !LoadingScreenActive();
  if (i == 4 && TowerCurrentFloor() > 50)
    enabled = enabled && sigridEndingUnlocked;
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
  if (i == 4 && TowerCurrentFloor() > 50) {
    label = TowerPlayerCharacter() == 0 && ItemsMelinaLetterRead()
                ? "Sortir de la tour"
                : "Altérer le destin";
  }
  const char *text = TextToUpper(label);
  float fontSize = h * 0.33f;
  Vector2 size = MeasureTextEx(font, text, fontSize, 2);
  if (size.x > w * 0.9f) {
    fontSize *= w * 0.9f / size.x;
    size = MeasureTextEx(font, text, fontSize, 2);
  }
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

bool GameMelinaEndingRequested(void) {
  bool requested = melinaEndingRequested;
  melinaEndingRequested = false;
  return requested;
}

bool GameDestinyReflectionRequested(void) {
  bool requested = destinyReflectionRequested;
  destinyReflectionRequested = false;
  return requested;
}

void GameFinishDestinyReflection(void) { ReturnToMenu(); }

bool GameFinishMelinaEnding(void) {
  if (!DeleteTowerProgressFile())
    return false;
  AchievementsUnlock(ACHIEVEMENT_COMPLETE_GAME);
  AchievementsUnlock(ACHIEVEMENT_MELINA_ENDING);
  AchievementsUnlockTitle(TITLE_FOUND_TRUTH);
  ResetTowerProgress();
  ReturnToMenu();
  return true;
}

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

  int size = GameUiPixels(20);
  DrawRectangleRec(box, (Color){18, 18, 24, 235});
  DrawRectangleLinesEx(box, 2 * GameUiScale(), accent);

  int textX = (int)(box.x + (box.width - MeasureText(message, size)) / 2);
  int textY = (int)(box.y + (h - size) / 2);
  DrawText(message, textX, textY, size, accent);
}

void DrawGameButtons(bool acceptInput) {
  bool inventoryWasActive = inventoryActive;

  if (font.texture.id == 0) {
    bandTex = LoadTexture("assets/button.png");
    SetTextureFilter(bandTex, TEXTURE_FILTER_BILINEAR);
    if (FileExists("assets/font.ttf")) {
      int codepointCount = 0;
      const int *codepoints = GameUiFontCodepoints(&codepointCount);
      font = LoadFontEx("assets/font.ttf", 64, codepoints, codepointCount);
    } else {
      font = GetFontDefault();
    }
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
  }

  if (trainingSelectionActive) {
    DrawTrainingSelection();
  } else if (trainingActive) {
    const char *monster = NULL;
    if (monsterPath[0])
      monster = monsterPath;
    DrawGameZone(UpdateTrainingGame, TrainingDamageCount, monster);
    GameZoneDrawActiveModifiers(
        (Rectangle){GameUiPixels(24), 0,
                    (float)GetScreenWidth() - 2 * GameUiPixels(24),
                    (float)GetScreenHeight()},
        GameUiPixels(24));

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
      melinaEndingRequested = false;
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
    if (ShopActive()) {
      ShopDraw();
    } else if (ForgeActive()) {
      ForgeDraw();
    } else {
      for (int i = 0; i < 8; i++)
        DrawButton(i, acceptInput);
      if (ShopActive())
        ShopDraw();
      if (ForgeActive())
        ForgeDraw();
      if (inventoryActive)
        inventoryActive = DrawInventory(inventoryWasActive);
    }
    if (!LoadingScreenActive())
      TowerDrawEtrisHud(true);
  }
  if (!LoadingScreenActive() && (subscreenActive || trainingActive))
    TowerDrawEtrisHud(false);
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

bool GameTrainingSelectionActive(void) { return trainingSelectionActive; }

bool GameInventoryActive(void) { return inventoryActive; }
bool GameShopActive(void) { return ShopActive() || ForgeActive(); }
bool GameResultActive(void) { return subscreenActive && TowerResultActive(); }
void GameSetSigridEndingUnlocked(bool unlocked) {
  sigridEndingUnlocked = unlocked;
}
bool GameSigridEndingUnlocked(void) { return sigridEndingUnlocked; }
bool GameReturnConfirmationActive(void) {
  return returnConfirmation || quitConfirmation;
}

void ResetGameButtons(void) {
  menuRequested = false;
  destinyReflectionRequested = false;
  subscreenActive = false;
  trainingActive = false;
  trainingSelectionActive = false;
  inventoryActive = false;
  quitConfirmation = false;
  returnConfirmation = false;
  monsterPath[0] = '\0';
  saveTime = 0;
  sigridEndingUnlocked = false;
  StopMusic();
  ForgeClose();
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