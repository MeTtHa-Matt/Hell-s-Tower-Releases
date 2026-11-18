#ifndef UPDATER_H
#define UPDATER_H

#include <stdbool.h>

void UpdaterInitialize(void);
void UpdaterStartCheck(void);
bool UpdaterCheckFinished(void);
bool UpdaterUpdateAvailable(void);
bool UpdaterHasPendingUpdate(void);
const char *UpdaterLatestVersion(void);
const char *UpdaterPendingVersion(void);
void UpdaterPersistUpdate(const char *version);
void UpdaterShutdown(void);

#endif
