#include "background/background.h"
#include "buttons/buttons.h"
#include "character_selection.h"
#include "../game/pause_menu/pause_menu.h"
#include "../game/monster/monster.h"
#include "../game/chatbox/chatbox.h"
#include "../game/buttons/buttons.h"
#include "../game/buttons/button_sounds.h"
#include "../game/tower.h"
#include "../game/ui_scale.h"
#include "../loading_screen/loading_screen.h"
#include "../updater.h"
#include "raylib.h"
#include "update_screen.h"
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
static Texture2D titleTexture;
static bool returnToMainMenu;
static bool characterSelectionPending;
static char sigridDialogues[MAX_SIGRID_DIALOGUES][SIGRID_TEXT_LEN];
static char sigridFinal[SIGRID_TEXT_LEN];
static int sigridCount, sigridFloor, sigridIndex;
static int sigridLoadedFloor = -1;
static bool sigridActive;

static const char *dialogues[DIALOGUE_COUNT] = {
    "Halte la, voyageur... Ne porte pas la main a ton arme. Je suis un monstre, "
    "il est vrai, mais je ne suis pas ne de la pierre maudite de cet edifice. "
    "Bienvenue au pied du monolithe.",
    "Etrange... En posant les yeux sur toi, j'ai eu l'impression fugace de "
    "t'avoir deja attendu ici, il y a tres longtemps. Bah, les siecles "
    "jouent des tours a ma vieille memoire.",
    "On me nomme Sigrid. Jadis, j'etais un mage-guide... bien que mes sortileges "
    "n'aient jamais su produire plus que de la cendre et de la fumee. J'ai "
    "perdu celui que je devais proteger, alors j'attends ici, a eclairer la "
    "route de ceux qui osent defier les cieux.",
    "Tu foules le Sanctuaire du Seuil. Ici, les lames se taisent et le sang "
    "ne coule pas. Reprends ton souffle. Tant que tu resteras dans l'ombre de "
    "cette arche, je veillerai sur ton repos.",
    "Avant de gravir la premiere marche, accorde-moi un instant. Les ames "
    "pressees n'ecoutent jamais les avertissements d'un paria... et leurs os "
    "finissent par paver les etages.",
    "Cette tour n'a ete erigee par aucune main mortelle. Elle a jailli des "
    "entrailles de la terre la nuit meme ou notre royaume s'est effondre dans "
    "les flammes. Depuis, elle appelle les ambitieux et les desesperes.",
    "Telle est sa sorcellerie : meme si la mort te fauche dans les hauteurs, "
    "ton ame boit l'echo de tes propres luttes. Ceux que la tour juge dignes "
    "renaissent de leurs cendres, plus affutes a chaque chute.",
    "Cinquante etages. Cinquante supplices. Et tous les dix paliers, un "
    "Gardien te barrera la route. Ne les prends pas pour de simples betes : "
    "ce sont des ames enchainees a un regret qu'elles n'ont jamais su abjurer.",
    "Au dixieme palier regne Behemot, le Demon de l'Arrogance. Il s'imagine "
    "que ces murs lui appartiennent. Brise son illusion. Je l'ai connu sous "
    "le soleil d'autrefois... avant que l'orgueil ne devore son visage.",
    "Plus haut encore, Selena, l'Astre Dechu, pleure un firmament qui ne lui "
    "repond plus. Et au trentieme seuil, Agares, le Fleau de la Vengeance, "
    "rumine une haine que les siecles n'ont pas su eteindre.",
    "Puis vient Dame Marika, la Souveraine des Cendres. Meme dans le silence "
    "de ces pierres, son nom suffit a faire ployer le genou des plus braves.",
    "Tout au sommet veille une derniere sentinelle. Je tairai son nom pour "
    "l'heure : certains fardeaux sont trop lourds pour un premier pas. Mais "
    "les anciens murmuraient que celui qui la terrassera se verra accorder un "
    "voeu par la tour. Un unique miracle.",
    "Mon jeune maitre a franchi ces portes bien avant toi, dans la nuit des "
    "temps. J'ai perdu sa trace dans le dedale des marches. J'ignore s'il "
    "respire encore... Je t'en conjure, grimpeur : si tu vas la-haut, cherche-le.",
    "Assez parle. Verifie ton paquetage, affermis ta prise, et affronte "
    "l'ascension lorsque ton coeur sera pret. Reviens me voir apres chaque "
    "epreuve : le Sanctuaire t'est ouvert, et ma memoire t'appartient.",
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
      Button("Jouer");
      Button("Options");
      Button("Quitter");
    }

    if (StartPlayingRequested() && !loadingToGame) {
      ClearStartPlayingRequest();
      LoadingScreenStartWithUpdateCheck(LoadSavedGame);
      loadingToGame = true;
    }

    bool updatePopupActive = LoadingScreenUpdatePopupActive();
    if (LoadingScreenDraw())
      game_running = false;
    if (!LoadingScreenActive() || updatePopupActive)
      DrawCustomCursor();
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

    bool updatePopupActive = LoadingScreenUpdatePopupActive();
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
  ResetGameButtons();
}

void Shutdown(void) {
  UnloadButtonSounds();
  UnloadHomeButtons();
  UnloadCharacterSelection();
  UnloadGameButtons();
  UnloadTexture(cursorTexture);
  UnloadTexture(titleTexture);
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