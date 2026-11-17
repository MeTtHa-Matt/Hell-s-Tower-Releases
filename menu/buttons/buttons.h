#include "raylib.h"
#include <stdbool.h>

extern bool game_running;

void StartPlayingButton(void);
void OptionsButton(void);
void QuitButton(void);
bool StartPlayingRequested(void);
void ClearStartPlayingRequest(void);
void Button(const char *name);
