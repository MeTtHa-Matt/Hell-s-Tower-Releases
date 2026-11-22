#if defined(__linux__)
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#endif

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

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

#ifdef __APPLE__
#include <errno.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int ChangeToExecutableAssetDirectory(void) {
  uint32_t pathSize = 1024;
  char *executablePath = malloc(pathSize);
  if (!executablePath) {
    fprintf(stderr, "Hell's Tower: mémoire insuffisante pour le chemin du binaire\n");
    return -1;
  }

  for (;;) {
    uint32_t requiredSize = pathSize;
    if (_NSGetExecutablePath(executablePath, &requiredSize) == 0)
      break;
    if (requiredSize <= pathSize) {
      if (pathSize > UINT32_MAX / 2) {
        fprintf(stderr, "Hell's Tower: chemin du binaire trop long\n");
        free(executablePath);
        return -1;
      }
      requiredSize = pathSize * 2;
    }
    char *largerPath = realloc(executablePath, requiredSize);
    if (!largerPath) {
      fprintf(stderr, "Hell's Tower: mémoire insuffisante pour le chemin du binaire\n");
      free(executablePath);
      return -1;
    }
    executablePath = largerPath;
    pathSize = requiredSize;
  }

  char *resolvedPath = realpath(executablePath, NULL);
  free(executablePath);
  if (!resolvedPath) {
    fprintf(stderr, "Hell's Tower: impossible de résoudre le chemin du binaire : %s\n",
            strerror(errno));
    return -1;
  }

  char *separator = strrchr(resolvedPath, '/');
  if (!separator) {
    fprintf(stderr, "Hell's Tower: chemin du binaire invalide\n");
    free(resolvedPath);
    return -1;
  }
  *separator = '\0';

  size_t directoryLength = strlen(resolvedPath);
  char *resourceDirectory = malloc(directoryLength + sizeof("/../Resources"));
  char *resourceAssets = malloc(directoryLength + sizeof("/../Resources/assets"));
  char *executableAssets = malloc(directoryLength + sizeof("/assets"));
  if (!resourceDirectory || !resourceAssets || !executableAssets) {
    fprintf(stderr, "Hell's Tower: mémoire insuffisante pour les ressources\n");
    free(resourceDirectory);
    free(resourceAssets);
    free(executableAssets);
    free(resolvedPath);
    return -1;
  }

  snprintf(resourceDirectory, directoryLength + sizeof("/../Resources"),
           "%s/../Resources", resolvedPath);
  snprintf(resourceAssets, directoryLength + sizeof("/../Resources/assets"),
           "%s/../Resources/assets", resolvedPath);
  snprintf(executableAssets, directoryLength + sizeof("/assets"), "%s/assets",
           resolvedPath);

  bool changedDirectory =
      access(resourceAssets, F_OK) == 0 && chdir(resourceDirectory) == 0;
  if (!changedDirectory)
    changedDirectory = access(executableAssets, F_OK) == 0 &&
                       chdir(resolvedPath) == 0;
  if (!changedDirectory)
    fprintf(stderr,
            "Hell's Tower: impossible de trouver les ressources à côté du "
            "binaire ou dans le bundle macOS\n");

  free(resourceDirectory);
  free(resourceAssets);
  free(executableAssets);
  free(resolvedPath);
  return changedDirectory ? 0 : -1;
}
#endif

bool game_running = true;

int main(void) {
#ifdef __linux__
  if (ChangeToExecutableDirectory() != 0)
    return 1;
  RegisterAppImageDesktopEntry();
#endif
#ifdef __APPLE__
  if (ChangeToExecutableAssetDirectory() != 0)
    return 1;
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