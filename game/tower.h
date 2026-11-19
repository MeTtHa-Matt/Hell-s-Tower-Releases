#ifndef TOWER_H
#define TOWER_H

#include <stdbool.h>
#include <stddef.h>

void Tower(void);
void UpdateTowerMusic(void);
void ResetTower(void);
bool LoadTowerProgress(void);
bool SaveTowerProgress(void);
bool TowerBuildDataFilePath(const char *filename, char *directory,
                            size_t directorySize, char *filePath,
                            size_t filePathSize);
bool DeleteTowerProgressFile(void);
void ResetTowerProgress(void);
bool TowerProgressNeedsSaving(void);
bool SetTowerPlayerCharacter(int character);
int TowerPlayerCharacter(void);
bool TowerTutorialSeen(void);
void MarkTowerTutorialSeen(void);
int TowerCurrentFloor(void);
int TowerEtris(void);
void TowerAddEtris(long long amount);
bool TowerSpendEtris(int amount);
void TowerDrawEtrisHud(bool alwaysVisible);
bool TowerTakeExitRequest(void);
bool TowerResultActive(void);
bool TowerSigridDialogSeen(int floor);
void MarkTowerSigridDialogSeen(int floor);

#endif
