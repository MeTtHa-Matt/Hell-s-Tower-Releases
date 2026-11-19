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
  ACHIEVEMENT_COUNT
} AchievementId;

typedef enum {
  TITLE_CHANGED_FATE,
  TITLE_FOUND_TRUTH,
  TITLE_FIRST_ASCENT,
  TITLE_TOWER_CONQUEROR,
  TITLE_UNTOUCHED,
  TITLE_COUNT
} TitleId;

void AchievementsInit(void);
bool AchievementsUnlock(AchievementId achievement);
bool AchievementsUnlockTitle(TitleId title);
bool AchievementsTitleUnlocked(TitleId title);
int AchievementsTitleCount(void);
const char *AchievementsTitleName(int index);
bool AchievementsTitleNotificationPending(void);
void AchievementsDrawNotifications(void);
void AchievementsDrawTitleNotification(void);
bool AchievementsDrawScreen(void);

#endif
