#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#ifdef __linux__

#include "linux_desktop.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char *JoinPath(const char *directory, const char *name) {
  size_t directoryLength = strlen(directory);
  size_t nameLength = strlen(name);
  int needsSeparator = directoryLength > 0 && directory[directoryLength - 1] != '/';
  char *path = malloc(directoryLength + (size_t)needsSeparator + nameLength + 1);

  if (path == NULL)
    return NULL;

  memcpy(path, directory, directoryLength);
  if (needsSeparator)
    path[directoryLength++] = '/';
  memcpy(path + directoryLength, name, nameLength + 1);
  return path;
}

static int MakeDirectories(char *path) {
  char *separator;
  struct stat status;

  if (path[0] != '/') {
    errno = EINVAL;
    return -1;
  }

  for (separator = path + 1;; separator++) {
    if (*separator != '/' && *separator != '\0')
      continue;

    char saved = *separator;
    *separator = '\0';
    if (path[1] != '\0' && mkdir(path, 0755) != 0 && errno != EEXIST) {
      *separator = saved;
      return -1;
    }
    if (path[1] != '\0') {
      if (stat(path, &status) != 0) {
        *separator = saved;
        return -1;
      }
      if (!S_ISDIR(status.st_mode)) {
        errno = ENOTDIR;
        *separator = saved;
        return -1;
      }
    }
    *separator = saved;
    if (saved == '\0')
      break;
  }

  return 0;
}

static int CopyFile(const char *sourcePath, const char *destinationPath) {
  FILE *source = fopen(sourcePath, "rb");
  FILE *destination;
  char buffer[8192];
  size_t bytesRead;
  int succeeded = 1;

  if (source == NULL) {
    fprintf(stderr, "Hell's Tower: impossible de lire l'icône %s : %s\n",
            sourcePath, strerror(errno));
    return -1;
  }

  destination = fopen(destinationPath, "wb");
  if (destination == NULL) {
    fprintf(stderr, "Hell's Tower: impossible d'écrire l'icône %s : %s\n",
            destinationPath, strerror(errno));
    fclose(source);
    return -1;
  }

  while ((bytesRead = fread(buffer, 1, sizeof(buffer), source)) > 0) {
    if (fwrite(buffer, 1, bytesRead, destination) != bytesRead) {
      succeeded = 0;
      break;
    }
  }

  if (ferror(source))
    succeeded = 0;
  if (fclose(source) != 0)
    succeeded = 0;
  if (fclose(destination) != 0)
    succeeded = 0;
  if (!succeeded) {
    fprintf(stderr, "Hell's Tower: échec de la copie de l'icône vers %s\n",
            destinationPath);
    return -1;
  }

  if (chmod(destinationPath, 0644) != 0) {
    fprintf(stderr, "Hell's Tower: impossible de régler les droits de %s : %s\n",
            destinationPath, strerror(errno));
    return -1;
  }
  return 0;
}

static int CopyAppImageAtomically(const char *sourcePath,
                                  const char *destinationPath) {
  char buffer[8192];
  size_t temporaryPathLength;
  char *temporaryPath;
  ssize_t bytesRead;
  int source = -1;
  int destination = -1;

  if (strlen(destinationPath) > (size_t)-1 - sizeof(".tmp.XXXXXX")) {
    errno = ENAMETOOLONG;
    goto failure;
  }
  temporaryPathLength = strlen(destinationPath) + sizeof(".tmp.XXXXXX");
  temporaryPath = malloc(temporaryPathLength);
  if (temporaryPath == NULL)
    goto failure;
  snprintf(temporaryPath, temporaryPathLength, "%s.tmp.XXXXXX", destinationPath);

  source = open(sourcePath, O_RDONLY);
  if (source < 0)
    goto failure_with_path;
  destination = mkstemp(temporaryPath);
  if (destination < 0)
    goto failure_with_path;

  for (;;) {
    do {
      bytesRead = read(source, buffer, sizeof(buffer));
    } while (bytesRead < 0 && errno == EINTR);
    if (bytesRead < 0)
      goto failure_with_path;
    if (bytesRead == 0)
      break;

    for (ssize_t bytesWritten = 0; bytesWritten < bytesRead;) {
      ssize_t result = write(destination, buffer + bytesWritten,
                             (size_t)(bytesRead - bytesWritten));
      if (result < 0 && errno == EINTR)
        continue;
      if (result <= 0) {
        if (result == 0)
          errno = EIO;
        goto failure_with_path;
      }
      bytesWritten += result;
    }
  }

  if (fchmod(destination, 0755) != 0)
    goto failure_with_path;
  if (close(source) != 0) {
    source = -1;
    goto failure_with_path;
  }
  source = -1;
  if (close(destination) != 0) {
    destination = -1;
    goto failure_with_path;
  }
  destination = -1;
  if (rename(temporaryPath, destinationPath) != 0)
    goto failure_with_path;

  free(temporaryPath);
  return 0;

failure_with_path: {
    int savedError = errno;
    if (source >= 0)
      close(source);
    if (destination >= 0)
      close(destination);
    unlink(temporaryPath);
    fprintf(stderr, "Hell's Tower: impossible d'installer %s : %s\n",
            destinationPath, strerror(savedError));
    free(temporaryPath);
    errno = savedError;
    return -1;
  }

failure:
  fprintf(stderr, "Hell's Tower: impossible de préparer la copie de %s : %s\n",
          destinationPath, strerror(errno));
  return -1;
}

static char *EscapeExecArgument(const char *argument) {
  size_t length = strlen(argument);
  size_t capacity;
  size_t outputLength = 0;
  char *escaped;

  if (length > (size_t)-1 / 2 - 1)
    return NULL;
  capacity = length * 2 + 1;
  escaped = malloc(capacity);
  if (escaped == NULL)
    return NULL;

  for (size_t i = 0; i < length; i++) {
    unsigned char character = (unsigned char)argument[i];
    if (character < 0x20 || character == 0x7f) {
      free(escaped);
      errno = EINVAL;
      return NULL;
    }
    if (character == ' ' || character == '\\' || character == '"' ||
        character == '`' || character == '$')
      escaped[outputLength++] = '\\';
    if (character == '%')
      escaped[outputLength++] = '%';
    escaped[outputLength++] = (char)character;
  }
  escaped[outputLength] = '\0';
  return escaped;
}

static int WriteDesktopEntry(const char *desktopPath, const char *appImagePath,
                             const char *appImageDirectory,
                             const char *iconPath) {
  char *escapedAppImage = EscapeExecArgument(appImagePath);
  char *temporaryPath;
  size_t temporaryPathLength;
  int descriptor;
  FILE *file;
  int succeeded = 1;

  if (escapedAppImage == NULL) {
    fprintf(stderr, "Hell's Tower: chemin APPIMAGE invalide : %s\n",
            strerror(errno));
    return -1;
  }

  temporaryPathLength = strlen(desktopPath) + sizeof(".tmp.XXXXXX");
  temporaryPath = malloc(temporaryPathLength);
  if (temporaryPath == NULL) {
    free(escapedAppImage);
    fprintf(stderr, "Hell's Tower: mémoire insuffisante pour créer le raccourci\n");
    return -1;
  }
  snprintf(temporaryPath, temporaryPathLength, "%s.tmp.XXXXXX", desktopPath);

  descriptor = mkstemp(temporaryPath);
  if (descriptor < 0) {
    fprintf(stderr, "Hell's Tower: impossible de créer %s : %s\n",
            temporaryPath, strerror(errno));
    free(temporaryPath);
    free(escapedAppImage);
    return -1;
  }

  file = fdopen(descriptor, "w");
  if (file == NULL) {
    int savedError = errno;
    close(descriptor);
    unlink(temporaryPath);
    fprintf(stderr, "Hell's Tower: impossible d'écrire le raccourci : %s\n",
            strerror(savedError));
    free(temporaryPath);
    free(escapedAppImage);
    return -1;
  }

  if (fprintf(file,
              "[Desktop Entry]\n"
              "Type=Application\n"
              "Name=Hell's Tower\n"
              "Comment=Jeu d'action 2D\n"
              "Exec=%s\n"
              "TryExec=%s\n"
              "Path=%s\n"
              "Icon=%s\n"
              "Terminal=false\n"
              "Categories=Game;\n",
              escapedAppImage, escapedAppImage, appImageDirectory, iconPath) < 0 ||
      fflush(file) != 0 || fchmod(fileno(file), 0755) != 0)
    succeeded = 0;

  if (fclose(file) != 0)
    succeeded = 0;
  if (succeeded && rename(temporaryPath, desktopPath) != 0)
    succeeded = 0;

  if (!succeeded) {
    int savedError = errno;
    unlink(temporaryPath);
    fprintf(stderr, "Hell's Tower: impossible de mettre à jour %s : %s\n",
            desktopPath, strerror(savedError));
  }

  free(temporaryPath);
  free(escapedAppImage);
  return succeeded ? 0 : -1;
}

void RegisterAppImageDesktopEntry(void) {
  const char *appImagePath = getenv("APPIMAGE");
  const char *appDir = getenv("APPDIR");
  const char *dataHome = getenv("XDG_DATA_HOME");
  const char *home = getenv("HOME");
  char *resolvedAppImage = NULL;
  char *binDirectory = NULL;
  char *appImageDirectory = NULL;
  char *permanentAppImagePath = NULL;
  char *dataDirectory = NULL;
  char *applicationsDirectory = NULL;
  char *iconsDirectory = NULL;
  char *desktopPath = NULL;
  char *iconPath = NULL;

  if (appImagePath == NULL || appImagePath[0] == '\0')
    return;
  if (home == NULL || home[0] == '\0') {
    fprintf(stderr, "Hell's Tower: HOME n'est pas défini, installation ignorée\n");
    return;
  }
  resolvedAppImage = realpath(appImagePath, NULL);
  if (resolvedAppImage == NULL) {
    fprintf(stderr, "Hell's Tower: impossible de résoudre APPIMAGE (%s) : %s\n",
            appImagePath, strerror(errno));
    return;
  }
  if (access(resolvedAppImage, X_OK) != 0) {
    fprintf(stderr, "Hell's Tower: APPIMAGE n'est pas exécutable (%s) : %s\n",
            resolvedAppImage, strerror(errno));
    goto cleanup;
  }

  binDirectory = JoinPath(home, ".local/bin");
  if (binDirectory == NULL) {
    fprintf(stderr, "Hell's Tower: mémoire insuffisante pour le dossier d'installation\n");
    goto cleanup;
  }
  if (MakeDirectories(binDirectory) != 0) {
    fprintf(stderr, "Hell's Tower: impossible de créer %s : %s\n",
            binDirectory, strerror(errno));
    goto cleanup;
  }
  appImageDirectory = realpath(binDirectory, NULL);
  if (appImageDirectory == NULL) {
    fprintf(stderr, "Hell's Tower: impossible de résoudre %s : %s\n",
            binDirectory, strerror(errno));
    goto cleanup;
  }
  permanentAppImagePath = JoinPath(appImageDirectory, "hells-tower.AppImage");
  if (permanentAppImagePath == NULL) {
    fprintf(stderr, "Hell's Tower: mémoire insuffisante pour le chemin d'installation\n");
    goto cleanup;
  }
  if (strcmp(resolvedAppImage, permanentAppImagePath) != 0 &&
      CopyAppImageAtomically(resolvedAppImage, permanentAppImagePath) != 0)
    goto cleanup;

  if (dataHome != NULL && dataHome[0] != '\0') {
    if (dataHome[0] != '/') {
      fprintf(stderr, "Hell's Tower: XDG_DATA_HOME doit être un chemin absolu\n");
      goto cleanup;
    }
    dataDirectory = strdup(dataHome);
  } else {
    if (home == NULL || home[0] == '\0') {
      fprintf(stderr, "Hell's Tower: HOME n'est pas défini, raccourci ignoré\n");
      goto cleanup;
    }
    dataDirectory = JoinPath(home, ".local/share");
  }

  if (dataDirectory == NULL) {
    fprintf(stderr, "Hell's Tower: mémoire insuffisante pour le dossier utilisateur\n");
    goto cleanup;
  }

  applicationsDirectory = JoinPath(dataDirectory, "applications");
  iconsDirectory = JoinPath(dataDirectory, "icons/hicolor/256x256/apps");
  if (applicationsDirectory == NULL || iconsDirectory == NULL) {
    fprintf(stderr, "Hell's Tower: mémoire insuffisante pour les dossiers XDG\n");
    goto cleanup;
  }

  if (MakeDirectories(applicationsDirectory) != 0) {
    fprintf(stderr, "Hell's Tower: impossible de créer %s : %s\n",
            applicationsDirectory, strerror(errno));
    goto cleanup;
  }
  if (MakeDirectories(iconsDirectory) != 0) {
    fprintf(stderr, "Hell's Tower: impossible de créer %s : %s\n",
            iconsDirectory, strerror(errno));
    goto cleanup;
  }
  if (appDir != NULL && appDir[0] != '\0') {
    char *sourceIcon = JoinPath(appDir,
                                "usr/share/icons/hicolor/256x256/apps/hells_tower.png");
    iconPath = JoinPath(iconsDirectory, "hells_tower.png");
    if (sourceIcon == NULL || iconPath == NULL) {
      fprintf(stderr, "Hell's Tower: mémoire insuffisante pour installer l'icône\n");
      free(sourceIcon);
      goto cleanup;
    } else {
      if (CopyFile(sourceIcon, iconPath) != 0) {
        free(sourceIcon);
        goto cleanup;
      }
    }
    free(sourceIcon);
  } else {
    fprintf(stderr, "Hell's Tower: APPDIR n'est pas défini, icône ignorée\n");
    goto cleanup;
  }

  desktopPath = JoinPath(applicationsDirectory, "hells_tower.desktop");
  if (desktopPath == NULL) {
    fprintf(stderr, "Hell's Tower: mémoire insuffisante pour le raccourci\n");
    goto cleanup;
  }
  if (WriteDesktopEntry(desktopPath, permanentAppImagePath, appImageDirectory,
                        iconPath) != 0)
    goto cleanup;

cleanup:
  free(resolvedAppImage);
  free(binDirectory);
  free(appImageDirectory);
  free(permanentAppImagePath);
  free(dataDirectory);
  free(applicationsDirectory);
  free(iconsDirectory);
  free(desktopPath);
  free(iconPath);
}

#endif
