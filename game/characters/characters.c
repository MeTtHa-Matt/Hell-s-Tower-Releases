#include "characters.h"
#include "../achievements/achievements.h"
#include "../items/functionnal/functionnal.h"
#include <stdio.h>
#include <string.h>

static const char *skills[6][2] = {
    {"Flamme du dernier souvenir",
     "Obtient un artefact supplémentaire à chaque niveau gagné."},
    {"Chance du débutant",
     "Double les chances d'obtenir un effet Épique ou supérieur, quel que soit le mini-jeu."},
    {"Pas du veilleur",
     "Gagne 15 % d'expérience supplémentaire après chaque combat."},
    {"Rempart ultime",
     "Gagne 10 % d'Etris supplémentaire après chaque combat."},
    {"Œil du veilleur",
     "Obtient un artefact supplémentaire à chaque niveau gagné."},
    {"Sceau du médecin",
     "Gagne 15 % d'Etris supplémentaire après chaque combat."},
};

static const char *titles[6][2] = {
    {"L'enfant préférée de la tour",
     "Double l'expérience et les artefacts obtenus à chaque niveau."},
    {"Oracle de la lune", "Gagne 20 % d'Etris supplémentaires par victoire."},
    {"Épée de l'écuyer", "Gagne 10 % d'expérience supplémentaire."},
    {"Héritier du bouclier brisé", "Gagne 10 % d'Etris supplémentaire."},
    {"Archiviste des runes",
     "Obtient un artefact supplémentaire à chaque niveau gagné."},
    {"Médecin de la cour", "Gagne 15 % d'expérience supplémentaire."},
};

const char *CharacterSkillName(int character) {
  return character >= 0 && character < 6 ? skills[character][0] : "";
}

const char *CharacterSkillDescription(int character) {
  return character >= 0 && character < 6 ? skills[character][1] : "";
}

const char *CharacterTitleName(int character) {
  return character >= 0 && character < 6 ? titles[character][0] : "";
}

const char *CharacterTitleDescription(int character) {
  return character >= 0 && character < 6 ? titles[character][1] : "";
}

void CharacterPlayerTitlesText(char *buffer, size_t capacity) {
  if (!buffer || capacity == 0)
    return;
  buffer[0] = '\0';
  int count = AchievementsTitleCount();
  if (count == 0) {
    snprintf(buffer, capacity, "Aucun titre personnel débloqué.");
    return;
  }
  for (int i = 0; i < count; i++) {
    size_t length = strlen(buffer);
    snprintf(buffer + length, capacity - length, "%s%s", i ? " / " : "",
             AchievementsTitleName(i));
  }
}

void CharacterApplyForGame(int character) {
  FunctionalApplyCharacter(character);
  FunctionalApplyPlayerTitle(AchievementsEquippedTitle());
  if (character >= 0 && character < 6)
    FunctionalApplyCharacterTitle(character);
}

float CharacterExperienceMultiplier(int character) {
  float multiplier = FunctionalGet()->experienceMultiplier;
  return character == 0 ? multiplier * 2 : multiplier;
}

int CharacterItemsPerLevel(int character) {
  int count = character == 0 ? 2 : 1;
  return count + FunctionalGet()->bonusLevelItems;
}

float CharacterEtrisMultiplier(int character) {
  return character == 1 ? 1.2f : 1;
}
