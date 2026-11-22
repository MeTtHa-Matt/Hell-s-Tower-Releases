#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H

#include <stdbool.h>

typedef enum {
  ACHIEVEMENT_COMPLETE_GAME,
  ACHIEVEMENT_MELINA_ENDING,
  ACHIEVEMENT_LEGENDARY_ITEM,
  ACHIEVEMENT_FLOOR_10,
  ACHIEVEMENT_FLOOR_20,
  ACHIEVEMENT_FLOOR_30,
  ACHIEVEMENT_FLOOR_40,
  ACHIEVEMENT_FLOOR_50,
  ACHIEVEMENT_CHANGED_FATE,
  ACHIEVEMENT_FIRST_VICTORY,
  ACHIEVEMENT_FLOOR_5,
  ACHIEVEMENT_FLOOR_15,
  ACHIEVEMENT_FLOOR_25,
  ACHIEVEMENT_FLOOR_35,
  ACHIEVEMENT_FLOOR_45,
  ACHIEVEMENT_UNTOUCHED_BOSS,
  ACHIEVEMENT_LEVEL_10,
  ACHIEVEMENT_FOUND_LETTER,
  ACHIEVEMENT_FINAL_BOSS_PERFECT,
  ACHIEVEMENT_ALL_BOSSES_PERFECT,
  ACHIEVEMENT_ALL_CHARACTERS_COMPLETE,
  ACHIEVEMENT_ALL_BOSSES_ALL_CHARACTERS,
  ACHIEVEMENT_COUNT
} AchievementId;

typedef enum {
  TITLE_CHANGED_FATE,
  TITLE_FOUND_TRUTH,
  TITLE_FIRST_ASCENT,
  TITLE_TOWER_CONQUEROR,
  TITLE_UNTOUCHED,
  TITLE_BOSS_HUNTER,
  TITLE_VENGEANCE,
  TITLE_ASHEN_CROWN,
  TITLE_FORTUNE,
  TITLE_FINAL_EDGE,
  TITLE_PERFECT_CROWN,
  TITLE_SIX_DESTINIES,
  TITLE_BOSS_ARCHIVIST,
  TITLE_COUNT
} TitleId;

void AchievementsInit(void);
bool AchievementsUnlock(AchievementId achievement);
bool AchievementsUnlockTitle(TitleId title);
bool AchievementsRecordBossVictory(int character, int floor, bool perfect);
bool AchievementsRecordEnding(int character);
bool AchievementsTitleUnlocked(TitleId title);
int AchievementsTitleCount(void);
int AchievementsUnlockedTitleAt(int index);
int AchievementsEquippedTitle(void);
bool AchievementsSetEquippedTitle(int title);
const char *AchievementsTitleName(int index);
const char *AchievementsTitleDescription(int index);
bool AchievementsTitleNotificationPending(void);
void AchievementsDrawNotifications(void);
void AchievementsDrawTitleNotification(void);
bool AchievementsDrawScreen(void);
void AchievementsStartTitleSelection(void);
bool AchievementsDrawTitleSelection(void);

#endif
