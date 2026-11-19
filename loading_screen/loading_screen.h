#ifndef LOADING_SCREEN_H
#define LOADING_SCREEN_H

#include <stdbool.h>

typedef void (*LoadingScreenAction)(void);

void LoadingScreenInit(void);
void LoadingScreenStart(LoadingScreenAction action);
void LoadingScreenStartWithUpdateCheck(LoadingScreenAction action);
void LoadingScreenStartWithBackground(LoadingScreenAction action,
                                     const char *backgroundPath);
void LoadingScreenStartWithBackgroundNoProgress(LoadingScreenAction action,
                                                const char *backgroundPath);
void LoadingScreenUpdate(void);
bool LoadingScreenDraw(void);
bool LoadingScreenActive(void);
bool LoadingScreenUpdatePopupActive(void);
void LoadingScreenUnload(void);

#endif
