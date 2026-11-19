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

void AchievementsInit(void);
bool AchievementsUnlock(AchievementId achievement);
void AchievementsDrawNotifications(void);
bool AchievementsDrawScreen(void);

#endif
