#include "background/background.h"
#include "buttons/buttons.h"
#include "../game/pause_menu/pause_menu.h"
#include "../game/monster/monster.h"
#include "../game/chatbox/chatbox.h"
#include "../game/buttons/buttons.h"
#include "../game/buttons/button_sounds.h"
#include "../game/tower.h"
#include "../game/ui_scale.h"
#include "../loading_screen/loading_screen.h"
#include "raylib.h"
#include "title/title.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TITLE "Hell's Tower"
#define DIALOGUE_COUNT 14
#define MAX_SIGRID_DIALOGUES 63
#define SIGRID_TEXT_LEN 512

static Music menuMusic;
static Music ambientMusic;
static bool menuMusicLoaded;
static bool menuMusicPaused;
static bool ambientMusicLoaded;
static bool ambientMusicStarted;
static bool ambientMusicPaused;
static Texture2D cursorTexture;
static bool returnToMainMenu;
static char sigridDialogues[MAX_SIGRID_DIALOGUES][SIGRID_TEXT_LEN];
static char sigridFinal[SIGRID_TEXT_LEN];
static int sigridCount, sigridFloor, sigridIndex;
static int sigridLoadedFloor = -1;
static bool sigridActive;

static const char *dialogues[DIALOGUE_COUNT] = {
    "Oh ! Un voyageur ! Bonjour, bonjour ! N'aie pas peur, je suis un "
    "monstre, mais pas de ceux de la tour. Bienvenue a son pied !",
    "Je m'appelle Sigrid. Je suis un mage-guide... enfin, mes sorts font "
    "surtout plus de fumee que d'effet. Disons que je me suis perdu en "
    "cherchant mon seigneur, et que j'aide ceux qui osent grimper.",
    "Ici, nous sommes dans la zone de paix. Personne n'a le droit de se "
    "battre, tu peux reprendre ton souffle tranquillement. Et si tu as "
    "envie de parler, je suis toujours la.",
    "Avant de monter, laisse-moi te dire ce que je sais. Peu de gens "
    "m'ecoutent jusqu'au bout, alors merci d'avance. Ceux qui le font vont "
    "souvent plus loin.",
    "Cette tour n'a pas ete batie par des mains d'homme. Elle a surgi "
    "apres la chute d'un royaume, en une seule nuit, et elle accorde "
    "d'etranges pouvoirs a ceux qu'elle juge dignes de la gravir.",
    "C'est sa magie : meme si tu echoues dans un etage, ton ame absorbe "
    "l'echo de tes combats. Certains elus deviennent ainsi plus forts a "
    "chaque tentative, comme s'ils elevaient leur propre essence.",
    "Cinquante etages. Cinquante epreuves. Et tous les dix paliers, un "
    "gardien barre la route, prisonnier d'une douleur qu'il n'a jamais su "
    "lacher.",
    "Au dixieme etage regne Behemot, Demon de l'Arrogance. Il est persuade "
    "que la tour lui appartient. Prouve-lui le contraire. Je l'ai connu "
    "autrefois : il n'a pas toujours ete un demon.",
    "Plus haut, Selena, la Deesse Dechue, pleure encore le ciel qu'elle a "
    "perdu. Agares, Demon de la Vengeance, n'oublie aucun visage.",
    "Et Dame Marika, Celle qui Regna. On dit que meme son nom fait baisser "
    "la voix des plus braves.",
    "Ceux qui redescendent de si haut ne parlent plus. Ils fixent "
    "l'escalier, comme s'ils entendaient encore un appel.",
    "Tout au sommet, une derniere gardienne veille. Je prefere taire son "
    "nom pour l'instant : certains noms pesent trop lourd pour un premier "
    "jour. Mais on murmure que celui qui la vaincra pourra faire un voeu "
    "a la tour. Un seul.",
    "Mon seigneur est monte avant moi, il y a si longtemps. J'ai perdu sa "
    "trace dans les marches, et je ne sais meme pas s'il est encore en "
    "vie. Alors je guide ceux qui grimpent, en esperant que l'un d'eux le "
    "retrouve. Aide-moi, s'il te plait.",
    "Mais assez parle. Prepare ton equipement dans ton inventaire, et "
    "tente de gravir la tour des que tu te sentiras pret. Reviens me voir "
    "apres chaque etage : la zone de paix t'est toujours ouverte, et moi "
    "aussi.",
};

static void LoadSigridDialogues(int floor) {
  char line[32768];
  if (sigridLoadedFloor == floor)
    return;
  sigridLoadedFloor = floor;
  sigridFloor = floor;
  sigridCount = 0;
  sigridFinal[0] = '\0';
  FILE *file = fopen("assets/sigrid.txt", "r");
  if (!file) {
    TraceLog(LOG_WARNING, "Fichier assets/sigrid.txt introuvable");
    return;
  }

  while (fgets(line, sizeof(line), file)) {
    char *first = strchr(line, ';');
    char *second = first ? strchr(first + 1, ';') : NULL;
    char *third = second ? strchr(second + 1, ';') : NULL;
    if (!first || !second || !third || atoi(second + 1) != floor)
      continue;

    *first = *second = *third = '\0';
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
    snprintf(sigridFinal, sizeof(sigridFinal), "%s", third + 1);
    sigridFinal[strcspn(sigridFinal, "\r\n")] = '\0';
    if (sigridCount != total - 1 || !sigridFinal[0]) {
      sigridCount = 0;
      sigridFinal[0] = '\0';
      TraceLog(LOG_WARNING, "Format invalide dans assets/sigrid.txt, etage %d", floor);
    }
    break;
  }
  fclose(file);
}

static void UpdateSigrid(bool clicked) {
  if (clicked && sigridFinal[0] && !sigridActive) {
    sigridActive = true;
    sigridIndex = TowerSigridDialogSeen(sigridFloor) ? sigridCount : 0;
    ResetChatbox();
    return;
  }
  if (!sigridActive)
    return;

  const char *text = sigridIndex < sigridCount
                         ? sigridDialogues[sigridIndex]
                         : sigridFinal;
  chatbox(text, "low");
  if (!IsChatboxFinished(text) || IsChatboxInputConsumed())
    return;

  if (sigridIndex < sigridCount) {
    sigridIndex++;
    ResetChatbox();
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
  LoadTowerProgress();
  StartPlayingButton();
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
  LoadingScreenInit();
  SetRandomSeed((unsigned int)(GetTime() * 1000000));
  SetExitKey(KEY_NULL);

  int w = GetMonitorWidth(GetCurrentMonitor());
  int h = GetMonitorHeight(GetCurrentMonitor());
  SetWindowSize(w, h);
  ToggleFullscreen();
  SetTargetFPS(60);

  cursorTexture = LoadTexture("assets/cursor.png");
  if (cursorTexture.id != 0)
    HideCursor();
  else
    TraceLog(LOG_ERROR, "Curseur introuvable");

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

void LoadMenu(void) {
  bool loadingToGame = false;

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
    DrawMenuTitle(TITLE);
    Button("Jouer");
    Button("Options");
    Button("Quitter");

    if (StartPlayingRequested() && !loadingToGame) {
      ClearStartPlayingRequest();
      LoadingScreenStart(LoadSavedGame);
      loadingToGame = true;
    }

    DrawCustomCursor();
    LoadingScreenDraw();
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
  ambientMusicStarted = false;
  ambientMusicPaused = false;

  ResetChatbox();
  ResetGameButtons();
  sigridActive = false;
  sigridIndex = 0;

  while (!WindowShouldClose() && game_running && !leave) {
    LoadingScreenUpdate();

    if (IsKeyPressed(KEY_ESCAPE) && !GameInventoryActive() &&
        !quitConfirmation && !returnConfirmation && !LoadingScreenActive()) {
      paused = !paused;
      shadeAlpha = 0;
    }

    UpdateGameButtonsMusic();

    bool inGame = GameSubscreenActive() || GameTrainingActive();
    UpdateAmbientMusic(!paused && !inGame);

    BeginButtonSoundFrame();
    BeginDrawing();
    ClearBackground(BLACK);

    if (paused && (quitConfirmation || returnConfirmation)) {
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
        if (TowerProgressNeedsSaving()) {
          returnConfirmation = true;
        } else if (inGame) {
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
      if (!tutorialSeen) {
        for (int i = 0; i < DIALOGUE_COUNT; i++)
          chatbox(dialogues[i], "low");
      }

      bool lastDialogueDone = IsChatboxFinished(dialogues[DIALOGUE_COUNT - 1]) &&
                              !IsChatboxInputConsumed();
      bool canTalk = tutorialSeen || lastDialogueDone;
      LoadSigridDialogues(TowerCurrentFloor());
      bool dead = TowerCurrentFloor() == 51 &&
                  (TowerSigridDialogSeen(51) ||
                   (sigridActive && sigridIndex == sigridCount));
      const char *image = dead ? "assets/dead_sigrid.png" : "assets/monster1.png";
      bool clicked = monster(image, "middle", canTalk && !GameInventoryActive() &&
                                                 !GameSaveAlertActive() &&
                                                 !GameReturnConfirmationActive() &&
                                                 !GameTrainingSelectionActive());
      if (canTalk && !GameSaveAlertActive() &&
          !GameReturnConfirmationActive() && !GameTrainingSelectionActive())
        UpdateSigrid(clicked);
      if (tutorialSeen || lastDialogueDone) {
        MarkTowerTutorialSeen();
        DrawGameButtons(true);
      }
      DrawGameSaveAlert();
    }

    if (!paused && GameReturnToMenuRequested())
      leave = true;
    if (returnToMainMenu)
      leave = true;

    if (paused || !inGame || GameResultActive())
      DrawCustomCursor();

    LoadingScreenDraw();
    EndButtonSoundFrame();
    EndDrawing();
  }

  if (ambientMusicStarted)
    StopMusicStream(ambientMusic);
  ResetGameButtons();
}

void Shutdown(void) {
  UnloadButtonSounds();
  UnloadGameButtons();
  UnloadTexture(cursorTexture);
  ShowCursor();
  UnloadChatbox();
  UnloadMonster();
  UnloadMenuBackground();

  if (menuMusicLoaded)
    UnloadMusicStream(menuMusic);
  if (ambientMusicLoaded)
    UnloadMusicStream(ambientMusic);

  LoadingScreenUnload();
  if (IsAudioDeviceReady())
    CloseAudioDevice();
}