#ifndef TOWER_H
#define TOWER_H

#include <stdbool.h>

void Tower(void);
void UpdateTowerMusic(void);
void ResetTower(void);
void LoadTowerProgress(void);
bool SaveTowerProgress(void);
bool DeleteTowerProgressFile(void);
void ResetTowerProgress(void);
bool TowerProgressNeedsSaving(void);
bool TowerTutorialSeen(void);
void MarkTowerTutorialSeen(void);
int TowerCurrentFloor(void);
bool TowerTakeExitRequest(void);
bool TowerResultActive(void);
bool TowerSigridDialogSeen(int floor);
void MarkTowerSigridDialogSeen(int floor);

#endif
