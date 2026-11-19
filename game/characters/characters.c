#include "characters.h"
#include "../achievements/achievements.h"
#include "../items/functionnal/functionnal.h"
#include <stdio.h>
#include <string.h>

static const char *skills[6][2] = {
    {"Flamme du dernier souvenir",
     "Gagne 0,5 s d'invulnérabilité après chaque dégât."},
    {"Chance du débutant",
     "Double les chances d'obtenir un effet Épique ou supérieur."},
    {"Pas du veilleur", "Se déplace 20 % plus vite dans les mini-jeux."},
    {"Rempart ultime",
     "Annule une fois un dégât fatal quand il reste une vie."},
    {"Reflet maîtrisé",
     "Ralentit de 20 % les projectiles du côté droit dans Mirror."},
    {"Sceau du médecin", "Charge les sceaux 20 % plus vite."},
};

static const char *titles[6][2] = {
    {"L'enfant préférée de la tour",
     "Double l'expérience et les artefacts obtenus à chaque niveau."},
    {"Oracle de la lune", "Gagne 20 % d'Etris supplémentaires par victoire."},
    {"Épée de l'écuyer", "Commence chaque mini-jeu avec un bouclier."},
    {"Héritier du bouclier brisé", "Commence chaque mini-jeu avec une vie."},
    {"Archiviste des runes", "Ajoute 2 secondes au chronomètre."},
    {"Médecin de la cour",
     "Prolonge de 0,3 s les avertissements des sceaux."},
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
  for (int i = 0; i < TITLE_COUNT; i++)
    if (AchievementsTitleUnlocked((TitleId)i))
      FunctionalApplyPlayerTitle(i);
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
