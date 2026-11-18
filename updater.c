#include "updater.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#endif

#include <curl/curl.h>
#include <raylib.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef APP_VERSION
#define APP_VERSION "v0.0.0-dev"
#endif

#define RELEASE_API_URL \
  "https://api.github.com/repos/MeTtHa-Matt/Hell-s-Tower-Releases/releases/latest"
#define RESPONSE_LIMIT (1024U * 1024U)
#define UPDATE_PATH_CAPACITY 2048

#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION updaterMutex;
static bool mutexInitialized;
static HANDLE updaterThread;
#else
#include <pthread.h>
static pthread_mutex_t updaterMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t updaterThread;
static bool threadCreated;
#endif

static bool checkFinished;
static bool updateAvailable;
static bool pendingUpdate;
static bool curlInitialized;
static char latestVersion[64];
static char pendingVersion[64];

typedef struct {
  char *data;
  size_t size;
} ResponseBuffer;

typedef struct {
  unsigned int major;
  unsigned int minor;
  unsigned int patch;
} SemanticVersion;

static void LockUpdater(void) {
#ifdef _WIN32
  EnterCriticalSection(&updaterMutex);
#else
  pthread_mutex_lock(&updaterMutex);
#endif
}

static void UnlockUpdater(void) {
#ifdef _WIN32
  LeaveCriticalSection(&updaterMutex);
#else
  pthread_mutex_unlock(&updaterMutex);
#endif
}

static void SetResult(bool isAvailable, const char *version) {
  LockUpdater();
  updateAvailable = isAvailable;
  if (version)
    snprintf(latestVersion, sizeof(latestVersion), "%s", version);
  checkFinished = true;
  UnlockUpdater();
}

static size_t ReceiveResponse(char *contents, size_t size, size_t count,
                              void *userData) {
  ResponseBuffer *buffer = userData;
  if (size != 0 && count > (size_t)-1 / size)
    return 0;
  size_t received = size * count;
  if (received > RESPONSE_LIMIT - buffer->size)
    return 0;

  char *larger = realloc(buffer->data, buffer->size + received + 1);
  if (!larger)
    return 0;
  buffer->data = larger;
  memcpy(buffer->data + buffer->size, contents, received);
  buffer->size += received;
  buffer->data[buffer->size] = '\0';
  return received;
}

static const char *SkipWhitespace(const char *text) {
  while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
    text++;
  return text;
}

static bool ExtractTagName(const char *json, char *tag, size_t tagCapacity) {
  const char *field = strstr(json, "\"tag_name\"");
  if (!field)
    return false;
  field = strchr(field + strlen("\"tag_name\""), ':');
  if (!field)
    return false;
  field = SkipWhitespace(field + 1);
  if (*field++ != '"')
    return false;

  size_t length = 0;
  while (*field && *field != '"') {
    if (*field == '\\' || length + 1 >= tagCapacity)
      return false;
    tag[length++] = *field++;
  }
  if (*field != '"' || length == 0)
    return false;
  tag[length] = '\0';
  return true;
}

static bool ParseVersion(const char *tag, SemanticVersion *version) {
  if (*tag == 'v' || *tag == 'V')
    tag++;

  unsigned int *parts[] = {&version->major, &version->minor, &version->patch};
  for (size_t i = 0; i < 3; i++) {
    if (*tag < '0' || *tag > '9')
      return false;
    char *end = NULL;
    unsigned long value = strtoul(tag, &end, 10);
    if (end == tag || value > UINT_MAX)
      return false;
    *parts[i] = (unsigned int)value;
    tag = end;
    if (i < 2) {
      if (*tag++ != '.')
        return false;
    }
  }

  return *tag == '\0' || *tag == '-' || *tag == '+';
}

static bool VersionIsNewer(const char *remoteTag) {
  SemanticVersion remote, current;
  if (!ParseVersion(remoteTag, &remote) || !ParseVersion(APP_VERSION, &current))
    return false;

  if (remote.major != current.major)
    return remote.major > current.major;
  if (remote.minor != current.minor)
    return remote.minor > current.minor;
  return remote.patch > current.patch;
}

static bool BuildPendingUpdatePath(char *directory, size_t directoryCapacity,
                                   char *path, size_t pathCapacity) {
  const char *baseDirectory;
  int directoryLength;

#ifdef _WIN32
  baseDirectory = getenv("LOCALAPPDATA");
  if (!baseDirectory || !baseDirectory[0])
    baseDirectory = getenv("APPDATA");
  if (!baseDirectory || !baseDirectory[0])
    return false;
  directoryLength =
      snprintf(directory, directoryCapacity, "%s/HellsTower", baseDirectory);
#elif __APPLE__
  baseDirectory = getenv("HOME");
  if (!baseDirectory || !baseDirectory[0])
    return false;
  directoryLength = snprintf(directory, directoryCapacity,
                             "%s/Library/Application Support/Hells Tower",
                             baseDirectory);
#else
  baseDirectory = getenv("XDG_DATA_HOME");
  if (baseDirectory && baseDirectory[0] == '/') {
    directoryLength = snprintf(directory, directoryCapacity,
                               "%s/hells-tower", baseDirectory);
  } else {
    baseDirectory = getenv("HOME");
    if (!baseDirectory || !baseDirectory[0])
      return false;
    directoryLength = snprintf(directory, directoryCapacity,
                               "%s/.local/share/hells-tower", baseDirectory);
  }
#endif

  if (directoryLength < 0 || (size_t)directoryLength >= directoryCapacity)
    return false;
  int pathLength =
      snprintf(path, pathCapacity, "%s/pending_update.txt", directory);
  return pathLength >= 0 && (size_t)pathLength < pathCapacity;
}

static void ClearPendingUpdateFile(const char *path) {
  if (remove(path) != 0 && errno != ENOENT)
    TraceLog(LOG_WARNING, "Updater: impossible d'effacer %s", path);
}

void UpdaterInitialize(void) {
  char directory[UPDATE_PATH_CAPACITY];
  char path[UPDATE_PATH_CAPACITY];
  char storedVersion[sizeof(pendingVersion)];
  SemanticVersion stored, current;
  pendingUpdate = false;
  pendingVersion[0] = '\0';

  if (!BuildPendingUpdatePath(directory, sizeof(directory), path, sizeof(path))) {
    TraceLog(LOG_WARNING, "Updater: impossible de trouver le fichier de mise à jour");
    return;
  }

  FILE *file = fopen(path, "r");
  if (!file) {
    if (errno != ENOENT)
      TraceLog(LOG_WARNING, "Updater: impossible de lire %s", path);
    return;
  }
  bool readVersion = fgets(storedVersion, sizeof(storedVersion), file) != NULL;
  fclose(file);
  if (!readVersion) {
    TraceLog(LOG_WARNING, "Updater: version requise illisible dans %s", path);
    ClearPendingUpdateFile(path);
    return;
  }
  storedVersion[strcspn(storedVersion, "\r\n")] = '\0';

  if (!ParseVersion(storedVersion, &stored) ||
      !ParseVersion(APP_VERSION, &current)) {
    TraceLog(LOG_WARNING, "Updater: version invalide dans %s", path);
    ClearPendingUpdateFile(path);
    return;
  }
  if (!VersionIsNewer(storedVersion)) {
    ClearPendingUpdateFile(path);
    return;
  }

  pendingUpdate = true;
  snprintf(pendingVersion, sizeof(pendingVersion), "%s", storedVersion);
}

bool UpdaterHasPendingUpdate(void) {
  return pendingUpdate;
}

const char *UpdaterPendingVersion(void) {
  return pendingVersion;
}

void UpdaterPersistUpdate(const char *version) {
  SemanticVersion parsedVersion;
  if (!version || !ParseVersion(version, &parsedVersion) ||
      !VersionIsNewer(version))
    return;

  pendingUpdate = true;
  snprintf(pendingVersion, sizeof(pendingVersion), "%s", version);

  char directory[UPDATE_PATH_CAPACITY];
  char path[UPDATE_PATH_CAPACITY];
  if (!BuildPendingUpdatePath(directory, sizeof(directory), path, sizeof(path))) {
    TraceLog(LOG_WARNING, "Updater: impossible de déterminer le chemin de sauvegarde");
    return;
  }
  if (!DirectoryExists(directory) && MakeDirectory(directory) != 0 &&
      !DirectoryExists(directory)) {
    TraceLog(LOG_WARNING, "Updater: impossible de créer %s", directory);
    return;
  }

  FILE *file = fopen(path, "w");
  if (!file) {
    TraceLog(LOG_WARNING, "Updater: impossible d'enregistrer %s", path);
    return;
  }
  bool written = fprintf(file, "%s\n", version) > 0;
  if (fclose(file) != 0)
    written = false;
  if (!written)
    TraceLog(LOG_WARNING, "Updater: échec de l'enregistrement de %s", path);
}

static void CheckLatestRelease(void) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    TraceLog(LOG_WARNING, "Updater: impossible d'initialiser la requête HTTP");
    SetResult(false, NULL);
    return;
  }

  ResponseBuffer response = {0};
  curl_easy_setopt(curl, CURLOPT_URL, RELEASE_API_URL);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hells-Tower-Updater/1.0");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 4000L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 4000L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReceiveResponse);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  CURLcode result = curl_easy_perform(curl);
  long statusCode = 0;
  if (result == CURLE_OK)
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

  bool available = false;
  char tag[sizeof(latestVersion)] = {0};
  if (result == CURLE_OK && statusCode >= 200 && statusCode < 300) {
    SemanticVersion parsedTag;
    if (response.data && ExtractTagName(response.data, tag, sizeof(tag)) &&
        ParseVersion(tag, &parsedTag)) {
      available = VersionIsNewer(tag);
    } else {
      TraceLog(LOG_WARNING, "Updater: réponse de release GitHub invalide");
    }
  } else {
    TraceLog(LOG_WARNING, "Updater: vérification ignorée (%s, HTTP %ld)",
             curl_easy_strerror(result), statusCode);
  }

  curl_easy_cleanup(curl);
  free(response.data);
  SetResult(available, available ? tag : NULL);
}

#ifdef _WIN32
static DWORD WINAPI UpdaterThread(void *unused) {
  (void)unused;
  CheckLatestRelease();
  return 0;
}
#else
static void *UpdaterThread(void *unused) {
  (void)unused;
  CheckLatestRelease();
  return NULL;
}
#endif

void UpdaterStartCheck(void) {
#ifdef _WIN32
  if (!mutexInitialized) {
    InitializeCriticalSection(&updaterMutex);
    mutexInitialized = true;
  }
  if (updaterThread) {
    WaitForSingleObject(updaterThread, INFINITE);
    CloseHandle(updaterThread);
    updaterThread = NULL;
  }
#else
  if (threadCreated) {
    pthread_join(updaterThread, NULL);
    threadCreated = false;
  }
#endif

  LockUpdater();
  checkFinished = false;
  updateAvailable = false;
  latestVersion[0] = '\0';
  UnlockUpdater();

  if (!curlInitialized) {
    CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (result != CURLE_OK) {
      TraceLog(LOG_WARNING, "Updater: impossible d'initialiser libcurl");
      SetResult(false, NULL);
      return;
    }
    curlInitialized = true;
  }

#ifdef _WIN32
  updaterThread = CreateThread(NULL, 0, UpdaterThread, NULL, 0, NULL);
  if (!updaterThread) {
    TraceLog(LOG_WARNING, "Updater: impossible de démarrer le thread");
    SetResult(false, NULL);
  }
#else
  if (pthread_create(&updaterThread, NULL, UpdaterThread, NULL) != 0) {
    TraceLog(LOG_WARNING, "Updater: impossible de démarrer le thread");
    SetResult(false, NULL);
  } else {
    threadCreated = true;
  }
#endif
}

bool UpdaterCheckFinished(void) {
  bool finished;
  LockUpdater();
  finished = checkFinished;
  UnlockUpdater();
  return finished;
}

bool UpdaterUpdateAvailable(void) {
  bool available;
  LockUpdater();
  available = updateAvailable;
  UnlockUpdater();
  return available;
}

const char *UpdaterLatestVersion(void) {
  return latestVersion;
}

void UpdaterShutdown(void) {
#ifdef _WIN32
  if (updaterThread) {
    WaitForSingleObject(updaterThread, INFINITE);
    CloseHandle(updaterThread);
    updaterThread = NULL;
  }
  if (mutexInitialized) {
    DeleteCriticalSection(&updaterMutex);
    mutexInitialized = false;
  }
#else
  if (threadCreated) {
    pthread_join(updaterThread, NULL);
    threadCreated = false;
  }
#endif
  if (curlInitialized) {
    curl_global_cleanup();
    curlInitialized = false;
  }
}
