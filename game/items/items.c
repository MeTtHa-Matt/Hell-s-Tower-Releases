#include "items.h"
#include "../achievements/achievements.h"
#include "drop_rates.h"
#include "functionnal/functionnal.h"
#include "../tower.h"
#include "../ui_scale.h"
#include "raylib.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define EQUIPPED_COUNT 5
#define STORAGE_VISIBLE_COUNT 25
#define STORAGE_COLUMNS 5
#define EFFECT_LIMIT 5
#define NEW_ITEM_EFFECT_LIMIT 3
#define ITEM_NAME_LENGTH 128
#define EFFECT_LIMIT_COUNT 128

typedef struct {
  char text[256];
  char rarity[32];
  char game[32];
} ItemEffect;

typedef struct {
  char name[ITEM_NAME_LENGTH];
  int effects[EFFECT_LIMIT];
  int effectCount;
  int useLimit;
  int usesLeft;
  Texture2D texture;
} Item;

typedef enum { QUEST_EMPTY, QUEST_LETTER, QUEST_SEAL } QuestItemType;
typedef enum {
  UNIQUE_CONVERSION_CLOSED,
  UNIQUE_CONVERSION_NOTICE,
  UNIQUE_CONVERSION_ANIMATION,
  UNIQUE_CONVERSION_RESULT
} UniqueConversionState;

typedef struct {
  QuestItemType type;
  int bossFloor;
  Texture2D texture;
} QuestItem;

static Item *items;
static int itemCapacity;
static int itemSlotCount = EQUIPPED_COUNT;
static int highestStoredSlot = -1;
static int rulesFloor = -1;
static bool rulesBossFloor;
static char rulesGame[32];
static int storageScrollOffset;
static float storageWheelRemainder;
static bool storageScrollDragging;
static float storageScrollGrab;
static int equipWarningSlot = -1;
static float equipWarningTimer;
static UniqueConversionState uniqueConversionState;
static int uniqueConversionSlot = -1;
static int uniqueConversionEffect = -1;
static float uniqueConversionTimer;
static bool uniqueConversionFailed;
static Item pendingUniqueItem;
static QuestItem questItems[QUEST_ITEM_COUNT];
static ItemEffect effects[EFFECT_LIMIT_COUNT];
static char itemNames[128][ITEM_NAME_LENGTH];
static int effectCount, itemNameCount;
static char knownItems[128][ITEM_NAME_LENGTH];
static Texture2D knownPreviewTextures[128];
static int knownItemCount;
static bool knownEffects[EFFECT_LIMIT_COUNT], collectionLoaded;
static bool dataLoaded;
static int rewardSlot = -1;
static int rewardQueue[5];
static int rewardQueueCount, rewardQueueIndex;
static int questRewardSlot = -1;
static float rewardTimer;
static int contextSlot = -1;
static int questContextSlot = -1;
static unsigned int bossSealMask;
static Rectangle inventoryContextBox;
static int letterSealsUsed;
static bool letterAwarded, letterRead, letterReaderActive, letterReachedEnd;
static char *letterText;
static Font letterFont;
static bool letterFontReady;
static float letterScroll, letterMaxScroll;
static const char *rarityNames[DROP_RARITY_COUNT] = {
    "COMMUN", "RARE", "EPIQUE", "LEGENDAIRE", "MAUDIT", "UNIQUE",
};

static void CloseLetterReader(void);
static int PickEffectForItem(const Item *item, int selectedCount);
static void UnloadItem(Item *item);
static void CompactStoredItems(void);
static void ResetUniqueConversion(void);

static void ShowEquipWarning(int slot) {
  equipWarningSlot = slot;
  equipWarningTimer = 0.65f;
}

static bool ReserveItemCapacity(int required) {
  if (required < 0)
    return false;
  if (required <= itemCapacity)
    return true;
  int capacity = itemCapacity > 0 ? itemCapacity : EQUIPPED_COUNT + STORAGE_VISIBLE_COUNT;
  while (capacity < required) {
    if (capacity > INT_MAX / 2) {
      capacity = required;
      break;
    }
    capacity *= 2;
  }
  if ((size_t)capacity > SIZE_MAX / sizeof(*items)) {
    TraceLog(LOG_ERROR, "Capacite d'inventaire trop importante");
    return false;
  }
  Item *resized = realloc(items, (size_t)capacity * sizeof(*items));
  if (!resized) {
    TraceLog(LOG_ERROR, "Impossible d'agrandir l'inventaire");
    return false;
  }
  memset(resized + itemCapacity, 0,
         (size_t)(capacity - itemCapacity) * sizeof(*items));
  items = resized;
  itemCapacity = capacity;
  return true;
}

static bool EnsureItemSlots(int count) {
  if (count < EQUIPPED_COUNT || !ReserveItemCapacity(count))
    return false;
  if (count > itemSlotCount)
    itemSlotCount = count;
  return true;
}

static int FindEmptyStorage(void) {
  if (!EnsureItemSlots(EQUIPPED_COUNT))
    return -1;
  CompactStoredItems();
  int slot = highestStoredSlot < EQUIPPED_COUNT
                 ? EQUIPPED_COUNT
                 : highestStoredSlot + 1;
  if (slot == INT_MAX || !EnsureItemSlots(slot + 1))
    return -1;
  return slot;
}

static int HighestStoredSlot(void) {
  return highestStoredSlot;
}

static void RecalculateHighestStoredSlot(void) {
  highestStoredSlot = -1;
  for (int i = itemSlotCount - 1; i >= EQUIPPED_COUNT; i--)
    if (items[i].name[0]) {
      highestStoredSlot = i;
      return;
    }
}

static void MoveItemToSlot(int source, int destination) {
  bool compactStorage =
      source >= EQUIPPED_COUNT && destination < EQUIPPED_COUNT;
  items[destination] = items[source];
  items[source] = (Item){0};
  if (source >= EQUIPPED_COUNT && source == highestStoredSlot)
    RecalculateHighestStoredSlot();
  if (destination >= EQUIPPED_COUNT && destination > highestStoredSlot)
    highestStoredSlot = destination;
  if (rewardSlot == source)
    rewardSlot = destination;
  for (int i = 0; i < rewardQueueCount; i++)
    if (rewardQueue[i] == source)
      rewardQueue[i] = destination;
  if (contextSlot == source)
    contextSlot = destination;
  if (equipWarningSlot == source)
    equipWarningSlot = destination;
  if (compactStorage)
    CompactStoredItems();
}

static void CompactStoredItems(void) {
  if (!items)
    return;
  int destination = EQUIPPED_COUNT;
  int lastSlot = highestStoredSlot;
  for (int source = EQUIPPED_COUNT; source <= lastSlot; source++) {
    if (!items[source].name[0])
      continue;
    if (source != destination)
      MoveItemToSlot(source, destination);
    destination++;
  }
  highestStoredSlot =
      destination == EQUIPPED_COUNT ? -1 : destination - 1;
}

static bool StoreEquippedItems(void) {
  int count = 0;
  for (int i = 0; i < EQUIPPED_COUNT; i++)
    count += items[i].name[0] != '\0';
  if (count == 0)
    return true;

  int lastSlot = highestStoredSlot >= EQUIPPED_COUNT ? highestStoredSlot
                                                      : EQUIPPED_COUNT - 1;
  if (count > INT_MAX - lastSlot - 1 ||
      !ReserveItemCapacity(lastSlot + count + 1))
    return false;

  for (int i = 0; i < EQUIPPED_COUNT; i++) {
    if (!items[i].name[0])
      continue;
    int destination = FindEmptyStorage();
    if (destination < 0)
      return false;
    MoveItemToSlot(i, destination);
  }
  return true;
}

static bool CollectionPath(char *directory, size_t directorySize,
                           char *path, size_t pathSize) {
  if (!TowerBuildDataFilePath("collection.txt", directory, directorySize,
                              path, pathSize)) {
    TraceLog(LOG_ERROR, "Impossible de trouver le dossier de collection");
    return false;
  }
  if (!DirectoryExists(directory) && MakeDirectory(directory) != 0 &&
      !DirectoryExists(directory)) {
    TraceLog(LOG_ERROR, "Impossible de creer le dossier de collection");
    return false;
  }
  return true;
}

static void SaveCollection(void) {
  char directory[2048], path[2048], data[8192] = "";
  size_t used = 0;
  if (!CollectionPath(directory, sizeof(directory), path, sizeof(path)))
    return;
  for (int i = 0; i < knownItemCount; i++) {
    int written = snprintf(data + used, sizeof(data) - used, "I:%s\n",
                           knownItems[i]);
    if (written < 0 || (size_t)written >= sizeof(data) - used) {
      TraceLog(LOG_ERROR, "Collection d'artefacts trop volumineuse");
      return;
    }
    used += (size_t)written;
  }
  for (int i = 0; i < effectCount; i++)
    if (knownEffects[i]) {
      int written = snprintf(data + used, sizeof(data) - used, "E:%d\n", i);
      if (written < 0 || (size_t)written >= sizeof(data) - used) {
        TraceLog(LOG_ERROR, "Collection d'effets trop volumineuse");
        return;
      }
      used += (size_t)written;
    }
  if (!SaveFileText(path, data))
    TraceLog(LOG_ERROR, "Impossible d'ecrire la collection: %s", path);
}

static void LoadCollection(void) {
  if (collectionLoaded)
    return;
  collectionLoaded = true;
  char directory[2048], path[2048];
  if (!TowerBuildDataFilePath("collection.txt", directory, sizeof(directory),
                              path, sizeof(path))) {
    TraceLog(LOG_ERROR, "Impossible de trouver le dossier de collection");
    return;
  }
  if (!FileExists(path))
    return;
  FILE *file = fopen(path, "r");
  if (!file) {
    TraceLog(LOG_ERROR, "Impossible de lire la collection: %s", path);
    return;
  }
  char line[ITEM_NAME_LENGTH + 16];
  while (fgets(line, sizeof(line), file)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (strncmp(line, "I:", 2) == 0 && line[2] &&
        strlen(line + 2) < ITEM_NAME_LENGTH &&
        !strchr(line + 2, '/') && !strchr(line + 2, '\\') &&
        FileExists(TextFormat("assets/items/%s.png", line + 2))) {
      bool found = false;
      for (int i = 0; i < knownItemCount; i++)
        found |= strcmp(knownItems[i], line + 2) == 0;
      if (!found && knownItemCount < 128)
        snprintf(knownItems[knownItemCount++], ITEM_NAME_LENGTH, "%s",
                 line + 2);
    } else if (strncmp(line, "E:", 2) == 0) {
      int index = atoi(line + 2);
      if (index >= 0 && index < EFFECT_LIMIT_COUNT)
        knownEffects[index] = true;
    }
  }
  fclose(file);
}

static void RememberItem(const Item *item) {
  if (!item || !item->name[0])
    return;
  LoadCollection();
  bool changed = false, found = false;
  for (int i = 0; i < knownItemCount; i++)
    found |= strcmp(knownItems[i], item->name) == 0;
  if (!found && knownItemCount < 128) {
    snprintf(knownItems[knownItemCount++], ITEM_NAME_LENGTH, "%s", item->name);
    changed = true;
  }
  for (int i = 0; i < item->effectCount; i++) {
    int index = item->effects[i];
    if (index >= 0 && index < effectCount && !knownEffects[index]) {
      knownEffects[index] = true;
      changed = true;
    }
  }
  if (changed)
    SaveCollection();
}

static bool CreateRandomItem(Item *item) {
  if (itemNameCount == 0 || effectCount == 0)
    return false;
  snprintf(item->name, sizeof(item->name), "%s",
           itemNames[GetRandomValue(0, itemNameCount - 1)]);
  item->useLimit = GetRandomValue(4, 8);
  item->usesLeft = item->useLimit;
  item->effectCount = GetRandomValue(1, NEW_ITEM_EFFECT_LIMIT);
  if (item->effectCount > effectCount)
    item->effectCount = effectCount;
  for (int i = 0; i < item->effectCount; i++) {
    int effect = PickEffectForItem(item, i);
    if (effect < 0) {
      UnloadItem(item);
      return false;
    }
    item->effects[i] = effect;
    if (strcmp(effects[effect].rarity, "UNIQUE") == 0) {
      item->effects[0] = effect;
      item->effectCount = 1;
      item->useLimit = item->usesLeft = 1;
      break;
    }
  }
  return true;
}

static int RaritySellValue(const char *rarity) {
  if (strcmp(rarity, "COMMUN") == 0)
    return 20;
  if (strcmp(rarity, "RARE") == 0)
    return 40;
  if (strcmp(rarity, "EPIQUE") == 0)
    return 75;
  if (strcmp(rarity, "LEGENDAIRE") == 0)
    return 150;
  if (strcmp(rarity, "MAUDIT") == 0)
    return 10;
  if (strcmp(rarity, "UNIQUE") == 0)
    return 600;
  return 0;
}

static const char *BossName(int floor) {
  switch (floor) {
  case 10:
    return "Béhémot";
  case 20:
    return "Séléna";
  case 30:
    return "Agarès";
  case 40:
    return "Dame Marika";
  case 50:
    return "Malénia";
  default:
    return "un Gardien";
  }
}

static const char *QuestItemName(const QuestItem *item) {
  if (item->type == QUEST_LETTER)
    return letterSealsUsed == 5 ? "Ma chérie..." : "Lettre scellée";
  if (item->type == QUEST_SEAL)
    return TextFormat("Sceau de %s", BossName(item->bossFloor));
  return "";
}

static const char *QuestItemDescription(const QuestItem *item) {
  if (item->type == QUEST_LETTER) {
    if (letterSealsUsed == 0)
      return "Une étrange lettre scellée, impossible à ouvrir pour le moment...";
    if (letterSealsUsed < 5)
      return "Une étrange lettre scellée, impossible à ouvrir pour le moment. "
             "Il semblerait que certains objets de la tour permettraient de "
             "l'ouvrir...";
    return "Une dernière lettre rédigée par une mère pour sa fille.";
  }
  if (item->type == QUEST_SEAL) {
    const char *bossName = BossName(item->bossFloor);
    static char description[256];
    snprintf(description, sizeof(description),
             "Un étrange sceau hérité de %s après l'avoir vaincu.",
             bossName);
    return description;
  }
  return "";
}

static const char *QuestItemPath(const QuestItem *item) {
  if (item->type == QUEST_LETTER)
    return letterSealsUsed == 5 ? "assets/open_letter.png" : "assets/letter.png";
  if (item->type == QUEST_SEAL)
    return "assets/seal.png";
  return "";
}

static bool LoadQuestTexture(QuestItem *item) {
  if (item->texture.id != 0)
    return true;
  const char *path = QuestItemPath(item);
  if (!path[0])
    return false;
  item->texture = LoadTexture(path);
  if (item->texture.id == 0)
    TraceLog(LOG_ERROR, "Image d'objet de quête introuvable: %s", path);
  return item->texture.id != 0;
}

static void DrawQuestItem(QuestItem *item, Rectangle slot) {
  if (item->type == QUEST_EMPTY || !LoadQuestTexture(item))
    return;
  float padding = slot.width * 0.12f;
  Rectangle target = {slot.x + padding, slot.y + padding,
                      slot.width - padding * 2, slot.height - padding * 2};
  Rectangle source = {0, 0, (float)item->texture.width,
                      (float)item->texture.height};
  DrawTexturePro(item->texture, source, target, (Vector2){0, 0}, 0, WHITE);
}

static void LoadData(void) {
  if (dataLoaded)
    return;
  dataLoaded = true;

  const char *effectPath = FileExists("assets/items_effect.txt")
                               ? "assets/items_effect.txt"
                               : "assets/items_effects.txt";
  FILE *file = fopen(effectPath, "r");
  if (file) {
    char line[512];
    while (fgets(line, sizeof(line), file) &&
           effectCount < EFFECT_LIMIT_COUNT) {
      line[strcspn(line, "\r\n")] = '\0';
      char *first = strchr(line, ';');
      if (!first)
        continue;
      *first++ = '\0';
      char *second = strchr(first, ';');
      if (!second)
        continue;
      *second++ = '\0';
      if (!line[0] || !first[0] || !second[0])
        continue;
      snprintf(effects[effectCount].text, sizeof(effects[effectCount].text),
               "%.*s", (int)sizeof(effects[effectCount].text) - 1, line);
      snprintf(effects[effectCount].rarity, sizeof(effects[effectCount].rarity),
               "%s", first);
      snprintf(effects[effectCount].game, sizeof(effects[effectCount].game),
               "%s", second);
      effectCount++;
    }
    fclose(file);
  } else {
    TraceLog(LOG_ERROR, "Fichier d'effets d'items introuvable");
  }

  FilePathList files = LoadDirectoryFilesEx("assets/items", ".png", false);
  for (unsigned int i = 0; i < files.count && itemNameCount < 128; i++) {
    const char *name = GetFileNameWithoutExt(files.paths[i]);
    snprintf(itemNames[itemNameCount++], ITEM_NAME_LENGTH, "%s", name);
  }
  UnloadDirectoryFiles(files);
  if (itemNameCount == 0)
    TraceLog(LOG_ERROR, "Aucun item dans assets/items");
}

static int ItemUniqueEffectIndex(const Item *item) {
  if (!item)
    return -1;
  for (int i = 0; i < item->effectCount; i++) {
    int effect = item->effects[i];
    if (effect >= 0 && effect < effectCount &&
        strcmp(effects[effect].rarity, "UNIQUE") == 0)
      return effect;
  }
  return -1;
}

static int UniqueBossLastFloor(int effect) {
  if (effect < 0 || effect >= effectCount ||
      strcmp(effects[effect].rarity, "UNIQUE") != 0)
    return 0;
  if (strcmp(effects[effect].game, "BOSS1") == 0)
    return 10;
  if (strcmp(effects[effect].game, "BOSS2") == 0)
    return 20;
  if (strcmp(effects[effect].game, "BOSS3") == 0)
    return 30;
  if (strcmp(effects[effect].game, "BOSS4") == 0)
    return 40;
  if (strcmp(effects[effect].game, "FINALBOSS") == 0)
    return 50;
  return 0;
}

static bool UniqueBossEffectIsObsolete(int effect) {
  int lastFloor = UniqueBossLastFloor(effect);
  return lastFloor > 0 && TowerBossFloorCompleted(lastFloor);
}

static int ItemObsoleteUniqueEffectIndex(const Item *item) {
  int effect = ItemUniqueEffectIndex(item);
  return UniqueBossEffectIsObsolete(effect) ? effect : -1;
}

static bool ItemCanEquipForRules(const Item *item, const char *game,
                                 bool bossFloor) {
  if (!item || !item->name[0] || item->effectCount < 1)
    return false;
  LoadData();
  bool hasUnique = false;
  for (int i = 0; i < item->effectCount; i++) {
    int effect = item->effects[i];
    if (effect < 0 || effect >= effectCount)
      return false;
    if (UniqueBossEffectIsObsolete(effect))
      return false;
    if (strcmp(effects[effect].rarity, "UNIQUE") != 0)
      continue;
    if (hasUnique || item->effectCount != 1)
      return false;
    hasUnique = true;
    if (game && game[0] && strcmp(effects[effect].game, "ALL") != 0 &&
        strcmp(effects[effect].game, game) != 0)
      return false;
  }
  return !bossFloor || hasUnique;
}

static void UnloadItem(Item *item) {
  if (item->texture.id != 0)
    UnloadTexture(item->texture);
  *item = (Item){0};
}

static void ResetUniqueConversion(void) {
  UnloadItem(&pendingUniqueItem);
  uniqueConversionState = UNIQUE_CONVERSION_CLOSED;
  uniqueConversionSlot = -1;
  uniqueConversionEffect = -1;
  uniqueConversionTimer = 0;
  uniqueConversionFailed = false;
}

static void OpenUniqueConversion(int slot) {
  if (!items || slot < EQUIPPED_COUNT || slot >= itemSlotCount ||
      !items[slot].name[0])
    return;
  int effect = ItemObsoleteUniqueEffectIndex(&items[slot]);
  if (effect < 0)
    return;
  ResetUniqueConversion();
  uniqueConversionState = UNIQUE_CONVERSION_NOTICE;
  uniqueConversionSlot = slot;
  uniqueConversionEffect = effect;
  contextSlot = -1;
}

static bool BeginUniqueConversion(void) {
  LoadData();
  int candidates[EFFECT_LIMIT_COUNT];
  int candidateCount = 0;
  for (int i = 0; i < effectCount; i++)
    if (strcmp(effects[i].rarity, "UNIQUE") == 0 &&
        !UniqueBossEffectIsObsolete(i))
      candidates[candidateCount++] = i;
  if (candidateCount == 0 || itemNameCount == 0) {
    TraceLog(LOG_ERROR,
             "Aucun nouvel effet UNIQUE valide pour la transmutation");
    return false;
  }

  pendingUniqueItem = (Item){0};
  snprintf(pendingUniqueItem.name, sizeof(pendingUniqueItem.name), "%s",
           itemNames[GetRandomValue(0, itemNameCount - 1)]);
  pendingUniqueItem.effects[0] =
      candidates[GetRandomValue(0, candidateCount - 1)];
  pendingUniqueItem.effectCount = 1;
  pendingUniqueItem.useLimit = pendingUniqueItem.usesLeft = 1;
  uniqueConversionEffect = pendingUniqueItem.effects[0];
  uniqueConversionTimer = 1.8f;
  uniqueConversionState = UNIQUE_CONVERSION_ANIMATION;
  return true;
}

static int PickEffectForItem(const Item *item, int selectedCount) {
  int rarityIndex =
      ItemsRarityForRoll(GetRandomValue(0, 9999),
                         TowerPlayerCharacter() == 1);
  const char *rarity = rarityNames[rarityIndex];

  int candidates[EFFECT_LIMIT_COUNT];
  int candidateCount = 0;
  for (int i = 0; rarity && i < effectCount; i++) {
    if (strcmp(effects[i].rarity, rarity) != 0 ||
        UniqueBossEffectIsObsolete(i))
      continue;
    bool alreadySelected = false;
    for (int j = 0; j < selectedCount; j++)
      if (item->effects[j] == i)
        alreadySelected = true;
    if (rarityIndex == DROP_UNIQUE) {
      for (int j = 0; j < selectedCount; j++)
        if (strcmp(effects[item->effects[j]].rarity, "UNIQUE") == 0)
          alreadySelected = true;
    }
    if (!alreadySelected)
      candidates[candidateCount++] = i;
  }

  if (candidateCount == 0) {
    TraceLog(LOG_WARNING, "Aucun effet disponible pour la rarete tiree");
    for (int i = 0; i < effectCount; i++) {
      if (UniqueBossEffectIsObsolete(i))
        continue;
      bool alreadySelected = false;
      for (int j = 0; j < selectedCount; j++)
        if (item->effects[j] == i)
          alreadySelected = true;
      if (!alreadySelected)
        candidates[candidateCount++] = i;
    }
  }
  if (candidateCount == 0) {
    TraceLog(LOG_ERROR, "Impossible de generer un effet d'item");
    return -1;
  }
  return candidates[GetRandomValue(0, candidateCount - 1)];
}

static bool LoadItemTexture(Item *item) {
  if (item->texture.id != 0)
    return true;
  char path[ITEM_NAME_LENGTH + 32];
  snprintf(path, sizeof(path), "assets/items/%s.png", item->name);
  item->texture = LoadTexture(path);
  return item->texture.id != 0;
}

Color ItemsRarityColor(const char *rarity) {
  if (!rarity)
    return RAYWHITE;
  if (strcmp(rarity, "COMMUN") == 0)
    return (Color){110, 210, 120, 255};
  if (strcmp(rarity, "RARE") == 0)
    return (Color){100, 175, 255, 255};
  if (strcmp(rarity, "EPIQUE") == 0)
    return (Color){205, 125, 255, 255};
  if (strcmp(rarity, "LEGENDAIRE") == 0)
    return (Color){255, 205, 70, 255};
  if (strcmp(rarity, "MAUDIT") == 0)
    return (Color){185, 105, 175, 255};
  if (strcmp(rarity, "UNIQUE") == 0)
    return (Color){255, 65, 75, 255};
  return RAYWHITE;
}

int ItemsRarityRank(const char *rarity) {
  if (!rarity)
    return 0;
  if (strcmp(rarity, "UNIQUE") == 0)
    return 6;
  if (strcmp(rarity, "MAUDIT") == 0)
    return 5;
  if (strcmp(rarity, "LEGENDAIRE") == 0)
    return 4;
  if (strcmp(rarity, "EPIQUE") == 0)
    return 3;
  if (strcmp(rarity, "RARE") == 0)
    return 2;
  if (strcmp(rarity, "COMMUN") == 0)
    return 1;
  return 0;
}

static int SortItemEffectsByRarity(const Item *item,
                                  int orderedEffects[EFFECT_LIMIT]) {
  int count = item->effectCount;
  if (count > EFFECT_LIMIT)
    count = EFFECT_LIMIT;
  for (int i = 0; i < count; i++) {
    int effect = item->effects[i];
    int rank = effect >= 0 && effect < effectCount
                   ? ItemsRarityRank(effects[effect].rarity)
                   : 0;
    int position = i;
    while (position > 0) {
      int previous = orderedEffects[position - 1];
      int previousRank =
          previous >= 0 && previous < effectCount
              ? ItemsRarityRank(effects[previous].rarity)
              : 0;
      if (previousRank >= rank)
        break;
      orderedEffects[position] = previous;
      position--;
    }
    orderedEffects[position] = effect;
  }
  return count;
}

static void DrawEffectText(const char *text, int x, int *y, int width,
                           int fontSize, Color color);
static int EffectLineCount(const char *text, int width, int fontSize);

static void FormatRarityPrefix(char *prefix, size_t prefixSize,
                               const char *rarity) {
  snprintf(prefix, prefixSize, "(%c)",
           rarity && rarity[0] ? rarity[0] : ' ');
}

static int EffectEntryLineCount(const char *text, const char *rarity,
                                int width, int fontSize) {
  char prefix[40];
  FormatRarityPrefix(prefix, sizeof(prefix), rarity);
  int bodyX = MeasureText(prefix, fontSize) + GameUiPixels(6);
  int bodyWidth = width - bodyX;
  if (bodyWidth < 1)
    bodyWidth = 1;
  return EffectLineCount(text, bodyWidth, fontSize);
}

static void DrawEffectEntry(const char *text, const char *rarity, int x,
                            int *y, int width, int fontSize) {
  char prefix[40];
  FormatRarityPrefix(prefix, sizeof(prefix), rarity);
  DrawText(prefix, x, *y, fontSize, ItemsRarityColor(rarity));
  int bodyX = x + MeasureText(prefix, fontSize) + GameUiPixels(6);
  int bodyWidth = width - (bodyX - x);
  if (bodyWidth < 1)
    bodyWidth = 1;
  DrawEffectText(text, bodyX, y, bodyWidth, fontSize, RAYWHITE);
}

static const char *EffectGameLabel(const char *game) {
  if (strcmp(game, "ALL") == 0)
    return "TOUS LES MINI-JEUX";
  if (strcmp(game, "BULLET_HELL") == 0)
    return "BULLET HELL";
  if (strcmp(game, "RING") == 0)
    return "RINGS";
  if (strcmp(game, "MIRROR") == 0)
    return "MIRROR";
  if (strcmp(game, "SEALS") == 0)
    return "SEALS";
  if (strcmp(game, "BOSS1") == 0)
    return "BEHEMOT";
  if (strcmp(game, "BOSS2") == 0)
    return "SELENA";
  if (strcmp(game, "SPIKES") == 0)
    return "SPIKES";
  if (strcmp(game, "FINALBOSS") == 0)
    return "BOSS FINAL";
  if (strcmp(game, "BOSS4") == 0)
    return "DAME MARIKA";
  if (strcmp(game, "BOSS3") == 0)
    return "AGARES";
  return game;
}

static void FormatEffectText(const ItemEffect *effect, char *text,
                             size_t textSize) {
  snprintf(text, textSize, "%s: %s", EffectGameLabel(effect->game),
           effect->text);
}

static void DrawEffectText(const char *text, int x, int *y, int width,
                           int fontSize, Color color) {
  char line[384] = "";
  const char *word = text;
  while (*word) {
    const char *end = strchr(word, ' ');
    size_t wordLength = end ? (size_t)(end - word) : strlen(word);
    char candidate[384];
    snprintf(candidate, sizeof(candidate), "%s%.*s",
             line[0] ? " " : "", (int)wordLength, word);
    if (line[0] && MeasureText(TextFormat("%s%s", line, candidate), fontSize) > width) {
      DrawText(line, x, *y, fontSize, color);
      *y += fontSize + GameUiPixels(3);
      line[0] = '\0';
    }
    strncat(line, candidate, sizeof(line) - strlen(line) - 1);
    if (!end)
      break;
    word = end + 1;
  }
  if (line[0]) {
    DrawText(line, x, *y, fontSize, color);
    *y += fontSize + GameUiPixels(3);
  }
}

static int EffectLineCount(const char *text, int width, int fontSize) {
  char line[384] = "";
  int count = 0;
  const char *word = text;
  while (*word) {
    const char *end = strchr(word, ' ');
    size_t wordLength = end ? (size_t)(end - word) : strlen(word);
    char candidate[384];
    snprintf(candidate, sizeof(candidate), "%s%.*s",
             line[0] ? " " : "", (int)wordLength, word);
    if (line[0] && MeasureText(TextFormat("%s%s", line, candidate), fontSize) > width) {
      count++;
      line[0] = '\0';
    }
    strncat(line, candidate, sizeof(line) - strlen(line) - 1);
    if (!end)
      break;
    word = end + 1;
  }
  return count + (line[0] ? 1 : 0);
}

static void DrawItemTooltip(int slot, Item *item, Vector2 position) {
  float scale = GameUiScale();
  int orderedEffects[EFFECT_LIMIT];
  int orderedCount = SortItemEffectsByRarity(item, orderedEffects);
  int margin = GameUiPixels(6);
  int padding = GameUiPixels(10);
  int width = GameUiPixels(390);
  if (width > GetScreenWidth() - margin * 2)
    width = GetScreenWidth() - margin * 2;
  int fontSize = GameUiPixels(17);
  int height;
  do {
    height = GameUiPixels(46);
      height += GameUiPixels(24);
    for (int i = 0; i < orderedCount; i++) {
      int effectIndex = orderedEffects[i];
      char effectText[320];
      FormatEffectText(&effects[effectIndex], effectText, sizeof(effectText));
      height += EffectEntryLineCount(effectText, effects[effectIndex].rarity,
                                     width - padding * 2, fontSize) *
                (fontSize + GameUiPixels(3));
    }
    if (height <= GetScreenHeight() - margin * 2 ||
        fontSize <= GameUiPixels(10))
      break;
    fontSize--;
  } while (true);
  if (height > GetScreenHeight() - margin * 2)
    height = GetScreenHeight() - margin * 2;

  Rectangle box = {position.x + GameUiPixels(14),
                   position.y + GameUiPixels(14), width, height};
  if (box.x + box.width > GetScreenWidth() - margin)
    box.x = position.x - box.width - GameUiPixels(14);
  if (box.y + box.height > GetScreenHeight() - margin)
    box.y = GetScreenHeight() - box.height - margin;
  if (box.x < margin)
    box.x = margin;
  if (box.y < margin)
    box.y = margin;

  DrawRectangleRec(box, (Color){15, 15, 20, 245});
  DrawRectangleLinesEx(box, 2 * scale, (Color){225, 185, 40, 255});
  DrawText(item->name, (int)box.x + padding, (int)box.y + GameUiPixels(8),
           GameUiPixels(19),
           ItemsRarityColor(ItemsItemRarestEffectRarity(slot)));
  const char *usesText = ItemUniqueEffectIndex(item) >= 0
                             ? "UNIQUE : détruit après une victoire"
                             : TextFormat("Mini-jeux réussis restants : %d / %d",
                                          item->usesLeft, item->useLimit);
  DrawText(usesText,
           (int)box.x + padding, (int)box.y + GameUiPixels(30),
           GameUiPixels(14), (Color){180, 210, 245, 255});
  int y = (int)box.y + GameUiPixels(50);
  int contentHeight = (int)(box.y + box.height) - y - margin;
  if (contentHeight < 0)
    contentHeight = 0;
  BeginScissorMode((int)box.x, y, width, contentHeight);
  for (int i = 0; i < orderedCount; i++) {
    ItemEffect *effect = &effects[orderedEffects[i]];
    char effectText[320];
    FormatEffectText(effect, effectText, sizeof(effectText));
    DrawEffectEntry(effectText, effect->rarity, (int)box.x + padding, &y,
                    width - padding * 2, fontSize);
  }
  EndScissorMode();
}

static void DrawItem(Item *item, Rectangle slot) {
  if (!item->name[0] || !LoadItemTexture(item))
    return;
  float padding = slot.width * 0.12f;
  Rectangle target = {slot.x + padding, slot.y + padding,
                      slot.width - padding * 2, slot.height - padding * 2};
  Rectangle source = {0, 0, (float)item->texture.width,
                      (float)item->texture.height};
  DrawTexturePro(item->texture, source, target, (Vector2){0, 0}, 0, WHITE);
}

static void DrawInventoryItem(int slot, Rectangle area) {
  if (slot == equipWarningSlot && equipWarningTimer > 0) {
    float shake = sinf((float)GetTime() * 42 + slot) * GameUiPixels(2);
    area.x += shake;
    area.y += cosf((float)GetTime() * 37 + slot) * GameUiPixels(1);
  }
  DrawItem(&items[slot], area);
  if (slot == equipWarningSlot && equipWarningTimer > 0)
    DrawRectangleRoundedLinesEx(area, 0.06f, 4, GameUiPixels(3),
                                (Color){255, 55, 65, 255});
}

static void SetStorageScrollFromThumb(float thumbTop, Rectangle track,
                                      float thumbHeight, int rowsPastView) {
  float travel = track.height - thumbHeight;
  if (travel <= 0 || rowsPastView <= 0) {
    storageScrollOffset = 0;
    return;
  }
  float progress = (thumbTop - track.y) / travel;
  if (progress < 0)
    progress = 0;
  if (progress > 1)
    progress = 1;
  storageScrollOffset =
      (int)lroundf(progress * rowsPastView) * STORAGE_COLUMNS;
}

static void DrawStorageScrollButton(Rectangle button, bool up, bool enabled,
                                    bool hovered) {
  Color accent = enabled ? (Color){225, 185, 40, 255}
                         : (Color){85, 78, 55, 255};
  DrawRectangleRec(button, hovered && enabled ? (Color){70, 60, 35, 255}
                                               : (Color){20, 21, 20, 255});
  DrawRectangleLinesEx(button, GameUiPixels(1.5f), accent);
  Vector2 center = {button.x + button.width / 2,
                    button.y + button.height / 2};
  float halfWidth = button.width * 0.24f;
  float halfHeight = button.height * 0.2f;
  Vector2 tip = {center.x, center.y + (up ? -halfHeight : halfHeight)};
  Vector2 left = {center.x - halfWidth,
                  center.y + (up ? halfHeight : -halfHeight)};
  Vector2 right = {center.x + halfWidth,
                   center.y + (up ? halfHeight : -halfHeight)};
  DrawTriangle(tip, left, right, accent);
}

static void DrawStorageScrollbar(Rectangle *stored, int rowsPastView,
                                 bool acceptInput) {
  if (rowsPastView <= 0) {
    storageScrollDragging = false;
    return;
  }

  Rectangle first = stored[0];
  Rectangle last = stored[STORAGE_VISIBLE_COUNT - 1];
  float buttonSize = GameUiPixels(20);
  float width = GameUiPixels(18);
  float x = last.x + last.width + GameUiPixels(6);
  Rectangle upButton = {x, first.y, width, buttonSize};
  Rectangle downButton = {x, last.y + last.height - buttonSize, width,
                          buttonSize};
  Rectangle track = {x, upButton.y + buttonSize, width,
                     downButton.y - (upButton.y + buttonSize)};
  int contentRows = STORAGE_VISIBLE_COUNT / STORAGE_COLUMNS + rowsPastView;
  float thumbHeight = track.height *
                      (float)(STORAGE_VISIBLE_COUNT / STORAGE_COLUMNS) /
                      contentRows;
  float minThumbHeight = GameUiPixels(24);
  if (thumbHeight < minThumbHeight)
    thumbHeight = minThumbHeight;
  if (thumbHeight > track.height)
    thumbHeight = track.height;
  float travel = track.height - thumbHeight;
  int maxScrollRows = rowsPastView;
  float progress = maxScrollRows > 0
                       ? (float)(storageScrollOffset / STORAGE_COLUMNS) /
                             maxScrollRows
                       : 0;
  if (progress < 0)
    progress = 0;
  if (progress > 1)
    progress = 1;
  Rectangle thumb = {x, track.y + travel * progress, width, thumbHeight};
  Vector2 mouse = GetMousePosition();

  if (!acceptInput || !IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    storageScrollDragging = false;
  if (acceptInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (CheckCollisionPointRec(mouse, upButton)) {
      storageScrollOffset -= STORAGE_COLUMNS;
      if (storageScrollOffset < 0)
        storageScrollOffset = 0;
    } else if (CheckCollisionPointRec(mouse, downButton)) {
      storageScrollOffset += STORAGE_COLUMNS;
      int maxOffset = rowsPastView * STORAGE_COLUMNS;
      if (storageScrollOffset > maxOffset)
        storageScrollOffset = maxOffset;
    } else if (CheckCollisionPointRec(mouse, track)) {
      if (CheckCollisionPointRec(mouse, thumb)) {
        storageScrollDragging = true;
        storageScrollGrab = mouse.y - thumb.y;
      } else {
        SetStorageScrollFromThumb(mouse.y - thumbHeight / 2, track,
                                  thumbHeight, rowsPastView);
        thumb.y = track.y + travel *
                                ((float)(storageScrollOffset / STORAGE_COLUMNS) /
                                 rowsPastView);
        storageScrollDragging = true;
        storageScrollGrab = thumbHeight / 2;
      }
    }
  }
  if (acceptInput && storageScrollDragging &&
      IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    SetStorageScrollFromThumb(mouse.y - storageScrollGrab, track, thumbHeight,
                              rowsPastView);
  }

  progress = (float)(storageScrollOffset / STORAGE_COLUMNS) / maxScrollRows;
  thumb.y = track.y + travel * progress;
  bool upHovered = CheckCollisionPointRec(mouse, upButton);
  bool downHovered = CheckCollisionPointRec(mouse, downButton);
  DrawStorageScrollButton(upButton, true, storageScrollOffset > 0, upHovered);
  DrawRectangleRec(track, (Color){18, 19, 18, 255});
  DrawRectangleLinesEx(track, GameUiPixels(1), (Color){110, 90, 25, 255});
  DrawRectangleRec((Rectangle){thumb.x + GameUiPixels(2),
                               thumb.y + GameUiPixels(2),
                               thumb.width - GameUiPixels(4),
                               thumb.height - GameUiPixels(4)},
                   storageScrollDragging ||
                           CheckCollisionPointRec(mouse, thumb)
                       ? (Color){255, 225, 110, 255}
                       : (Color){205, 166, 45, 255});
  DrawStorageScrollButton(downButton, false,
                          storageScrollOffset < maxScrollRows * STORAGE_COLUMNS,
                          downHovered);
}

static int FindEmpty(int first, int end) {
  if (!items)
    return -1;
  if (first < 0)
    first = 0;
  if (end > itemSlotCount)
    end = itemSlotCount;
  for (int i = first; i < end; i++)
    if (!items[i].name[0])
      return i;
  return -1;
}

static void QueueItemReward(int slot) {
  if (slot < 0 || slot >= itemSlotCount || !items[slot].name[0])
    return;
  if (rewardQueueCount < 5)
    rewardQueue[rewardQueueCount++] = slot;
  if (rewardQueueCount == 1 || rewardTimer <= 0) {
    rewardQueueIndex = 0;
    rewardSlot = rewardQueue[0];
    questRewardSlot = -1;
    rewardTimer = 3;
  }
}

static void RemoveQueuedRewardSlot(int slot) {
  bool activeRewardRemoved = rewardSlot == slot;
  for (int i = 0; i < rewardQueueCount;) {
    if (rewardQueue[i] != slot) {
      i++;
      continue;
    }
    if (i < rewardQueueIndex)
      rewardQueueIndex--;
    memmove(&rewardQueue[i], &rewardQueue[i + 1],
            (size_t)(rewardQueueCount - i - 1) * sizeof(rewardQueue[0]));
    rewardQueueCount--;
  }
  if (!activeRewardRemoved)
    return;
  if (questRewardSlot < 0 && rewardQueueCount > 0) {
    if (rewardQueueIndex >= rewardQueueCount)
      rewardQueueIndex = 0;
    rewardSlot = rewardQueue[rewardQueueIndex];
    rewardTimer = 3;
  } else {
    rewardSlot = -1;
    if (questRewardSlot < 0)
      rewardTimer = 0;
  }
}

void ItemsReset(void) {
  ResetUniqueConversion();
  for (int i = 0; i < itemCapacity; i++)
    UnloadItem(&items[i]);
  if (items)
    memset(items, 0, (size_t)itemCapacity * sizeof(*items));
  itemSlotCount = EQUIPPED_COUNT;
  highestStoredSlot = -1;
  rulesFloor = -1;
  rulesBossFloor = false;
  rulesGame[0] = '\0';
  storageScrollOffset = 0;
  storageWheelRemainder = 0;
  storageScrollDragging = false;
  storageScrollGrab = 0;
  equipWarningSlot = -1;
  equipWarningTimer = 0;
  if (!ReserveItemCapacity(EQUIPPED_COUNT))
    itemSlotCount = 0;
  for (int i = 0; i < knownItemCount; i++) {
    if (knownPreviewTextures[i].id != 0)
      UnloadTexture(knownPreviewTextures[i]);
    knownPreviewTextures[i] = (Texture2D){0};
  }
  for (int i = 0; i < QUEST_ITEM_COUNT; i++) {
    if (questItems[i].texture.id != 0)
      UnloadTexture(questItems[i].texture);
    questItems[i] = (QuestItem){0};
  }
  if (letterText)
    UnloadFileText(letterText);
  letterText = NULL;
  rewardSlot = -1;
  questRewardSlot = -1;
  rewardTimer = 0;
  rewardQueueCount = rewardQueueIndex = 0;
  contextSlot = -1;
  questContextSlot = -1;
  bossSealMask = 0;
  letterSealsUsed = 0;
  letterAwarded = letterRead = letterReaderActive = letterReachedEnd = false;
  letterScroll = letterMaxScroll = 0;
}

void ItemsUnload(void) {
  ItemsReset();
  free(items);
  items = NULL;
  itemCapacity = 0;
  itemSlotCount = EQUIPPED_COUNT;
  if (letterFontReady)
    UnloadFont(letterFont);
  letterFont = (Font){0};
  letterFontReady = false;
  dataLoaded = false;
  effectCount = itemNameCount = 0;
}

void ItemsLoad(const char *data) {
  ItemsReset();
  LoadData();
  LoadCollection();
  if (!data || !data[0])
    return;

  size_t dataLength = strlen(data);
  if (dataLength == SIZE_MAX) {
    TraceLog(LOG_ERROR, "Donnees d'inventaire trop volumineuses");
    return;
  }
  char *copy = malloc(dataLength + 1);
  if (!copy) {
    TraceLog(LOG_ERROR, "Impossible de charger les donnees d'inventaire");
    return;
  }
  memcpy(copy, data, dataLength + 1);
  char *record = copy;
  while (record) {
    char *nextRecord = strchr(record, ';');
    if (nextRecord)
      *nextRecord++ = '\0';
    if (strncmp(record, "QM:", 3) == 0) {
      int awarded = 0, used = 0, read = 0;
      unsigned int mask = 0;
      if (sscanf(record + 3, "%d:%u:%d:%d", &awarded, &mask, &used, &read) == 4) {
        letterAwarded = awarded != 0;
        bossSealMask = mask & 31u;
        letterSealsUsed = used < 0 ? 0 : used > 5 ? 5 : used;
        letterRead = read != 0 && letterSealsUsed == 5;
      }
      record = nextRecord;
      continue;
    }
    if (record[0] == 'Q' && record[1] >= '0' && record[1] <= '9') {
      char *first = strchr(record, ':');
      char *second = first ? strchr(first + 1, ':') : NULL;
      char *third = second ? strchr(second + 1, ':') : NULL;
      char *fourth = third ? strchr(third + 1, ':') : NULL;
      if (first && second) {
        *first++ = '\0';
        *second++ = '\0';
        if (third)
          *third++ = '\0';
        if (fourth)
          *fourth++ = '\0';
        int slot = atoi(record + 1);
        int type = atoi(first);
        int floor = atoi(second);
        if (slot >= 0 && slot < QUEST_ITEM_COUNT &&
            ((type == QUEST_LETTER && slot == 0 && floor == 0 &&
              FileExists("assets/letter.png") &&
              FileExists("assets/open_letter.png")) ||
             (type == QUEST_SEAL && slot > 0 && floor >= 10 && floor <= 50 &&
              floor % 10 == 0 && FileExists("assets/seal.png"))))
          questItems[slot] = (QuestItem){(QuestItemType)type, floor, {0}};
      }
      record = nextRecord;
      continue;
    }
    char *first = strchr(record, ':');
    char *second = first ? strchr(first + 1, ':') : NULL;
    if (first && second) {
      *first++ = '\0';
      *second++ = '\0';
      char *useLeft = NULL;
      char *useLimit = NULL;
      char *effectEnd = second;
      char *firstUseSeparator = strchr(effectEnd, ':');
      if (firstUseSeparator) {
        *firstUseSeparator++ = '\0';
        useLeft = firstUseSeparator;
        char *secondUseSeparator = strchr(useLeft, ':');
        if (secondUseSeparator) {
          *secondUseSeparator++ = '\0';
          useLimit = secondUseSeparator;
        }
      }
      char *slotEnd = NULL;
      long savedSlot = strtol(record, &slotEnd, 10);
      if (slotEnd != record && slotEnd && !*slotEnd && savedSlot >= 0 &&
          savedSlot < INT_MAX && first[0] &&
          strlen(first) < ITEM_NAME_LENGTH && !strchr(first, '/') &&
          !strchr(first, '\\') && effectCount > 0) {
        int slot = savedSlot < EQUIPPED_COUNT
                       ? (int)savedSlot
                       : FindEmptyStorage();
        if (slot < 0 || !EnsureItemSlots(slot + 1)) {
          record = nextRecord;
          continue;
        }
        Item *item = &items[slot];
        snprintf(item->name, sizeof(item->name), "%s", first);
        int savedLimit = useLimit ? atoi(useLimit) : 0;
        int savedUses = useLeft ? atoi(useLeft) : 0;
        item->useLimit =
            savedLimit >= 4 && savedLimit <= 8 ? savedLimit
                                                : GetRandomValue(4, 8);
        item->usesLeft =
            savedUses >= 1 && savedUses <= item->useLimit ? savedUses
                                                          : item->useLimit;
        if (item->useLimit < 1)
          item->useLimit = 4;
        if (item->usesLeft < 1)
          item->usesLeft = item->useLimit;
        char path[ITEM_NAME_LENGTH + 32];
        snprintf(path, sizeof(path), "assets/items/%s.png", item->name);
        if (FileExists(path)) {
          char *effect = second;
          while (effect && *effect && item->effectCount < EFFECT_LIMIT) {
            char *nextEffect = strchr(effect, ',');
            if (nextEffect)
              *nextEffect++ = '\0';
            int index = atoi(effect);
            if (index >= 0 && index < effectCount)
              item->effects[item->effectCount++] = index;
            effect = nextEffect;
          }
          if (item->effectCount == 0)
            UnloadItem(item);
          else {
            int uniqueEffect = ItemUniqueEffectIndex(item);
            if (uniqueEffect >= 0) {
              item->effects[0] = uniqueEffect;
              item->effectCount = 1;
              item->useLimit = item->usesLeft = 1;
            }
            RememberItem(item);
            if (slot >= EQUIPPED_COUNT && slot > highestStoredSlot)
              highestStoredSlot = slot;
          }
        } else {
          UnloadItem(item);
        }
      }
    }
    record = nextRecord;
  }
  if (questItems[0].type == QUEST_LETTER)
    letterAwarded = true;
  else if (letterAwarded)
    questItems[0].type = QUEST_LETTER;
  for (int i = 1; i < QUEST_ITEM_COUNT; i++)
    if (questItems[i].type == QUEST_SEAL)
      bossSealMask |= 1u << (questItems[i].bossFloor / 10 - 1);
  free(copy);
}

char *ItemsSave(void) {
  if (!EnsureItemSlots(EQUIPPED_COUNT))
    return NULL;
  int highestSlot = ItemsHighestOccupiedSlot();
  int slotLimit = highestSlot >= 0 ? highestSlot + 1 : EQUIPPED_COUNT;
  size_t size = 64;
  for (int i = 0; i < slotLimit; i++) {
    if (!items[i].name[0])
      continue;
    size_t itemSize = strlen(items[i].name) + 64 +
                      (size_t)items[i].effectCount * 12;
    if (itemSize > SIZE_MAX - size) {
      TraceLog(LOG_ERROR, "Sauvegarde des objets trop volumineuse");
      return NULL;
    }
    size += itemSize;
  }
  for (int i = 0; i < QUEST_ITEM_COUNT; i++)
    if (questItems[i].type != QUEST_EMPTY) {
      if (32 > SIZE_MAX - size) {
        TraceLog(LOG_ERROR, "Sauvegarde des objets trop volumineuse");
        return NULL;
      }
      size += 32;
    }
  char *data = calloc(size, 1);
  if (!data) {
    TraceLog(LOG_ERROR, "Impossible d'allouer la sauvegarde des objets");
    return NULL;
  }
  size_t used = 0;
  for (int i = 0; i < slotLimit; i++) {
    if (!items[i].name[0])
      continue;
    int written = snprintf(data + used, size - used, "%s%d:%s:",
                           used ? ";" : "", i, items[i].name);
    if (written < 0 || (size_t)written >= size - used)
      goto save_failed;
    used += (size_t)written;
    for (int j = 0; j < items[i].effectCount; j++) {
      written = snprintf(data + used, size - used, "%s%d",
                         j ? "," : "", items[i].effects[j]);
      if (written < 0 || (size_t)written >= size - used)
        goto save_failed;
      used += (size_t)written;
    }
    written = snprintf(data + used, size - used, ":%d:%d",
                       items[i].usesLeft, items[i].useLimit);
    if (written < 0 || (size_t)written >= size - used)
      goto save_failed;
    used += (size_t)written;
  }
  for (int i = 0; i < QUEST_ITEM_COUNT; i++) {
    if (questItems[i].type == QUEST_EMPTY)
      continue;
    int written = snprintf(data + used, size - used, "%sQ%d:%d:%d",
                           used ? ";" : "", i, questItems[i].type,
                           questItems[i].bossFloor);
    if (written < 0 || (size_t)written >= size - used)
      goto save_failed;
    used += (size_t)written;
  }
  int written = snprintf(data + used, size - used, "%sQM:%d:%u:%d:%d",
                         used ? ";" : "", letterAwarded, bossSealMask,
                         letterSealsUsed, letterRead);
  if (written < 0 || (size_t)written >= size - used)
    goto save_failed;
  return data;

save_failed:
  free(data);
  TraceLog(LOG_ERROR, "Sauvegarde des objets trop volumineuse");
  return NULL;
}

void ItemsGrantQuestLetter(void) {
  LoadData();
  if (letterAwarded || questItems[0].type == QUEST_LETTER)
    return;
  questItems[0] = (QuestItem){QUEST_LETTER, 0, {0}};
  letterAwarded = true;
  questRewardSlot = 0;
  rewardSlot = -1;
  rewardTimer = 3;
}

void ItemsGrantBossSeal(int floor) {
  int bossIndex = floor / 10 - 1;
  if (floor < 10 || floor > 50 || floor % 10 != 0)
    return;
  unsigned int bit = 1u << bossIndex;
  if (bossSealMask & bit)
    return;
  int slot = -1;
  for (int i = 1; i < QUEST_ITEM_COUNT; i++)
    if (questItems[i].type == QUEST_EMPTY) {
      slot = i;
      break;
    }
  if (slot < 0) {
    TraceLog(LOG_ERROR, "Aucun emplacement libre pour le sceau du boss");
    return;
  }
  questItems[slot] = (QuestItem){QUEST_SEAL, floor, {0}};
  bossSealMask |= bit;
  questRewardSlot = slot;
  rewardSlot = -1;
  rewardTimer = 3;
}

void ItemsMigrateQuestRewards(int nextFloor) {
  if (nextFloor > 5)
    ItemsGrantQuestLetter();
  for (int floor = 10; floor <= 50 && floor < nextFloor; floor += 10)
    ItemsGrantBossSeal(floor);
}

bool ItemsMelinaLetterRead(void) {
  return letterAwarded && letterSealsUsed == 5 && letterRead;
}

void ItemsGrantForLevelUp(void) {
  LoadData();
  LoadCollection();
  int slot = FindEmptyStorage();
  if (slot < 0) {
    TraceLog(LOG_WARNING, "Impossible d'attribuer l'item de niveau");
    rewardSlot = -1;
    rewardTimer = 0;
    return;
  }
  Item *item = &items[slot];
  if (!CreateRandomItem(item)) {
    rewardSlot = -1;
    rewardTimer = 0;
    return;
  }
  if (slot > highestStoredSlot)
    highestStoredSlot = slot;
  RememberItem(item);
  for (int i = 0; i < item->effectCount; i++)
    if (strcmp(effects[item->effects[i]].rarity, "LEGENDAIRE") == 0 ||
        strcmp(effects[item->effects[i]].rarity, "UNIQUE") == 0) {
      AchievementsUnlock(ACHIEVEMENT_LEGENDARY_ITEM);
      AchievementsUnlockTitle(TITLE_FORTUNE);
      break;
    }
  QueueItemReward(slot);
}

void ItemsPrepareForFloor(int floor, const char *game, bool bossFloor) {
  if (!EnsureItemSlots(EQUIPPED_COUNT))
    return;
  LoadData();

  bool enteringBossFloor =
      bossFloor && (rulesFloor != floor || !rulesBossFloor);
  bool contextChanged = rulesFloor != floor || rulesBossFloor != bossFloor ||
                        strcmp(rulesGame, game ? game : "") != 0;
  if (enteringBossFloor) {
    if (!StoreEquippedItems())
      TraceLog(LOG_ERROR,
               "Impossible de ranger les objets equipes avant le boss");
  } else if (contextChanged) {
    for (int i = 0; i < EQUIPPED_COUNT; i++) {
      if (!items[i].name[0] ||
          ItemCanEquipForRules(&items[i], game, bossFloor))
        continue;
      int destination = FindEmptyStorage();
      if (destination < 0) {
        TraceLog(LOG_ERROR, "Impossible de ranger un objet non compatible");
        break;
      }
      MoveItemToSlot(i, destination);
    }
  }

  rulesFloor = floor;
  rulesBossFloor = bossFloor;
  snprintf(rulesGame, sizeof(rulesGame), "%s", game ? game : "");
}

void ItemsApplyForGame(const char *game, bool bossFloor) {
  if (!game || !EnsureItemSlots(EQUIPPED_COUNT))
    return;
  LoadData();
  FunctionalReset();
  bool applied[EFFECT_LIMIT_COUNT] = {false};
  for (int i = 0; i < EQUIPPED_COUNT; i++) {
    if (!items[i].name[0] ||
        !ItemCanEquipForRules(&items[i], game, bossFloor))
      continue;
    for (int j = 0; j < items[i].effectCount; j++) {
      int effectIndex = items[i].effects[j];
      ItemEffect *effect = &effects[effectIndex];
      bool appliesToGame = strcmp(effect->game, "ALL") == 0 ||
                           strcmp(effect->game, game) == 0;
      bool nonUniqueFinalBossEffect =
          strcmp(game, "FINALBOSS") == 0 &&
          strcmp(effect->game, "FINALBOSS") == 0 &&
          strcmp(effect->rarity, "UNIQUE") != 0;
      if (!applied[effectIndex] && appliesToGame &&
          !nonUniqueFinalBossEffect) {
        FunctionalAddEffect(effectIndex);
        applied[effectIndex] = true;
      }
    }

  }
}

void ItemsDestroy(int slot) {
  if (!items || slot < 0 || slot >= itemSlotCount || !items[slot].name[0])
    return;
  RemoveQueuedRewardSlot(slot);
  UnloadItem(&items[slot]);
  if (contextSlot == slot)
    contextSlot = -1;
  if (equipWarningSlot == slot) {
    equipWarningSlot = -1;
    equipWarningTimer = 0;
  }
  if (slot >= EQUIPPED_COUNT)
    CompactStoredItems();
}

void ItemsConsumeEquippedUses(void) {
  if (!EnsureItemSlots(EQUIPPED_COUNT))
    return;
  for (int i = 0; i < EQUIPPED_COUNT; i++) {
    if (!items[i].name[0])
      continue;
    if (ItemUniqueEffectIndex(&items[i]) >= 0) {
      ItemsDestroy(i);
      continue;
    }
    if (items[i].useLimit > 0)
      items[i].usesLeft--;
    if (items[i].usesLeft <= 0)
      ItemsDestroy(i);
  }
}

int ItemsSellValue(int slot) {
  if (!items || slot < 0 || slot >= itemSlotCount || !items[slot].name[0])
    return 0;
  LoadData();
  int value = 0;
  for (int i = 0; i < items[slot].effectCount; i++) {
    int effect = items[slot].effects[i];
    if (effect >= 0 && effect < effectCount)
      value += RaritySellValue(effects[effect].rarity);
  }
  return value * (100 + 20 * (items[slot].effectCount - 1)) / 100;
}

int ItemsSell(int slot) {
  int value = ItemsSellValue(slot);
  if (value <= 0)
    return 0;
  ItemsDestroy(slot);
  TowerAddEtris(value);
  return value;
}

bool ItemsCanStore(int count) {
  return count >= 1 && count <= 5;
}

bool ItemsGrantShopPack(int count, int slots[5]) {
  if (!slots || !ItemsCanStore(count))
    return false;
  LoadData();
  LoadCollection();
  int lastSlot = highestStoredSlot < EQUIPPED_COUNT
                     ? EQUIPPED_COUNT - 1
                     : highestStoredSlot;
  if (lastSlot > INT_MAX - count - 1 ||
      !ReserveItemCapacity(lastSlot + count + 1))
    return false;
  int created = 0;
  while (created < count) {
    int slot = FindEmptyStorage();
    if (slot < 0 || !CreateRandomItem(&items[slot])) {
      if (slot >= 0)
        UnloadItem(&items[slot]);
      for (int j = 0; j < created; j++)
        UnloadItem(&items[slots[j]]);
      CompactStoredItems();
      return false;
    }
    slots[created++] = slot;
    if (slot > highestStoredSlot)
      highestStoredSlot = slot;
  }
  for (int i = 0; i < count; i++)
    RememberItem(&items[slots[i]]);
  for (int i = 0; i < count; i++)
    QueueItemReward(slots[i]);
  return true;
}

int ItemsKnownItemCount(void) {
  LoadData();
  LoadCollection();
  return knownItemCount;
}

const char *ItemsKnownItem(int index) {
  LoadCollection();
  return index >= 0 && index < knownItemCount ? knownItems[index] : "";
}

int ItemsKnownEffectCount(void) {
  LoadData();
  LoadCollection();
  int count = 0;
  for (int i = 0; i < effectCount; i++)
    count += knownEffects[i];
  return count;
}

int ItemsKnownEffect(int index) {
  LoadData();
  LoadCollection();
  for (int i = 0; i < effectCount; i++)
    if (knownEffects[i] && index-- == 0)
      return i;
  return -1;
}

int ItemsHighestOccupiedSlot(void) {
  int highest = highestStoredSlot;
  for (int i = EQUIPPED_COUNT - 1; i >= 0; i--)
    if (items && items[i].name[0]) {
      if (i > highest)
        highest = i;
      break;
    }
  return highest;
}

typedef struct {
  Item item;
  int originalSlot;
  int rarestEffectRank;
} StoredItemSortEntry;

static int CompareStoredItemsByRarity(const void *left, const void *right) {
  const StoredItemSortEntry *a = left;
  const StoredItemSortEntry *b = right;
  if (a->rarestEffectRank != b->rarestEffectRank)
    return a->rarestEffectRank > b->rarestEffectRank ? -1 : 1;
  return (a->originalSlot > b->originalSlot) -
         (a->originalSlot < b->originalSlot);
}

static int CompareStoredItemsByName(const void *left, const void *right) {
  const StoredItemSortEntry *a = left;
  const StoredItemSortEntry *b = right;
  int nameComparison = strcmp(a->item.name, b->item.name);
  if (nameComparison != 0)
    return nameComparison;
  return (a->originalSlot > b->originalSlot) -
         (a->originalSlot < b->originalSlot);
}

static int CompareStoredItemsByEffectCount(const void *left,
                                           const void *right) {
  const StoredItemSortEntry *a = left;
  const StoredItemSortEntry *b = right;
  if (a->item.effectCount != b->item.effectCount)
    return a->item.effectCount > b->item.effectCount ? -1 : 1;
  return (a->originalSlot > b->originalSlot) -
         (a->originalSlot < b->originalSlot);
}

static int RemapSortedStorageSlot(
    int slot, int firstSlot, const StoredItemSortEntry *ordered, int count) {
  if (slot < firstSlot || slot >= firstSlot + count)
    return slot;
  for (int i = 0; i < count; i++)
    if (ordered[i].originalSlot == slot)
      return firstSlot + i;
  return slot;
}

bool ItemsSortStoredItems(ItemSortOrder order) {
  if (uniqueConversionState != UNIQUE_CONVERSION_CLOSED ||
      order < ITEM_SORT_RARITY || order >= ITEM_SORT_COUNT)
    return false;
  LoadData();
  CompactStoredItems();
  if (highestStoredSlot < EQUIPPED_COUNT)
    return true;

  int firstSlot = EQUIPPED_COUNT;
  int count = highestStoredSlot - firstSlot + 1;
  if ((size_t)count > SIZE_MAX / sizeof(StoredItemSortEntry)) {
    TraceLog(LOG_ERROR, "Tri des objets trop volumineux");
    return false;
  }
  StoredItemSortEntry *ordered =
      malloc((size_t)count * sizeof(StoredItemSortEntry));
  if (!ordered) {
    TraceLog(LOG_ERROR, "Impossible d'allouer le tri des objets");
    return false;
  }
  for (int i = 0; i < count; i++) {
    Item *item = &items[firstSlot + i];
    int rarestRank = 0;
    for (int j = 0; j < item->effectCount; j++) {
      int effect = item->effects[j];
      if (effect >= 0 && effect < effectCount) {
        int rank = ItemsRarityRank(effects[effect].rarity);
        if (rank > rarestRank)
          rarestRank = rank;
      }
    }
    ordered[i] = (StoredItemSortEntry){*item, firstSlot + i, rarestRank};
  }

  int (*compare)(const void *, const void *) =
      order == ITEM_SORT_RARITY
          ? CompareStoredItemsByRarity
          : order == ITEM_SORT_NAME ? CompareStoredItemsByName
                                    : CompareStoredItemsByEffectCount;
  qsort(ordered, (size_t)count, sizeof(*ordered), compare);
  for (int i = 0; i < count; i++)
    items[firstSlot + i] = ordered[i].item;

  rewardSlot = RemapSortedStorageSlot(rewardSlot, firstSlot, ordered, count);
  for (int i = 0; i < rewardQueueCount; i++)
    rewardQueue[i] =
        RemapSortedStorageSlot(rewardQueue[i], firstSlot, ordered, count);
  contextSlot =
      RemapSortedStorageSlot(contextSlot, firstSlot, ordered, count);
  equipWarningSlot =
      RemapSortedStorageSlot(equipWarningSlot, firstSlot, ordered, count);
  free(ordered);
  return true;
}

const char *ItemsEffectText(int effect) {
  LoadData();
  return effect >= 0 && effect < effectCount ? effects[effect].text : "";
}

const char *ItemsEffectRarity(int effect) {
  LoadData();
  return effect >= 0 && effect < effectCount ? effects[effect].rarity : "";
}

const char *ItemsEffectGame(int effect) {
  LoadData();
  return effect >= 0 && effect < effectCount ? effects[effect].game : "ALL";
}

bool ItemsEffectCanBeForged(int effect) {
  LoadData();
  return effect >= 0 && effect < effectCount &&
         strcmp(effects[effect].rarity, "MAUDIT") != 0 &&
         !UniqueBossEffectIsObsolete(effect);
}

const char *ItemsItemName(int slot) {
  return items && slot >= 0 && slot < itemSlotCount ? items[slot].name : "";
}

const char *ItemsItemRarestEffectRarity(int slot) {
  if (!items || slot < 0 || slot >= itemSlotCount || !items[slot].name[0])
    return "";
  LoadData();
  const char *rarest = "";
  int rank = 0;
  for (int i = 0; i < items[slot].effectCount; i++) {
    int effect = items[slot].effects[i];
    if (effect < 0 || effect >= effectCount)
      continue;
    int effectRank = ItemsRarityRank(effects[effect].rarity);
    if (effectRank > rank) {
      rarest = effects[effect].rarity;
      rank = effectRank;
    }
  }
  return rarest;
}

int ItemsRewardSlot(void) { return rewardSlot; }

void ItemsDrawPreview(int slot, Rectangle box) {
  if (!items || slot < 0 || slot >= itemSlotCount || !items[slot].name[0])
    return;
  DrawItem(&items[slot], box);
}

void ItemsDrawKnownPreview(const char *name, Rectangle box) {
  if (!name || !name[0])
    return;
  int index = -1;
  for (int i = 0; i < knownItemCount; i++)
    if (strcmp(knownItems[i], name) == 0) {
      index = i;
      break;
    }
  if (index < 0)
    return;
  if (knownPreviewTextures[index].id == 0) {
    knownPreviewTextures[index] =
        LoadTexture(TextFormat("assets/items/%s.png", name));
  }
  Texture2D texture = knownPreviewTextures[index];
  if (texture.id == 0)
    return;
  float padding = box.width * 0.08f;
  Rectangle target = {box.x + padding, box.y + padding,
                      box.width - padding * 2, box.height - padding * 2};
  Rectangle source = {0, 0, (float)texture.width, (float)texture.height};
  DrawTexturePro(texture, source, target, (Vector2){0, 0}, 0, WHITE);
}

void ItemsDrawTooltip(int slot, Vector2 position) {
  if (!items || slot < 0 || slot >= itemSlotCount || !items[slot].name[0])
    return;
  DrawItemTooltip(slot, &items[slot], position);
}

static bool KnownItemName(const char *name) {
  for (int i = 0; name && i < knownItemCount; i++)
    if (strcmp(knownItems[i], name) == 0)
      return true;
  return false;
}

bool ItemsForgeCanComplete(const char *name, const int *selectedEffects,
                           int selectedEffectCount, const int *sacrifices,
                           int sacrificeCount) {
  LoadData();
  LoadCollection();
  if (!name || !KnownItemName(name) || !selectedEffects ||
      selectedEffectCount < 1 || selectedEffectCount > EFFECT_LIMIT ||
      !sacrifices || sacrificeCount < 1 || sacrificeCount > 6)
    return false;
  bool hasUniqueEffect = false;
  for (int i = 0; i < selectedEffectCount; i++) {
    int effect = selectedEffects[i];
    if (!ItemsEffectCanBeForged(effect) || !knownEffects[effect])
      return false;
    if (strcmp(effects[effect].rarity, "UNIQUE") == 0)
      hasUniqueEffect = true;
    for (int j = 0; j < i; j++)
      if (selectedEffects[j] == effect)
        return false;
  }
  if (hasUniqueEffect && selectedEffectCount != 1)
    return false;
  for (int i = 0; i < sacrificeCount; i++) {
    int slot = sacrifices[i];
    if (slot < 0 || slot >= itemSlotCount || !items[slot].name[0])
      return false;
    for (int j = 0; j < i; j++)
      if (sacrifices[j] == slot)
        return false;
  }
  return true;
}

int ItemsForgeComplete(const char *name, const int *selectedEffects,
                       int selectedEffectCount, const int *sacrifices,
                       int sacrificeCount) {
  if (!ItemsForgeCanComplete(name, selectedEffects, selectedEffectCount,
                             sacrifices, sacrificeCount))
    return -1;
  if (highestStoredSlot > INT_MAX - 2)
    return -1;
  int requiredCapacity = highestStoredSlot + 2;
  if (requiredCapacity < EQUIPPED_COUNT + 1)
    requiredCapacity = EQUIPPED_COUNT + 1;
  if (!ReserveItemCapacity(requiredCapacity))
    return -1;
  for (int i = 0; i < sacrificeCount; i++) {
    RemoveQueuedRewardSlot(sacrifices[i]);
    UnloadItem(&items[sacrifices[i]]);
  }
  CompactStoredItems();
  int slot = FindEmptyStorage();
  if (slot < 0)
    return -1;
  Item *item = &items[slot];
  snprintf(item->name, sizeof(item->name), "%s", name);
  if (slot > highestStoredSlot)
    highestStoredSlot = slot;
  item->useLimit = GetRandomValue(4, 8);
  item->usesLeft = item->useLimit;
  item->effectCount = selectedEffectCount;
  memcpy(item->effects, selectedEffects,
         (size_t)selectedEffectCount * sizeof(selectedEffects[0]));
  if (ItemUniqueEffectIndex(item) >= 0)
    item->useLimit = item->usesLeft = 1;
  for (int i = 0; i < selectedEffectCount; i++)
    if (strcmp(effects[selectedEffects[i]].rarity, "LEGENDAIRE") == 0 ||
        strcmp(effects[selectedEffects[i]].rarity, "UNIQUE") == 0) {
      AchievementsUnlock(ACHIEVEMENT_LEGENDARY_ITEM);
      AchievementsUnlockTitle(TITLE_FORTUNE);
      break;
    }
  RememberItem(item);
  QueueItemReward(slot);
  return slot;
}

void ItemsCloseContextMenu(void) {
  contextSlot = -1;
  questContextSlot = -1;
  ResetUniqueConversion();
}

bool ItemsLetterReaderActive(void) { return letterReaderActive; }

void ItemsCloseLetterReader(void) { CloseLetterReader(); }

static void DrawContextButton(Rectangle box, const char *label, Color fill) {
  float scale = GameUiScale();
  bool hover = CheckCollisionPointRec(GetMousePosition(), box);
  if (hover)
    fill = Fade(fill, 0.82f);
  DrawRectangleRec(box, fill);
  DrawRectangleLinesEx(box, 2 * scale, RAYWHITE);
  int size = GameUiPixels(14);
  while (size > GameUiPixels(9) && MeasureText(label, size) > box.width - 12 * scale)
    size--;
  DrawText(label, (int)(box.x + (box.width - MeasureText(label, size)) / 2),
           (int)(box.y + (box.height - size) / 2), size, RAYWHITE);
}

static void DrawQuestItemTooltip(QuestItem *item, Vector2 position) {
  float scale = GameUiScale();
  int margin = GameUiPixels(6);
  int padding = GameUiPixels(10);
  int width = GameUiPixels(360);
  if (width > GetScreenWidth() - margin * 2)
    width = GetScreenWidth() - margin * 2;
  int fontSize = GameUiPixels(15);
  const char *description = QuestItemDescription(item);
  int height = GameUiPixels(48) +
               EffectLineCount(description, width - padding * 2, fontSize) *
                   (fontSize + GameUiPixels(3));
  Rectangle box = {position.x + GameUiPixels(14), position.y + GameUiPixels(14),
                   width, height};
  if (box.x + box.width > GetScreenWidth() - margin)
    box.x = position.x - box.width - GameUiPixels(14);
  if (box.y + box.height > GetScreenHeight() - margin)
    box.y = GetScreenHeight() - box.height - margin;
  if (box.x < margin)
    box.x = margin;
  if (box.y < margin)
    box.y = margin;

  DrawRectangleRec(box, (Color){15, 15, 20, 245});
  DrawRectangleLinesEx(box, 2 * scale, (Color){225, 185, 40, 255});
  DrawText(QuestItemName(item), (int)box.x + padding,
           (int)box.y + GameUiPixels(8), GameUiPixels(17),
           (Color){245, 220, 130, 255});
  int y = (int)box.y + GameUiPixels(32);
  DrawEffectText(description, (int)box.x + padding, &y, width - padding * 2,
                 fontSize, RAYWHITE);
}

static void UseQuestSeal(int slot) {
  if (slot < 1 || slot >= QUEST_ITEM_COUNT ||
      questItems[slot].type != QUEST_SEAL ||
      questItems[0].type != QUEST_LETTER || letterSealsUsed >= 5)
    return;
  if (questItems[slot].texture.id != 0)
    UnloadTexture(questItems[slot].texture);
  questItems[slot] = (QuestItem){0};
  if (questItems[0].texture.id != 0)
    UnloadTexture(questItems[0].texture);
  questItems[0].texture = (Texture2D){0};
  letterSealsUsed++;
  questContextSlot = -1;
}

static void OpenLetterReader(void) {
  if (!letterText)
    letterText = LoadFileText("assets/open_letter.txt");
  if (!letterText) {
    TraceLog(LOG_ERROR, "Texte assets/open_letter.txt introuvable");
    return;
  }
  letterReaderActive = true;
  letterReachedEnd = false;
  letterScroll = letterMaxScroll = 0;
  questContextSlot = -1;
}

static float DrawLetterText(const char *text, Rectangle box, float size,
                            float scroll) {
  if (!letterFontReady) {
    int codepointCount = 0;
    const int *codepoints = GameUiFontCodepoints(&codepointCount);
    letterFont =
        LoadFontEx("assets/NotoSerif-Light.ttf", 64, codepoints, codepointCount);
    letterFontReady = letterFont.texture.id != 0;
    if (!letterFontReady)
      letterFont = GetFontDefault();
  }
  Font textFont = letterFont;
  float spacing = GameUiScale();
  float lineHeight = size * 1.55f;
  float startY = box.y - scroll;
  float y = startY;
  char line[1024] = "";
  const char *word = text;

  BeginScissorMode((int)box.x, (int)box.y, (int)box.width, (int)box.height);
  while (*word) {
    if (*word == '\n' || *word == '\r') {
      if (line[0]) {
        if (y + lineHeight > box.y && y < box.y + box.height)
          DrawTextEx(textFont, line, (Vector2){box.x, y}, size, spacing,
                     (Color){224, 214, 194, 255});
        line[0] = '\0';
        y += lineHeight;
      } else {
        y += lineHeight * 0.6f;
      }
      if (*word == '\r' && word[1] == '\n')
        word++;
      word++;
      continue;
    }
    if (*word == ' ' || *word == '\t') {
      word++;
      continue;
    }

    const char *end = word;
    while (*end && *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r')
      end++;
    size_t length = (size_t)(end - word);
    char candidate[1024];
    snprintf(candidate, sizeof(candidate), "%s%s%.*s", line,
             line[0] ? " " : "", (int)length, word);
    if (line[0] &&
        MeasureTextEx(textFont, candidate, size, spacing).x > box.width) {
      if (y + lineHeight > box.y && y < box.y + box.height)
        DrawTextEx(textFont, line, (Vector2){box.x, y}, size, spacing,
                   (Color){224, 214, 194, 255});
      line[0] = '\0';
      y += lineHeight;
      continue;
    }
    snprintf(line, sizeof(line), "%s", candidate);
    word = end;
  }
  if (line[0] && y + lineHeight > box.y && y < box.y + box.height)
    DrawTextEx(textFont, line, (Vector2){box.x, y}, size, spacing,
               (Color){224, 214, 194, 255});
  EndScissorMode();
  return fmaxf(0, y + lineHeight - startY);
}

static void CloseLetterReader(void) {
  if (letterReachedEnd)
    letterRead = true;
  letterReaderActive = false;
}

static void DrawLetterReader(void) {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  float scale = GameUiScale();
  float panelW = fminf(sw * 0.9f, 900 * fmaxf(scale, 1));
  float panelH = fminf(sh * 0.9f, 900 * fmaxf(scale, 1));
  Rectangle panel = {(sw - panelW) / 2, (sh - panelH) / 2, panelW, panelH};
  Rectangle textBox = {panel.x + panelW * 0.09f, panel.y + panelH * 0.17f,
                       panelW * 0.82f, panelH * 0.65f};
  float size = fmaxf(22 * scale, fminf(sh * 0.038f, 34 * scale));
  float closeH = fmaxf(34 * scale, panelH * 0.07f);
  Rectangle close = {panel.x + panelW * 0.32f,
                     panel.y + panelH * 0.87f, panelW * 0.36f, closeH};

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 205});
  DrawRectangleRounded(panel, 0.035f, 12, (Color){26, 23, 21, 250});
  DrawRectangleRoundedLinesEx(panel, 0.035f, 12, 2 * scale,
                              (Color){205, 166, 96, 255});
  DrawText("Ma chérie...", (int)(panel.x + panelW * 0.09f),
           (int)(panel.y + panelH * 0.07f), GameUiPixels(30),
           (Color){230, 194, 130, 255});

  if (letterText && CheckCollisionPointRec(GetMousePosition(), textBox))
    letterScroll -= GetMouseWheelMove() * size * 3;
  letterScroll = fmaxf(0, fminf(letterScroll, letterMaxScroll));
  letterMaxScroll =
      DrawLetterText(letterText ? letterText : "", textBox, size, letterScroll) -
      textBox.height;
  letterMaxScroll = fmaxf(0, letterMaxScroll);
  letterScroll = fmaxf(0, fminf(letterScroll, letterMaxScroll));
  if (letterScroll >= letterMaxScroll - 2 * scale)
    letterReachedEnd = true;

  if (letterMaxScroll > 0) {
    float trackWidth = 3 * scale;
    float thumbHeight =
        fmaxf(trackWidth * 3,
              textBox.height * textBox.height /
                  (textBox.height + letterMaxScroll));
    float thumbY = textBox.y +
                   (textBox.height - thumbHeight) * letterScroll /
                       letterMaxScroll;
    DrawRectangleRec((Rectangle){textBox.x + textBox.width - trackWidth,
                                 textBox.y, trackWidth, textBox.height},
                     (Color){120, 104, 79, 160});
    DrawRectangleRec((Rectangle){textBox.x + textBox.width - trackWidth,
                                 thumbY, trackWidth, thumbHeight},
                     (Color){199, 157, 90, 235});
  }

  DrawContextButton(close, "FERMER", (Color){82, 61, 38, 255});
  if (IsKeyPressed(KEY_ESCAPE) ||
      (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
       CheckCollisionPointRec(GetMousePosition(), close)))
    CloseLetterReader();
}

static void DrawQuestItemContextMenu(int slot, bool acceptInput) {
  QuestItem *item = &questItems[slot];
  float scale = GameUiScale();
  int margin = GameUiPixels(12);
  int width = GetScreenWidth() - margin * 2;
  int maxWidth = GameUiPixels(430);
  if (width > maxWidth)
    width = maxWidth;
  int height = GameUiPixels(214);
  Rectangle box = {(GetScreenWidth() - width) / 2.0f,
                   (GetScreenHeight() - height) / 2.0f, width, height};
  Rectangle action = {box.x + margin, box.y + box.height - GameUiPixels(78),
                      box.width - margin * 2, GameUiPixels(30)};
  Rectangle close = {box.x + margin, box.y + box.height - GameUiPixels(40),
                     box.width - margin * 2, GameUiPixels(30)};
  bool useSeal = item->type == QUEST_SEAL &&
                 questItems[0].type == QUEST_LETTER && letterSealsUsed < 5;
  bool readLetter = item->type == QUEST_LETTER && letterSealsUsed == 5;

  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                (Color){0, 0, 0, 150});
  DrawRectangleRec(box, (Color){15, 15, 20, 250});
  DrawRectangleLinesEx(box, 2 * scale, (Color){225, 185, 40, 255});
  DrawText(QuestItemName(item), (int)box.x + margin,
           (int)box.y + GameUiPixels(12), GameUiPixels(20),
           (Color){245, 220, 130, 255});
  int y = (int)box.y + GameUiPixels(48);
  DrawEffectText(QuestItemDescription(item), (int)box.x + margin, &y,
                 width - margin * 2, GameUiPixels(15), RAYWHITE);
  if (useSeal)
    DrawContextButton(action, "UTILISER SUR LA LETTRE",
                      (Color){92, 67, 34, 255});
  else if (readLetter)
    DrawContextButton(action, "LIRE LA LETTRE", (Color){92, 67, 34, 255});
  DrawContextButton(close, "FERMER", (Color){45, 50, 58, 255});

  if (!acceptInput)
    return;
  Vector2 mouse = GetMousePosition();
  if (IsKeyPressed(KEY_ESCAPE) ||
      (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) &&
       !CheckCollisionPointRec(mouse, box))) {
    questContextSlot = -1;
  } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (useSeal && CheckCollisionPointRec(mouse, action))
      UseQuestSeal(slot);
    else if (readLetter && CheckCollisionPointRec(mouse, action))
      OpenLetterReader();
    else if (CheckCollisionPointRec(mouse, close) ||
             !CheckCollisionPointRec(mouse, box))
      questContextSlot = -1;
  }
}

static void DrawItemContextMenu(Item *item, bool acceptInput) {
  float scale = GameUiScale();
  int orderedEffects[EFFECT_LIMIT];
  int orderedCount = SortItemEffectsByRarity(item, orderedEffects);
  int margin = GameUiPixels(12);
  int width = (int)inventoryContextBox.width;
  int availableWidth = GetScreenWidth() - margin * 2;
  if (width > availableWidth)
    width = availableWidth;
  if (width < 1)
    width = availableWidth;
  int fontSize = GameUiPixels(17);
  int contentWidth = width - margin * 2;
  int height, lines;
  do {
    lines = 0;
    for (int i = 0; i < orderedCount; i++) {
      int effectIndex = orderedEffects[i];
      char effectText[320];
      FormatEffectText(&effects[effectIndex], effectText, sizeof(effectText));
      lines += EffectEntryLineCount(effectText, effects[effectIndex].rarity,
                                    contentWidth, fontSize);
    }
    height = GameUiPixels(132) + lines * (fontSize + GameUiPixels(3));
    if (height <= GetScreenHeight() - margin * 2 ||
        fontSize <= GameUiPixels(10))
      break;
    fontSize--;
  } while (true);
  int availableHeight = GetScreenHeight() - margin * 2;
  if (height > availableHeight)
    height = availableHeight;
  Rectangle box = {(GetScreenWidth() - width) / 2.0f,
                   (GetScreenHeight() - height) / 2.0f, width, height};
  int buttonHeight = GameUiPixels(30);
  int buttonGap = GameUiPixels(6);
  float actionWidth = (box.width - 2 * margin - 2 * buttonGap) / 3;
  Rectangle equip = {box.x + margin, box.y + box.height - GameUiPixels(42),
                     actionWidth, buttonHeight};
  Rectangle destroy = {equip.x + equip.width + buttonGap, equip.y,
                       actionWidth, buttonHeight};
  Rectangle close = {destroy.x + destroy.width + buttonGap, equip.y,
                     actionWidth, buttonHeight};

  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                (Color){0, 0, 0, 150});
  DrawRectangleRounded(box, 0.025f, 8, (Color){15, 15, 20, 250});
  DrawRectangleRoundedLinesEx(box, 0.025f, 8, 2 * scale,
                              (Color){225, 185, 40, 255});
  DrawText(item->name, (int)box.x + margin, (int)box.y + GameUiPixels(10),
           GameUiPixels(20),
           ItemsRarityColor(ItemsItemRarestEffectRarity(contextSlot)));
  DrawText(TextFormat("Valeur de vente : %d Etris", ItemsSellValue(contextSlot)),
           (int)box.x + margin, (int)box.y + GameUiPixels(36),
           GameUiPixels(14), (Color){222, 196, 128, 255});
  const char *usesText = ItemUniqueEffectIndex(item) >= 0
                             ? "UNIQUE : détruit après une victoire"
                             : TextFormat("Mini-jeux réussis restants : %d / %d",
                                          item->usesLeft, item->useLimit);
  DrawText(usesText,
           (int)box.x + margin, (int)box.y + GameUiPixels(52),
           GameUiPixels(14), (Color){180, 210, 245, 255});
  int y = (int)box.y + GameUiPixels(72);
  int contentHeight = (int)destroy.y - y - GameUiPixels(6);
  if (contentHeight < 0)
    contentHeight = 0;
  BeginScissorMode((int)box.x + margin, y, contentWidth, contentHeight);
  for (int i = 0; i < orderedCount; i++) {
    ItemEffect *effect = &effects[orderedEffects[i]];
    char effectText[320];
    FormatEffectText(effect, effectText, sizeof(effectText));
    DrawEffectEntry(effectText, effect->rarity, (int)box.x + margin, &y,
                    contentWidth, fontSize);
  }
  EndScissorMode();

  bool canEquip = contextSlot < EQUIPPED_COUNT ||
                  ItemCanEquipForRules(&items[contextSlot],
                                       rulesGame[0] ? rulesGame : NULL,
                                       rulesBossFloor);
  DrawContextButton(equip, contextSlot < EQUIPPED_COUNT ? "RETIRER"
                                                        : "ÉQUIPER",
                    (Color){92, 67, 34, 255});
  DrawContextButton(destroy,
                    TextFormat("VENDRE %d ETRIS", ItemsSellValue(contextSlot)),
                    (Color){94, 72, 33, 255});
  DrawContextButton(close, "FERMER", (Color){45, 50, 58, 255});

  if (!acceptInput)
    return;
  Vector2 mouse = GetMousePosition();
  if (IsKeyPressed(KEY_ESCAPE) ||
      (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) &&
       !CheckCollisionPointRec(mouse, box))) {
    contextSlot = -1;
  } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (CheckCollisionPointRec(mouse, equip)) {
      if (!canEquip) {
        if (ItemObsoleteUniqueEffectIndex(&items[contextSlot]) >= 0)
          OpenUniqueConversion(contextSlot);
        else
          ShowEquipWarning(contextSlot);
      } else {
        int destination = contextSlot < EQUIPPED_COUNT
                              ? FindEmptyStorage()
                              : FindEmpty(0, EQUIPPED_COUNT);
        if (destination >= 0) {
          MoveItemToSlot(contextSlot, destination);
          contextSlot = destination;
        }
      }
    } else if (CheckCollisionPointRec(mouse, destroy))
      ItemsSell(contextSlot);
    else if (CheckCollisionPointRec(mouse, close) ||
             !CheckCollisionPointRec(mouse, box))
      contextSlot = -1;
  }
}

static void DrawCenteredText(const char *text, Rectangle box, int y,
                             int fontSize, Color color) {
  int width = MeasureText(text, fontSize);
  DrawText(text, (int)(box.x + (box.width - width) / 2), y, fontSize, color);
}

static const char *UniqueBossName(int effect) {
  if (effect < 0 || effect >= effectCount)
    return "ce boss";
  if (strcmp(effects[effect].game, "BOSS1") == 0)
    return "Béhémot";
  if (strcmp(effects[effect].game, "BOSS2") == 0)
    return "Séléna";
  if (strcmp(effects[effect].game, "BOSS3") == 0)
    return "Agares";
  if (strcmp(effects[effect].game, "BOSS4") == 0)
    return "Dame Marika";
  if (strcmp(effects[effect].game, "FINALBOSS") == 0)
    return "le boss final";
  return "ce boss";
}

static void CompleteUniqueConversion(void) {
  int slot = uniqueConversionSlot;
  if (!items || slot < EQUIPPED_COUNT || slot >= itemSlotCount ||
      !items[slot].name[0] || pendingUniqueItem.effectCount != 1 ||
      UniqueBossEffectIsObsolete(pendingUniqueItem.effects[0])) {
    TraceLog(LOG_ERROR, "La transmutation UNIQUE n'a pas pu etre finalisee");
    ResetUniqueConversion();
    return;
  }
  UnloadItem(&items[slot]);
  items[slot] = pendingUniqueItem;
  pendingUniqueItem = (Item){0};
  uniqueConversionEffect = items[slot].effects[0];
  uniqueConversionState = UNIQUE_CONVERSION_RESULT;
  uniqueConversionTimer = 0;
  RememberItem(&items[slot]);
}

static void DrawUniqueConversionWindow(bool acceptInput) {
  if (!items || uniqueConversionSlot < EQUIPPED_COUNT ||
      uniqueConversionSlot >= itemSlotCount ||
      !items[uniqueConversionSlot].name[0]) {
    ResetUniqueConversion();
    return;
  }

  float scale = GameUiScale();
  int margin = GameUiPixels(14);
  int width = GameUiPixels(560);
  int availableWidth = GetScreenWidth() - margin * 2;
  if (width > availableWidth)
    width = availableWidth;
  if (width < 1)
    width = availableWidth;
  int height = GameUiPixels(300);
  if (height > GetScreenHeight() - margin * 2)
    height = GetScreenHeight() - margin * 2;
  Rectangle panel = {(GetScreenWidth() - width) / 2.0f,
                     (GetScreenHeight() - height) / 2.0f, width, height};
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                (Color){0, 0, 0, 170});
  DrawRectangleRounded(panel, 0.035f, 10, (Color){18, 17, 22, 250});
  DrawRectangleRoundedLinesEx(panel, 0.035f, 10, 2 * scale,
                              (Color){225, 185, 40, 255});

  const char *title =
      uniqueConversionState == UNIQUE_CONVERSION_NOTICE
          ? "EFFET UNIQUE OBSOLETE"
          : uniqueConversionState == UNIQUE_CONVERSION_ANIMATION
                ? "TRANSMUTATION UNIQUE"
                : "NOUVEL OBJET UNIQUE";
  int titleSize = GameUiPixels(22);
  DrawText(title, (int)(panel.x + (panel.width - MeasureText(title, titleSize)) / 2),
           (int)panel.y + GameUiPixels(18), titleSize,
           (Color){245, 220, 130, 255});

  int iconSize = GameUiPixels(82);
  if (uniqueConversionState == UNIQUE_CONVERSION_NOTICE) {
    Item *item = &items[uniqueConversionSlot];
    DrawText(item->name,
             (int)(panel.x + (panel.width - MeasureText(item->name,
                                                        GameUiPixels(18))) /
                                  2),
             (int)panel.y + GameUiPixels(54), GameUiPixels(18), RAYWHITE);
    Rectangle preview = {panel.x + margin, panel.y + GameUiPixels(86),
                         iconSize, iconSize};
    DrawItem(item, preview);
    char effectText[320];
    FormatEffectText(&effects[uniqueConversionEffect], effectText,
                     sizeof(effectText));
    int textY = (int)panel.y + GameUiPixels(88);
    int textX = (int)(preview.x + preview.width + margin);
    int textWidth = (int)(panel.x + panel.width - margin - textX);
    DrawEffectEntry(effectText, "UNIQUE", textX, &textY, textWidth,
                    GameUiPixels(14));
    char message[160];
    snprintf(message, sizeof(message),
             "Le boss associe a cet effet (%s) est deja vaincu.",
             UniqueBossName(uniqueConversionEffect));
    DrawEffectText(message, textX, &textY, textWidth, GameUiPixels(14),
                   (Color){220, 214, 200, 255});
    if (uniqueConversionFailed) {
      DrawText("Aucun effet UNIQUE valide n'est disponible.",
               textX, textY + GameUiPixels(8), GameUiPixels(13),
               (Color){238, 120, 110, 255});
    }

    float buttonGap = GameUiPixels(10);
    float buttonWidth = (panel.width - 2 * margin - buttonGap) / 2;
    Rectangle convert = {panel.x + margin,
                         panel.y + panel.height - GameUiPixels(48),
                         buttonWidth, GameUiPixels(34)};
    Rectangle close = {convert.x + buttonWidth + buttonGap, convert.y,
                       buttonWidth, convert.height};
    DrawContextButton(convert, "TRANSMUTER", (Color){92, 67, 34, 255});
    DrawContextButton(close, "ANNULER", (Color){45, 50, 58, 255});
    if (acceptInput) {
      Vector2 mouse = GetMousePosition();
      if (IsKeyPressed(KEY_ESCAPE) ||
          ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) &&
           !CheckCollisionPointRec(mouse, panel))) {
        ResetUniqueConversion();
      } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, convert)) {
          uniqueConversionFailed = !BeginUniqueConversion();
        } else if (CheckCollisionPointRec(mouse, close)) {
          ResetUniqueConversion();
        }
      }
    }
    return;
  }

  if (uniqueConversionState == UNIQUE_CONVERSION_ANIMATION) {
    float progress = 1.0f - uniqueConversionTimer / 1.8f;
    float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 18);
    Rectangle oldPreview = {panel.x + margin * 2,
                            panel.y + GameUiPixels(80), iconSize, iconSize};
    Rectangle newPreview = {panel.x + panel.width - margin * 2 - iconSize,
                            oldPreview.y, iconSize, iconSize};
    DrawItem(&items[uniqueConversionSlot], oldPreview);
    DrawItem(&pendingUniqueItem, newPreview);
    Vector2 center = {panel.x + panel.width / 2,
                      oldPreview.y + oldPreview.height / 2};
    float radius = GameUiPixels(20 + 14 * pulse);
    DrawCircleV(center, radius, Fade(ItemsRarityColor("UNIQUE"), 0.22f));
    DrawRing(center, radius, radius + GameUiPixels(4), -90,
             -90 + 360 * progress, 36, ItemsRarityColor("UNIQUE"));
    DrawText("ANCIEN", (int)(oldPreview.x +
                              (oldPreview.width -
                               MeasureText("ANCIEN", GameUiPixels(13))) /
                                  2),
             (int)(oldPreview.y + oldPreview.height + GameUiPixels(6)),
             GameUiPixels(13), (Color){200, 195, 185, 255});
    DrawText("NOUVEAU", (int)(newPreview.x +
                               (newPreview.width -
                                MeasureText("NOUVEAU", GameUiPixels(13))) /
                                   2),
             (int)(newPreview.y + newPreview.height + GameUiPixels(6)),
             GameUiPixels(13), (Color){255, 95, 105, 255});
    DrawCenteredText("Les resonances se recomposent...",
                     panel, (int)panel.y + GameUiPixels(226),
                     GameUiPixels(15), RAYWHITE);
    uniqueConversionTimer -= GetFrameTime();
    if (uniqueConversionTimer <= 0)
      CompleteUniqueConversion();
    return;
  }

  Item *item = &items[uniqueConversionSlot];
  Rectangle preview = {panel.x + (panel.width - iconSize) / 2,
                       panel.y + GameUiPixels(56), iconSize, iconSize};
  DrawItem(item, preview);
  DrawCenteredText(item->name, panel,
                   (int)(preview.y + preview.height + GameUiPixels(8)),
                   GameUiPixels(17), RAYWHITE);
  char effectText[320];
  FormatEffectText(&effects[uniqueConversionEffect], effectText,
                   sizeof(effectText));
  int textY = (int)panel.y + GameUiPixels(170);
  DrawEffectEntry(effectText, "UNIQUE", (int)panel.x + margin, &textY,
                  (int)panel.width - margin * 2, GameUiPixels(15));
  Rectangle close = {panel.x + margin,
                     panel.y + panel.height - GameUiPixels(48),
                     panel.width - margin * 2, GameUiPixels(34)};
  DrawContextButton(close, "FERMER", (Color){45, 50, 58, 255});
  if (acceptInput) {
    Vector2 mouse = GetMousePosition();
    if (IsKeyPressed(KEY_ESCAPE) ||
        (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
         (CheckCollisionPointRec(mouse, close) ||
          !CheckCollisionPointRec(mouse, panel))))
      ResetUniqueConversion();
  }
}

void ItemsDrawInventory(Rectangle equipped[EQUIPPED_COUNT],
                        Rectangle stored[STORAGE_VISIBLE_COUNT],
                        Rectangle quest[QUEST_ITEM_COUNT],
                        bool showQuestItems,
                        Rectangle contextBox, bool acceptInput) {
  if (!EnsureItemSlots(EQUIPPED_COUNT))
    return;
  if (equipWarningTimer > 0) {
    equipWarningTimer -= GetFrameTime();
    if (equipWarningTimer <= 0) {
      equipWarningTimer = 0;
      equipWarningSlot = -1;
    }
  }
  inventoryContextBox = contextBox;
  if (showQuestItems && letterReaderActive) {
    DrawLetterReader();
    return;
  }

  Vector2 mouse = GetMousePosition();
  int hovered = -1, hoveredQuest = -1;
  for (int i = 0; i < EQUIPPED_COUNT; i++) {
    if (items[i].name[0])
      DrawInventoryItem(i, equipped[i]);
    if (items[i].name[0] && CheckCollisionPointRec(mouse, equipped[i]))
      hovered = i;
  }

  Rectangle storageArea = {
      stored[0].x, stored[0].y,
      stored[STORAGE_COLUMNS - 1].x + stored[STORAGE_COLUMNS - 1].width -
          stored[0].x,
      stored[STORAGE_VISIBLE_COUNT - STORAGE_COLUMNS].y +
          stored[STORAGE_VISIBLE_COUNT - STORAGE_COLUMNS].height -
          stored[0].y};
  int highestStoredSlot = HighestStoredSlot();
  int extraSlots =
      highestStoredSlot - (EQUIPPED_COUNT + STORAGE_VISIBLE_COUNT - 1);
  int rowsPastView =
      extraSlots > 0
          ? extraSlots / STORAGE_COLUMNS +
                (extraSlots % STORAGE_COLUMNS != 0)
          : 0;
  int maxScrollRows =
      (INT_MAX - (EQUIPPED_COUNT + STORAGE_VISIBLE_COUNT - 1)) /
      STORAGE_COLUMNS;
  if (rowsPastView > maxScrollRows)
    rowsPastView = maxScrollRows;
  int maxScrollOffset = rowsPastView * STORAGE_COLUMNS;
  if (storageScrollOffset > maxScrollOffset)
    storageScrollOffset = maxScrollOffset;
  bool canScroll = acceptInput &&
                   uniqueConversionState == UNIQUE_CONVERSION_CLOSED &&
                   contextSlot < 0 && questContextSlot < 0;
  if (canScroll && CheckCollisionPointRec(mouse, storageArea)) {
    storageWheelRemainder += GetMouseWheelMove();
    int wheelSteps = (int)storageWheelRemainder;
    if (wheelSteps != 0) {
      storageWheelRemainder -= wheelSteps;
      storageScrollOffset -= wheelSteps * STORAGE_COLUMNS;
      if (storageScrollOffset < 0)
        storageScrollOffset = 0;
      if (storageScrollOffset > maxScrollOffset)
        storageScrollOffset = maxScrollOffset;
    }
  }
  for (int i = 0; i < STORAGE_VISIBLE_COUNT; i++) {
    int slot = EQUIPPED_COUNT + storageScrollOffset + i;
    if (slot < itemSlotCount && items[slot].name[0])
      DrawInventoryItem(slot, stored[i]);
    if (slot < itemSlotCount && items[slot].name[0] &&
        CheckCollisionPointRec(mouse, stored[i]))
      hovered = slot;
  }
  DrawStorageScrollbar(stored, rowsPastView, canScroll);

  if (showQuestItems)
    for (int i = 0; i < QUEST_ITEM_COUNT; i++) {
      if (questItems[i].type != QUEST_EMPTY)
        DrawQuestItem(&questItems[i], quest[i]);
      if (questItems[i].type != QUEST_EMPTY &&
          CheckCollisionPointRec(mouse, quest[i]))
        hoveredQuest = i;
    }

  if (uniqueConversionState != UNIQUE_CONVERSION_CLOSED) {
    DrawUniqueConversionWindow(acceptInput);
    return;
  }

  if (contextSlot >= 0) {
    if (contextSlot < itemSlotCount && items[contextSlot].name[0])
      DrawItemContextMenu(&items[contextSlot], acceptInput);
    else
      contextSlot = -1;
    return;
  }
  if (questContextSlot >= 0) {
    if (showQuestItems && questContextSlot < QUEST_ITEM_COUNT &&
        questItems[questContextSlot].type != QUEST_EMPTY)
      DrawQuestItemContextMenu(questContextSlot, acceptInput);
    else
      questContextSlot = -1;
    return;
  }

  if (hovered >= 0)
    DrawItemTooltip(hovered, &items[hovered], mouse);
  else if (hoveredQuest >= 0)
    DrawQuestItemTooltip(&questItems[hoveredQuest], mouse);

  if (acceptInput && hovered >= 0 &&
      IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    contextSlot = hovered;
  } else if (acceptInput && hoveredQuest >= 0 &&
             IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    questContextSlot = hoveredQuest;
  } else if (acceptInput && hovered >= 0 &&
             IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    int destination = hovered < EQUIPPED_COUNT
                          ? FindEmptyStorage()
                          : FindEmpty(0, EQUIPPED_COUNT);
    bool canEquip = hovered < EQUIPPED_COUNT ||
                    ItemCanEquipForRules(&items[hovered],
                                         rulesGame[0] ? rulesGame : NULL,
                                         rulesBossFloor);
    if (hovered >= EQUIPPED_COUNT &&
        ItemObsoleteUniqueEffectIndex(&items[hovered]) >= 0) {
      OpenUniqueConversion(hovered);
    } else if (!canEquip) {
      ShowEquipWarning(hovered);
    } else if (destination >= 0) {
      MoveItemToSlot(hovered, destination);
    } else {
      if (destination < 0)
        TraceLog(LOG_WARNING,
                 "Impossible d'agrandir l'inventaire pour deplacer l'artefact");
    }
  }
}

void ItemsClearRewards(void) {
  rewardSlot = -1;
  questRewardSlot = -1;
  rewardTimer = 0;
  rewardQueueCount = rewardQueueIndex = 0;
}

void ItemsDrawReward(void) {
  if (rewardTimer <= 0)
    return;
  bool questReward = questRewardSlot >= 0 &&
                     questItems[questRewardSlot].type != QUEST_EMPTY;
  bool regularReward = items && rewardSlot >= 0 &&
                       rewardSlot < itemSlotCount &&
                       items[rewardSlot].name[0];
  if (!questReward && !regularReward) {
    rewardTimer = 0;
    return;
  }
  rewardTimer -= GetFrameTime();
  if (rewardTimer <= 0 && rewardQueueCount > 0 &&
      questRewardSlot < 0) {
    rewardQueueIndex++;
    if (rewardQueueIndex < rewardQueueCount) {
      rewardSlot = rewardQueue[rewardQueueIndex];
      questRewardSlot = -1;
      rewardTimer = 3;
    } else {
      rewardQueueCount = rewardQueueIndex = 0;
      rewardSlot = -1;
      rewardTimer = 0;
    }
  }
  float size = GetScreenHeight() * 0.09f;
  float minSize = GameUiPixels(56), maxSize = GameUiPixels(96);
  if (size < minSize)
    size = minSize;
  if (size > maxSize)
    size = maxSize;
  Rectangle box = {GetScreenWidth() / 2.0f - size / 2,
                   GetScreenHeight() * 0.94f -
                       GetScreenHeight() * 0.56f / 2 - size / 2,
                   size, size};
  DrawRectangleRec(box, (Color){20, 20, 24, 235});
  DrawRectangleLinesEx(box, 3 * GameUiScale(), (Color){245, 205, 70, 255});
  if (questReward) {
    DrawQuestItem(&questItems[questRewardSlot],
                  (Rectangle){box.x + 6, box.y + 6, box.width - 12,
                              box.height - 12});
    if (CheckCollisionPointRec(GetMousePosition(), box))
      DrawQuestItemTooltip(&questItems[questRewardSlot], GetMousePosition());
  } else {
    DrawItem(&items[rewardSlot], (Rectangle){box.x + 6, box.y + 6,
                                             box.width - 12, box.height - 12});
    if (CheckCollisionPointRec(GetMousePosition(), box))
      DrawItemTooltip(rewardSlot, &items[rewardSlot], GetMousePosition());
  }
}
