#include "background/background.h"
#include "buttons/buttons.h"
#include "character_selection.h"
#include "../game/pause_menu/pause_menu.h"
#include "../game/monster/monster.h"
#include "../game/chatbox/chatbox.h"
#include "../game/buttons/buttons.h"
#include "../game/buttons/button_sounds.h"
#include "../game/achievements/achievements.h"
#include "../game/tower.h"
#include "../game/items/items.h"
#include "../game/ui_scale.h"
#include "../loading_screen/loading_screen.h"
#include "../updater.h"
#include "raylib.h"
#include "update_screen.h"
#include "title/title.h"
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TITLE "Hell's Tower"
#define TUTORIAL_DIALOGUE_COUNT 14
#define MAX_SIGRID_DIALOGUES 63
#define SIGRID_TEXT_LEN 512

static const char *characterFiles[] = {
    "melina", "elyssia", "kaelen", "gorran", "ysolde", "thaddeus"};
static Music menuMusic;
static Music ambientMusic;
static bool menuMusicLoaded;
static bool menuMusicPaused;
static bool ambientMusicLoaded;
static bool ambientMusicStarted;
static bool ambientMusicPaused;
static Texture2D cursorTexture;
static Texture2D titleTexture;
static Font introFont;
static bool introFontLoaded, introFontReady, startupSequencePlayed;
static bool startupMenuFadePending;
static float startupMenuFadeOpacity;
static bool returnToMainMenu;
static bool characterSelectionPending;
static char tutorialDialogues[TUTORIAL_DIALOGUE_COUNT][SIGRID_TEXT_LEN];
static int tutorialCount, tutorialCharacter = -1;
static char sigridDialogues[MAX_SIGRID_DIALOGUES][SIGRID_TEXT_LEN];
static char sigridFinal[SIGRID_TEXT_LEN];
static int sigridCount, sigridFloor, sigridIndex;
static int sigridLoadedFloor = -1, sigridLoadedCharacter = -1;
static bool sigridActive;
static Texture2D creditsBackground;
static bool creditsActive, creditsFinal, creditsFinishFailed;
static float creditsScroll, creditsFinalTimer;
static bool creditsFadeOut;
static float creditsFadeProgress;
static bool destinyReflectionActive, destinyTextFading;
static float destinyTextProgress, destinyFadeProgress;

static bool LoadTutorialDialogues(void) {
  int character = TowerPlayerCharacter();
  if (tutorialCharacter == character)
    return tutorialCount == TUTORIAL_DIALOGUE_COUNT;
  tutorialCharacter = character;
  tutorialCount = 0;
  if (character < 0 || character >= (int)(sizeof(characterFiles) /
                                           sizeof(characterFiles[0]))) {
    TraceLog(LOG_ERROR, "Personnage invalide pour le tutoriel: %d", character);
    return false;
  }
  char path[128], line[SIGRID_TEXT_LEN];
  snprintf(path, sizeof(path), "assets/tuto/sigrid_tuto_%s.txt",
           characterFiles[character]);
  FILE *file = fopen(path, "r");
  if (!file) {
    TraceLog(LOG_ERROR, "Fichier de tutoriel introuvable: %s", path);
    return false;
  }
  while (fgets(line, sizeof(line), file)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (!line[0])
      continue;
    if (tutorialCount == TUTORIAL_DIALOGUE_COUNT) {
      TraceLog(LOG_ERROR, "Trop de dialogues dans %s", path);
      tutorialCount = 0;
      break;
    }
    snprintf(tutorialDialogues[tutorialCount++], SIGRID_TEXT_LEN, "%s", line);
  }
  fclose(file);
  if (tutorialCount != TUTORIAL_DIALOGUE_COUNT) {
    TraceLog(LOG_ERROR, "Format invalide dans %s: %d dialogues", path,
             tutorialCount);
    tutorialCount = 0;
  }
  return tutorialCount == TUTORIAL_DIALOGUE_COUNT;
}

static void LoadSigridDialogues(int floor) {
  char line[32768];
  int character = TowerPlayerCharacter();
  if (sigridLoadedFloor == floor && sigridLoadedCharacter == character)
    return;
  sigridLoadedFloor = floor;
  sigridLoadedCharacter = character;
  sigridFloor = floor;
  sigridCount = 0;
  sigridFinal[0] = '\0';
  if (character < 0 || character >= (int)(sizeof(characterFiles) /
                                           sizeof(characterFiles[0]))) {
    TraceLog(LOG_ERROR, "Personnage invalide pour les dialogues de Sigrid");
    return;
  }
  char path[128];
  snprintf(path, sizeof(path), "assets/story/story_%s.txt",
           characterFiles[character]);
  FILE *file = fopen(path, "r");
  if (!file) {
    TraceLog(LOG_ERROR, "Fichier de dialogues introuvable: %s", path);
    return;
  }

  while (fgets(line, sizeof(line), file)) {
    char *first = strchr(line, ';');
    if (!first)
      continue;

    char *floorSeparator = NULL;
    char *floorEnd = NULL;
    for (char *separator = strchr(first + 1, ';'); separator;
         separator = strchr(separator + 1, ';')) {
      char *end = NULL;
      long entryFloor = strtol(separator + 1, &end, 10);
      if (end == separator + 1 || *end != ';' || entryFloor != floor)
        continue;
      floorSeparator = separator;
      floorEnd = end;
      break;
    }
    if (!floorSeparator)
      continue;

    *first = *floorSeparator = *floorEnd = '\0';
    int total = atoi(line);
    if (total < 1 || total > MAX_SIGRID_DIALOGUES + 1)
      break;
    char *part = first + 1;
    while (*part) {
      char *next = strchr(part, '&');
      if (next)
        *next = '\0';
      if (sigridCount >= total - 1 || strlen(part) >= SIGRID_TEXT_LEN) {
        sigridCount = 0;
        break;
      }
      snprintf(sigridDialogues[sigridCount++], SIGRID_TEXT_LEN, "%s", part);
      if (!next)
        break;
      part = next + 1;
    }
    snprintf(sigridFinal, sizeof(sigridFinal), "%s", floorEnd + 1);
    sigridFinal[strcspn(sigridFinal, "\r\n")] = '\0';
    if (strcmp(sigridFinal, "...") == 0)
      sigridFinal[0] = '\0';
    if (sigridCount != total - 1 ||
        (!sigridCount && !sigridFinal[0])) {
      sigridCount = 0;
      sigridFinal[0] = '\0';
      TraceLog(LOG_ERROR, "Format invalide dans %s, etage %d", path, floor);
    }
    break;
  }
  fclose(file);
  if (!sigridCount && !sigridFinal[0])
    TraceLog(LOG_WARNING, "Aucun dialogue dans %s pour le tag %d", path, floor);
}

static void UpdateSigrid(bool clicked) {
  if (clicked && (sigridCount || sigridFinal[0]) && !sigridActive) {
    sigridActive = true;
    sigridIndex = TowerSigridDialogSeen(sigridFloor) ? sigridCount : 0;
    ResetChatbox();
    return;
  }
  if (!sigridActive)
    return;

  if (sigridIndex < sigridCount) {
    const char *text = sigridDialogues[sigridIndex];
    chatbox(text, "low");
    if (!IsChatboxFinished(text) || IsChatboxInputConsumed())
      return;
    sigridIndex++;
    ResetChatbox();
    return;
  }
  if (sigridFinal[0]) {
    chatbox(sigridFinal, "low");
    if (!IsChatboxFinished(sigridFinal) || IsChatboxInputConsumed())
      return;
  }

  sigridActive = false;
  if (!TowerSigridDialogSeen(sigridFloor)) {
    MarkTowerSigridDialogSeen(sigridFloor);
  }
  ResetChatbox();
}

static void RequestMainMenuExit(void) {
  returnToMainMenu = true;
}

static void LoadSavedGame(void) {
  if (LoadTowerProgress()) {
    StartPlayingButton();
    return;
  }
  ResetTowerProgress();
  StartCharacterSelection();
  characterSelectionPending = true;
}

static void DrawCustomCursor(void) {
  if (cursorTexture.id == 0)
    return;

  Vector2 mouse = GetMousePosition();
  float width = 84 * GameUiScale();
  float height = width * cursorTexture.height / cursorTexture.width;

  Rectangle source = {0, 0, (float)cursorTexture.width, (float)cursorTexture.height};
  Rectangle dest = {mouse.x - width * 420 / 1254, mouse.y - height * 270 / 1254,
                    width, height};
  DrawTexturePro(cursorTexture, source, dest, (Vector2){0, 0}, 0, WHITE);
}

static void UpdateAmbientMusic(bool active) {
  if (!ambientMusicLoaded)
    return;

  if (active && !LoadingScreenActive()) {
    if (!ambientMusicStarted) {
      PlayMusicStream(ambientMusic);
      ambientMusicStarted = true;
    } else if (ambientMusicPaused) {
      ResumeMusicStream(ambientMusic);
      ambientMusicPaused = false;
    }
    UpdateMusicStream(ambientMusic);
  } else if (ambientMusicStarted && !ambientMusicPaused) {
    PauseMusicStream(ambientMusic);
    ambientMusicPaused = true;
  }
}

static void StartMelinaCredits(void) {
  creditsBackground = LoadTexture("assets/melina_ending.png");
  if (creditsBackground.id == 0)
    TraceLog(LOG_ERROR, "Image de fin introuvable: assets/melina_ending.png");
  creditsScroll = 0;
  creditsFinalTimer = 0;
  creditsFadeOut = false;
  creditsFadeProgress = 0;
  creditsFinal = creditsFinishFailed = false;
  creditsActive = true;
  if (menuMusicLoaded) {
    PlayMusicStream(menuMusic);
    menuMusicPaused = false;
  }
}

static void FinishMelinaCredits(void) {
  if (!GameFinishMelinaEnding()) {
    creditsFinishFailed = true;
    return;
  }
  creditsActive = false;
  if (creditsBackground.id != 0)
    UnloadTexture(creditsBackground);
  creditsBackground = (Texture2D){0};
  if (menuMusicLoaded)
    StopMusicStream(menuMusic);
  menuMusicPaused = false;
}

static void DrawMelinaCredits(void) {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  float scale = GameUiScale();
  if (creditsBackground.id != 0) {
    float screenAspect = (float)sw / sh;
    float imageAspect =
        (float)creditsBackground.width / creditsBackground.height;
    Rectangle source = {0, 0, (float)creditsBackground.width,
                        (float)creditsBackground.height};
    if (imageAspect > screenAspect) {
      source.width = creditsBackground.height * screenAspect;
      source.x = (creditsBackground.width - source.width) / 2;
    } else {
      source.height = creditsBackground.width / screenAspect;
      source.y = (creditsBackground.height - source.height) / 2;
    }
    DrawTexturePro(creditsBackground, source,
                   (Rectangle){0, 0, (float)sw, (float)sh},
                   (Vector2){0, 0}, 0, WHITE);
  } else {
    ClearBackground(BLACK);
  }
  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 160});

  if (creditsFinal) {
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 130});
    const char *lines[] = {
        "Tu n'as pas réécrit le passé,",
        "mais tu as offert un lendemain.",
        "Merci à toi, grimpeuse."};
    int size = GameUiPixels(42);
    float maxWidth = sw * 0.86f;
    for (int i = 0; i < 3; i++)
      if (MeasureText(lines[i], size) > maxWidth)
        size = (int)(size * maxWidth / MeasureText(lines[i], size));
    int lineHeight = (int)(size * 1.45f);
    int startY = (sh - lineHeight * 3) / 2;
    for (int i = 0; i < 3; i++)
      DrawText(lines[i], (sw - MeasureText(lines[i], size)) / 2,
               startY + i * lineHeight, size, (Color){255, 239, 195, 255});
    if (creditsFinishFailed) {
      const char *error = "Impossible de supprimer la sauvegarde. Réessayez.";
      int errorSize = GameUiPixels(18);
      DrawText(error, (sw - MeasureText(error, errorSize)) / 2,
               sh * 0.88f, errorSize, RAYWHITE);
      if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
          IsKeyPressed(KEY_ESCAPE))
        FinishMelinaCredits();
      return;
    }
    creditsFinalTimer += GetFrameTime();
    if (creditsFinalTimer >= 4 || IsKeyPressed(KEY_ESCAPE))
      creditsFadeOut = true;
    if (creditsFadeOut) {
      creditsFadeProgress += GetFrameTime() / 1.2f;
      if (creditsFadeProgress < 0)
        creditsFadeProgress = 0;
      if (creditsFadeProgress > 1)
        creditsFadeProgress = 1;
      DrawRectangle(0, 0, sw, sh,
                    (Color){0, 0, 0,
                            (unsigned char)(255 * creditsFadeProgress)});
      if (creditsFadeProgress >= 1)
        FinishMelinaCredits();
    }
    return;
  }

  if (IsKeyPressed(KEY_ESCAPE)) {
    creditsFinal = true;
    creditsFinalTimer = 0;
    return;
  }
  const char *lines[] = {"HELL'S TOWER", "Merci d'avoir joué."};
  float lineHeight = GameUiPixels(42);
  creditsScroll += GetFrameTime() * (IsKeyDown(KEY_SPACE) ? 180 : 36) * scale;
  for (int i = 0; i < 2; i++) {
    int size = GameUiPixels(i == 0 ? 34 : 24);
    float y = sh + lineHeight * i - creditsScroll;
    DrawText(lines[i], (sw - MeasureText(lines[i], size)) / 2, (int)y, size,
             (Color){245, 225, 180, 255});
  }
  if (creditsScroll > sh + lineHeight * 2) {
    creditsFinal = true;
    creditsFinalTimer = 0;
  }
}

void Init(void) {
  char assetsDirectory[1024];
#ifdef __APPLE__
  const char *applicationDirectory = GetApplicationDirectory();
  size_t pathLength = strlen(applicationDirectory);
  const char *separator =
      pathLength > 0 && applicationDirectory[pathLength - 1] != '/' &&
              applicationDirectory[pathLength - 1] != '\\'
          ? "/"
          : "";
  snprintf(assetsDirectory, sizeof(assetsDirectory), "%s%s../Resources",
           applicationDirectory, separator);
#else
  snprintf(assetsDirectory, sizeof(assetsDirectory), "%s",
           GetApplicationDirectory());
#endif
  if (ChangeDirectory(assetsDirectory) != 0)
    TraceLog(LOG_ERROR, "Impossible d'acceder au dossier du jeu: %s",
             assetsDirectory);

  InitWindow(800, 450, TITLE);
  AchievementsInit();
  Image windowIcon = LoadImage("assets/logo.png");
  if (windowIcon.data != NULL) {
    SetWindowIcon(windowIcon);
    UnloadImage(windowIcon);
  } else {
    TraceLog(LOG_ERROR, "Icône de l'application introuvable");
  }
  LoadingScreenInit();
  UpdaterInitialize();
  SetRandomSeed((unsigned int)(GetTime() * 1000000));
  SetExitKey(KEY_NULL);

  int w = GetMonitorWidth(GetCurrentMonitor());
  int h = GetMonitorHeight(GetCurrentMonitor());
  SetWindowSize(w, h);
  ToggleFullscreen();
  SetTargetFPS(60);
  InitHomeButtons();
  InitCharacterSelection();

  cursorTexture = LoadTexture("assets/cursor.png");
  if (cursorTexture.id != 0)
    HideCursor();
  else
    TraceLog(LOG_ERROR, "Curseur introuvable");

  titleTexture = LoadTexture("assets/title.png");
  if (titleTexture.id == 0)
    TraceLog(LOG_ERROR, "Titre du menu introuvable");

  InitAudioDevice();
  if (IsAudioDeviceReady()) {
    InitButtonSounds();
    menuMusic = LoadMusicStream("assets/musics/Soliloquy.mp3");
    menuMusicLoaded = menuMusic.stream.buffer != NULL;
    if (!menuMusicLoaded)
      TraceLog(LOG_ERROR, "Musique du menu introuvable");

    ambientMusic = LoadMusicStream("assets/musics/ambiant.mp3");
    ambientMusicLoaded = ambientMusic.stream.buffer != NULL;
    if (!ambientMusicLoaded)
      TraceLog(LOG_ERROR, "Musique d'ambiance introuvable");
  } else {
    TraceLog(LOG_ERROR, "Audio indisponible");
  }

  InitMenuBackground();
}

static void DrawStartupTitle(float opacity) {
  if (titleTexture.id == 0)
    return;
  float width = GetScreenWidth() * 0.82f;
  float height = width * titleTexture.height / titleTexture.width;
  float maxHeight = GetScreenHeight() * 0.55f;
  if (height > maxHeight) {
    height = maxHeight;
    width = height * titleTexture.width / titleTexture.height;
  }
  Rectangle source = {0, 0, (float)titleTexture.width,
                     (float)titleTexture.height};
  Rectangle dest = {(GetScreenWidth() - width) / 2,
                    (GetScreenHeight() - height) / 2, width, height};
  DrawTexturePro(titleTexture, source, dest, (Vector2){0, 0}, 0,
                 (Color){255, 255, 255, (unsigned char)(opacity * 255)});
}

static void DrawStartupParagraph(const char *text, Rectangle bounds,
                                 float size, float opacity) {
  char copy[768], lines[16][256] = {{0}};
  snprintf(copy, sizeof(copy), "%s", text);
  float spacing = size * 0.015f;
  int row = 0;
  char *word = strtok(copy, " ");
  while (word && row < 16) {
    char candidate[256];
    snprintf(candidate, sizeof(candidate), "%s%s%s", lines[row],
             lines[row][0] ? " " : "", word);
    if (lines[row][0] &&
        MeasureTextEx(introFont, candidate, size, spacing).x > bounds.width) {
      row++;
      if (row == 16)
        break;
      snprintf(lines[row], sizeof(lines[row]), "%s", word);
    } else {
      snprintf(lines[row], sizeof(lines[row]), "%s", candidate);
    }
    word = strtok(NULL, " ");
  }

  float lineHeight = size * 1.45f;
  float y = bounds.y + (bounds.height - (row + 1) * lineHeight) / 2;
  Color color = {229, 220, 199, (unsigned char)(opacity * 255)};
  for (int i = 0; i <= row; i++) {
    Vector2 textSize = MeasureTextEx(introFont, lines[i], size, spacing);
    DrawTextEx(introFont, lines[i],
               (Vector2){bounds.x + (bounds.width - textSize.x) / 2, y},
               size, spacing, color);
    y += lineHeight;
  }
}

static bool DrawDestinyReflection(void) {
  static const char text[] =
      "La roue s'est arrêtée, mais le monde n'a rien oublié. Les cendres "
      "gardent encore la forme de ce qui fut. Tu as choisi de reprendre le "
      "fil à son commencement, comme si le temps pouvait rendre ce qu'il a "
      "pris. Pourtant, les morts ne reviennent pas quand on remonte les "
      "aiguilles, et effacer une douleur n'est pas la guérir. La tour ne "
      "nomme pas cela une fin heureuse. Le destin peut se briser, se "
      "retarder, se défier... mais jamais se réécrire sans que le fil se "
      "souvienne de chaque coupure.";
  int length = GetCodepointCount(text);
  bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                 IsKeyPressed(KEY_SPACE);
  if (!destinyTextFading) {
    destinyTextProgress += GetFrameTime() * 58;
    if (destinyTextProgress > length)
      destinyTextProgress = (float)length;
    if (pressed) {
      if (destinyTextProgress < length)
        destinyTextProgress = (float)length;
      else
        destinyTextFading = true;
    }
  } else {
    destinyFadeProgress += GetFrameTime() / 0.9f;
  }

  int visible = (int)destinyTextProgress;
  int byteCount = 0;
  for (int i = 0; i < visible; i++) {
    int codepointSize;
    GetCodepoint(text + byteCount, &codepointSize);
    byteCount += codepointSize;
  }
  char shown[sizeof(text)];
  memcpy(shown, text, byteCount);
  shown[byteCount] = '\0';
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  float opacity = destinyTextFading ? 1 - destinyFadeProgress : 1;
  if (opacity < 0)
    opacity = 0;
  Rectangle bounds = {sw * 0.12f, sh * 0.2f, sw * 0.76f, sh * 0.6f};
  DrawStartupParagraph(shown, bounds, GameUiPixels(32), opacity);
  if (visible == length && !destinyTextFading) {
    const char *hint = "CLIQUEZ OU APPUYEZ SUR ESPACE";
    int size = GameUiPixels(17);
    Vector2 dimensions = MeasureTextEx(introFont, hint, size, size * 0.02f);
    DrawTextEx(introFont, hint,
               (Vector2){(sw - dimensions.x) / 2, sh * 0.88f}, size,
               size * 0.02f, (Color){205, 184, 137, 255});
  }
  return destinyTextFading && destinyFadeProgress >= 1;
}

static void DrawStartupWarning(float opacity) {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  const char *heading = "AVERTISSEMENT";
  const char *text =
      "Ce jeu est une œuvre de fiction. "
      "Ne reproduisez jamais dans la vie réelle ses dangers, violences ou actes interdits.";
  float headingSize = GameUiPixels(42);
  float textSize = GameUiPixels(26);
  unsigned char alpha = (unsigned char)(opacity * 255);
  Vector2 headingSizePx =
      MeasureTextEx(introFont, heading, headingSize, headingSize * 0.02f);
  DrawTextEx(introFont, heading,
             (Vector2){(sw - headingSizePx.x) / 2, sh * 0.38f},
             headingSize, headingSize * 0.02f,
             (Color){230, 194, 130, alpha});
  DrawStartupParagraph(text,
                       (Rectangle){sw * 0.12f, sh * 0.46f,
                                   sw * 0.76f, sh * 0.2f},
                       textSize, opacity);
}

static bool ShowStartupSequence(void) {
  if (startupSequencePlayed)
    return true;
  if (!introFontReady) {
    int codepointCount = 0;
    const int *codepoints = GameUiFontCodepoints(&codepointCount);
    introFont =
        LoadFontEx("assets/IMFellEnglish-Regular.ttf", 64, codepoints,
                   codepointCount);
    introFontLoaded = introFont.texture.id != 0;
    introFontReady = true;
    if (!introFontLoaded) {
      TraceLog(LOG_ERROR, "Police de l'ecran d'introduction introuvable");
      introFont = GetFontDefault();
    } else {
      SetTextureFilter(introFont.texture, TEXTURE_FILTER_BILINEAR);
    }
  }

  static const float durations[] = {1.0f, 1.5f, 1.0f};
  int phase = 0;
  float elapsed = 0;
  while (phase < 3 && !WindowShouldClose() && game_running) {
    float opacity = 1;
    if (phase == 0)
      opacity = elapsed / durations[phase];
    else if (phase == 2)
      opacity = 1 - elapsed / durations[phase];
    if (opacity < 0)
      opacity = 0;
    if (opacity > 1)
      opacity = 1;

    BeginDrawing();
    ClearBackground(BLACK);
    DrawStartupTitle(opacity);
    EndDrawing();

    elapsed += GetFrameTime();
    if (elapsed >= durations[phase]) {
      phase++;
      elapsed = 0;
    }
  }

  float warningElapsed = 0;
  float warningDuration = 3.4f;
  while (warningElapsed < warningDuration &&
         !WindowShouldClose() && game_running) {
    float opacity = 1;
    if (warningElapsed < 0.8f)
      opacity = warningElapsed / 0.8f;
    else if (warningElapsed > warningDuration - 0.8f)
      opacity = (warningDuration - warningElapsed) / 0.8f;
    if (opacity < 0)
      opacity = 0;
    BeginDrawing();
    ClearBackground(BLACK);
    DrawStartupWarning(opacity);
    EndDrawing();
    warningElapsed += GetFrameTime();
  }
  startupSequencePlayed = true;
  startupMenuFadePending = true;
  startupMenuFadeOpacity = 1;
  return true;
}

void LoadMenu(void) {
  bool loadingToGame = false;
  bool achievementsOpen = false;

  if (!ShowStartupSequence())
    return;
  if (menuMusicLoaded)
    PlayMusicStream(menuMusic);

  while (!WindowShouldClose() && game_running && !StartPlayingRequested()) {
    LoadingScreenUpdate();
    if (menuMusicLoaded) {
      if (LoadingScreenActive()) {
        if (!menuMusicPaused) {
          PauseMusicStream(menuMusic);
          menuMusicPaused = true;
        }
      } else {
        if (menuMusicPaused) {
          ResumeMusicStream(menuMusic);
          menuMusicPaused = false;
        }
        UpdateMusicStream(menuMusic);
      }
    }

    BeginButtonSoundFrame();
    BeginDrawing();
    ClearBackground(BLACK);
    UpdateAndDrawMenuBackground();
    if (characterSelectionPending) {
      if (DrawCharacterSelection()) {
        if (SetTowerPlayerCharacter(CharacterSelectionPlayer()) &&
            SaveTowerProgress()) {
          characterSelectionPending = false;
          StartPlayingButton();
        } else {
          CharacterSelectionSaveFailed();
        }
      }
    } else {
      DrawMenuTitle(titleTexture);
      if (achievementsOpen) {
        if (AchievementsDrawScreen())
          achievementsOpen = false;
      } else if (!AchievementsTitleNotificationPending()) {
        Button("Jouer");
        Button("Succes");
        Button("Options");
        Button("Quitter");
        if (TakeSuccessesRequest())
          achievementsOpen = true;
      }
    }

    if (StartPlayingRequested() && !loadingToGame) {
      ClearStartPlayingRequest();
      LoadingScreenStartWithUpdateCheck(LoadSavedGame);
      loadingToGame = true;
    }

    bool updatePopupActive = LoadingScreenUpdatePopupActive();
    AchievementsDrawNotifications();
    if (!characterSelectionPending && !achievementsOpen &&
        !LoadingScreenActive() && !startupMenuFadePending)
      AchievementsDrawTitleNotification();
    if (LoadingScreenDraw())
      game_running = false;
    if (!LoadingScreenActive() || updatePopupActive)
      DrawCustomCursor();
    if (startupMenuFadePending) {
      if (!LoadingScreenActive())
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){0, 0, 0,
                              (unsigned char)(255 * startupMenuFadeOpacity)});
      startupMenuFadeOpacity -= GetFrameTime() / 0.8f;
      if (startupMenuFadeOpacity <= 0)
        startupMenuFadePending = false;
    }
    EndButtonSoundFrame();
    EndDrawing();
  }

  if (menuMusicLoaded)
    StopMusicStream(menuMusic);
  menuMusicPaused = false;
}

void LoadBlackScreen(void) {
  bool paused = false;
  bool leave = false;
  bool quitConfirmation = false, returnConfirmation = false;
  float shadeAlpha = 0;

  returnToMainMenu = false;
  destinyReflectionActive = false;
  destinyTextFading = false;
  destinyTextProgress = destinyFadeProgress = 0;
  ambientMusicStarted = false;
  ambientMusicPaused = false;

  ResetChatbox();
  ResetGameButtons();
  sigridActive = false;
  sigridIndex = 0;

  while (!WindowShouldClose() && game_running && !leave) {
    LoadingScreenUpdate();
    if (GameDestinyReflectionRequested()) {
      destinyReflectionActive = true;
      destinyTextFading = false;
      destinyTextProgress = destinyFadeProgress = 0;
    }

    if (IsKeyPressed(KEY_ESCAPE) && !GameInventoryActive() &&
        !GameShopActive() &&
        !quitConfirmation && !returnConfirmation && !LoadingScreenActive() &&
        !creditsActive && !destinyReflectionActive) {
      paused = !paused;
      shadeAlpha = 0;
    }

    UpdateGameButtonsMusic();

    bool inGame = GameSubscreenActive() || GameTrainingActive();
    UpdateAmbientMusic(!paused && !inGame && !creditsActive);
    if (creditsActive && menuMusicLoaded && !LoadingScreenActive())
      UpdateMusicStream(menuMusic);

    BeginButtonSoundFrame();
    BeginDrawing();
    ClearBackground(BLACK);

    if (creditsActive) {
      if (LoadingScreenActive()) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){0, 0, 0, 255});
      } else {
      DrawMelinaCredits();
      }
    } else if (destinyReflectionActive) {
      if (!LoadingScreenActive() && DrawDestinyReflection()) {
        destinyReflectionActive = false;
        startupMenuFadePending = true;
        startupMenuFadeOpacity = 1;
        GameFinishDestinyReflection();
      }
    } else if (paused && (quitConfirmation || returnConfirmation)) {
      UnsavedDialogResult result = DrawUnsavedSessionDialog();
      if (result == UNSAVED_DIALOG_CONTINUE) {
        if (quitConfirmation) {
          game_running = false;
          leave = true;
        } else {
          returnConfirmation = false;
          if (inGame)
            LoadingScreenStart(ResetGameButtons);
          else
            LoadingScreenStart(RequestMainMenuExit);
          paused = false;
          shadeAlpha = 0;
        }
      } else if (result == UNSAVED_DIALOG_CANCEL) {
        quitConfirmation = false;
        returnConfirmation = false;
      }
    } else if (paused) {
      HomePauseResult result = DrawBlackScreenPauseMenu(&shadeAlpha);
      if (result == HOME_PAUSE_RESUME) {
        paused = false;
        shadeAlpha = 0;
      } else if (result == HOME_PAUSE_RETURN_TO_MENU) {
        if (inGame) {
          LoadingScreenStart(ResetGameButtons);
          paused = false;
          shadeAlpha = 0;
        } else {
          LoadingScreenStart(RequestMainMenuExit);
          paused = false;
          shadeAlpha = 0;
        }
      } else if (result == HOME_PAUSE_QUIT) {
        if (TowerProgressNeedsSaving()) {
          quitConfirmation = true;
        } else {
          game_running = false;
          leave = true;
        }
      }
    } else if (inGame) {
      DrawGameButtons(true);
    } else {
      UpdateGameSaveAlert();
      bool tutorialSeen = TowerTutorialSeen();
      bool tutorialAvailable = LoadTutorialDialogues();
      if (!tutorialSeen && tutorialAvailable)
        for (int i = 0; i < tutorialCount; i++)
          chatbox(tutorialDialogues[i], "low");

      bool lastDialogueDone =
          !tutorialAvailable ||
          (IsChatboxFinished(tutorialDialogues[tutorialCount - 1]) &&
           !IsChatboxInputConsumed());
      bool canTalk = tutorialSeen || lastDialogueDone;
      int storyTag = TowerCurrentFloor();
      bool melinaLetterRead = TowerPlayerCharacter() == 0 &&
                              ItemsMelinaLetterRead();
      if (storyTag == 51 && melinaLetterRead)
        storyTag = 52;
      LoadSigridDialogues(storyTag);
      GameSetSigridEndingUnlocked(TowerSigridDialogSeen(storyTag));
      bool dead = storyTag == 51 &&
                  !melinaLetterRead &&
                  TowerSigridDialogSeen(51);
      const char *image = dead ? "assets/dead_sigrid.png" : "assets/monster1.png";
      bool clicked = monster(image, "middle", canTalk && !GameInventoryActive() &&
                                                 !GameSaveAlertActive() &&
                                                 !GameReturnConfirmationActive() &&
                                                 !GameTrainingSelectionActive() &&
                                                 !GameShopActive());
      if (canTalk && !GameSaveAlertActive() &&
          !GameReturnConfirmationActive() && !GameTrainingSelectionActive() &&
          !GameShopActive())
        UpdateSigrid(clicked);
      if (tutorialSeen || lastDialogueDone) {
        MarkTowerTutorialSeen();
        DrawGameButtons(true);
      }
      DrawGameSaveAlert();
    }

    if (GameMelinaEndingRequested())
      LoadingScreenStartWithBackgroundNoProgress(
          StartMelinaCredits, "assets/melina_ending.png");
    if (!paused && GameReturnToMenuRequested())
      leave = true;
    if (returnToMainMenu)
      leave = true;

    bool updatePopupActive = LoadingScreenUpdatePopupActive();
    if (!leave && !destinyReflectionActive)
      AchievementsDrawNotifications();
    if (LoadingScreenDraw())
      game_running = false;
    if (updatePopupActive ||
        (!LoadingScreenActive() && (paused || !inGame || GameResultActive())))
      DrawCustomCursor();
    EndButtonSoundFrame();
    EndDrawing();
  }

  if (ambientMusicStarted)
    StopMusicStream(ambientMusic);
  if (creditsBackground.id != 0)
    UnloadTexture(creditsBackground);
  creditsBackground = (Texture2D){0};
  if (creditsActive && menuMusicLoaded)
    StopMusicStream(menuMusic);
  creditsActive = false;
  menuMusicPaused = false;
  ResetGameButtons();
}

void Shutdown(void) {
  UnloadButtonSounds();
  UnloadHomeButtons();
  UnloadCharacterSelection();
  UnloadGameButtons();
  UnloadTexture(cursorTexture);
  UnloadTexture(titleTexture);
  if (introFontLoaded)
    UnloadFont(introFont);
  if (creditsBackground.id != 0)
    UnloadTexture(creditsBackground);
  ShowCursor();
  UnloadChatbox();
  UnloadMonster();
  UnloadMenuBackground();

  if (menuMusicLoaded)
    UnloadMusicStream(menuMusic);
  if (ambientMusicLoaded)
    UnloadMusicStream(ambientMusic);

  LoadingScreenUnload();
  UpdaterShutdown();
  if (IsAudioDeviceReady())
    CloseAudioDevice();
}