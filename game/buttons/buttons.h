#ifndef GAME_BUTTONS_H
#define GAME_BUTTONS_H

#include <stdbool.h>

void DrawGameButtons(bool acceptInput);
void UpdateGameButtonsMusic(void);
void ResetGameButtons(void);
void UnloadGameButtons(void);
bool GameReturnToMenuRequested(void);
bool GameSubscreenActive(void);
bool GameTrainingActive(void);
bool GameTrainingSelectionActive(void);
bool GameInventoryActive(void);
bool GameResultActive(void);
bool GameReturnConfirmationActive(void);
bool GameSaveAlertActive(void);
bool GameMelinaEndingRequested(void);
bool GameFinishMelinaEnding(void);
void UpdateGameSaveAlert(void);
void DrawGameSaveAlert(void);

#endif
