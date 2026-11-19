#ifndef CHARACTER_SELECTION_H
#define CHARACTER_SELECTION_H

#include <stdbool.h>

void InitCharacterSelection(void);
void StartCharacterSelection(void);
bool DrawCharacterSelection(void);
int CharacterSelectionPlayer(void);
void CharacterSelectionSaveFailed(void);
void UnloadCharacterSelection(void);

#endif
