#include "menu/initialisation.h"
#include "menu/buttons/buttons.h"
#include <stdbool.h>

#ifdef __linux__
#include "linux_desktop.h"
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int ChangeToExecutableDirectory(void) {
  size_t capacity = 256;
  char *executablePath = malloc(capacity);
  ssize_t pathLength;

  if (executablePath == NULL) {
    fprintf(stderr, "Hell's Tower: mémoire insuffisante pour le chemin du binaire\n");
    return -1;
  }

  for (;;) {
    pathLength = readlink("/proc/self/exe", executablePath, capacity);
    if (pathLength < 0) {
      fprintf(stderr, "Hell's Tower: impossible de lire /proc/self/exe : %s\n",
              strerror(errno));
      free(executablePath);
      return -1;
    }
    if ((size_t)pathLength < capacity)
      break;
    if (capacity > (size_t)-1 / 2) {
      fprintf(stderr, "Hell's Tower: chemin du binaire trop long\n");
      free(executablePath);
      return -1;
    }
    capacity *= 2;
    char *largerPath = realloc(executablePath, capacity);
    if (largerPath == NULL) {
      fprintf(stderr, "Hell's Tower: mémoire insuffisante pour le chemin du binaire\n");
      free(executablePath);
      return -1;
    }
    executablePath = largerPath;
  }

  executablePath[pathLength] = '\0';
  if (chdir(dirname(executablePath)) != 0) {
    fprintf(stderr, "Hell's Tower: impossible de rejoindre le dossier du binaire : %s\n",
            strerror(errno));
    free(executablePath);
    return -1;
  }

  free(executablePath);
  return 0;
}
#endif

bool game_running = true;

int main(void) {
#ifdef __linux__
  if (ChangeToExecutableDirectory() != 0)
    return 1;
  RegisterAppImageDesktopEntry();
#endif

  Init();

  while (!WindowShouldClose() && game_running) {
    LoadMenu();

    if (StartPlayingRequested()) {
      ClearStartPlayingRequest();
      LoadBlackScreen();
    }
  }

  Shutdown();
  CloseWindow();
  return 0;
}