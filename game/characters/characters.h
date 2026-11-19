#ifndef GAME_CHARACTERS_H
#define GAME_CHARACTERS_H

#include <stddef.h>

const char *CharacterSkillName(int character);
const char *CharacterSkillDescription(int character);
const char *CharacterTitleName(int character);
const char *CharacterTitleDescription(int character);
void CharacterPlayerTitlesText(char *buffer, size_t capacity);
void CharacterApplyForGame(int character);
float CharacterExperienceMultiplier(int character);
int CharacterItemsPerLevel(int character);
float CharacterEtrisMultiplier(int character);

#endif
