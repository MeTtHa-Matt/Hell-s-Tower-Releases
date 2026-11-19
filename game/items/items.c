#include "items.h"
#include "../achievements/achievements.h"
#include "functionnal/functionnal.h"
#include "../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EQUIPPED_COUNT 5
#define STORED_COUNT 25
#define ITEM_COUNT (EQUIPPED_COUNT + STORED_COUNT)
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
  Texture2D texture;
} Item;

typedef enum { QUEST_EMPTY, QUEST_LETTER, QUEST_SEAL } QuestItemType;

typedef struct {
  QuestItemType type;
  int bossFloor;
  Texture2D texture;
} QuestItem;

typedef struct {
  const char *name;
  int chance;
} RarityChance;

static Item items[ITEM_COUNT];
static QuestItem questItems[QUEST_ITEM_COUNT];
static ItemEffect effects[EFFECT_LIMIT_COUNT];
static char itemNames[128][ITEM_NAME_LENGTH];
static int effectCount, itemNameCount;
static bool dataLoaded;
static int rewardSlot = -1;
static int questRewardSlot = -1;
static float rewardTimer;
static int contextSlot = -1;
static int questContextSlot = -1;
static unsigned int bossSealMask;
static int letterSealsUsed;
static bool letterAwarded, letterRead, letterReaderActive, letterReachedEnd;
static char *letterText;
static float letterScroll, letterMaxScroll;
static const RarityChance rarityChances[] = {
    {"COMMUN", 50}, {"RARE", 30}, {"EPIQUE", 15},
    {"LEGENDAIRE", 4}, {"MAUDIT", 1},
};

static void CloseLetterReader(void);

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
  if (item->type == QUEST_SEAL)
    return TextFormat("Un étrange sceau hérité de %s après l'avoir vaincu.",
                      BossName(item->bossFloor));
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

static void UnloadItem(Item *item) {
  if (item->texture.id != 0)
    UnloadTexture(item->texture);
  *item = (Item){0};
}

static int PickEffectForItem(const Item *item, int selectedCount) {
  int roll = GetRandomValue(1, 100);
  const char *rarity = NULL;
  for (int i = 0; i < (int)(sizeof(rarityChances) / sizeof(rarityChances[0]));
       i++) {
    if (roll <= rarityChances[i].chance) {
      rarity = rarityChances[i].name;
      break;
    }
    roll -= rarityChances[i].chance;
  }

  int candidates[EFFECT_LIMIT_COUNT];
  int candidateCount = 0;
  for (int i = 0; rarity && i < effectCount; i++) {
    if (strcmp(effects[i].rarity, rarity) != 0)
      continue;
    bool alreadySelected = false;
    for (int j = 0; j < selectedCount; j++)
      if (item->effects[j] == i)
        alreadySelected = true;
    if (!alreadySelected)
      candidates[candidateCount++] = i;
  }

  if (candidateCount == 0) {
    TraceLog(LOG_WARNING, "Aucun effet disponible pour la rarete tiree");
    for (int i = 0; i < effectCount; i++) {
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

static Color RarityColor(const char *rarity) {
  if (strcmp(rarity, "COMMUN") == 0)
    return (Color){110, 210, 120, 255};
  if (strcmp(rarity, "RARE") == 0)
    return (Color){100, 175, 255, 255};
  if (strcmp(rarity, "EPIQUE") == 0)
    return (Color){205, 125, 255, 255};
  if (strcmp(rarity, "LEGENDAIRE") == 0)
    return (Color){255, 205, 70, 255};
  if (strcmp(rarity, "MAUDIT") == 0)
    return (Color){255, 100, 100, 255};
  return RAYWHITE;
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

static void DrawItemTooltip(Item *item, Vector2 position) {
  float scale = GameUiScale();
  int margin = GameUiPixels(6);
  int padding = GameUiPixels(10);
  int width = GameUiPixels(390);
  if (width > GetScreenWidth() - margin * 2)
    width = GetScreenWidth() - margin * 2;
  int fontSize = GameUiPixels(17);
  int height;
  do {
    height = GameUiPixels(46);
    for (int i = 0; i < item->effectCount; i++) {
      char effectText[320];
      FormatEffectText(&effects[item->effects[i]], effectText,
                       sizeof(effectText));
      height += EffectLineCount(effectText, width - padding * 2, fontSize) *
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
           (Color){245, 220, 130, 255});
  int y = (int)box.y + GameUiPixels(34);
  int contentHeight = (int)(box.y + box.height) - y - margin;
  if (contentHeight < 0)
    contentHeight = 0;
  BeginScissorMode((int)box.x, y, width, contentHeight);
  for (int i = 0; i < item->effectCount; i++) {
    ItemEffect *effect = &effects[item->effects[i]];
    char effectText[320];
    FormatEffectText(effect, effectText, sizeof(effectText));
    DrawEffectText(effectText, (int)box.x + padding, &y, width - padding * 2,
                   fontSize, RarityColor(effect->rarity));
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

static int FindEmpty(int first, int end) {
  for (int i = first; i < end; i++)
    if (!items[i].name[0])
      return i;
  return -1;
}

void ItemsReset(void) {
  for (int i = 0; i < ITEM_COUNT; i++)
    UnloadItem(&items[i]);
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
  contextSlot = -1;
  questContextSlot = -1;
  bossSealMask = 0;
  letterSealsUsed = 0;
  letterAwarded = letterRead = letterReaderActive = letterReachedEnd = false;
  letterScroll = letterMaxScroll = 0;
}

void ItemsUnload(void) {
  ItemsReset();
  dataLoaded = false;
  effectCount = itemNameCount = 0;
}

void ItemsLoad(const char *data) {
  ItemsReset();
  LoadData();
  if (!data || !data[0])
    return;

  char copy[8192];
  snprintf(copy, sizeof(copy), "%s", data);
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
      if (first && second) {
        *first++ = '\0';
        *second++ = '\0';
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
      int slot = atoi(record);
      if (slot >= 0 && slot < ITEM_COUNT && first[0] &&
          strlen(first) < ITEM_NAME_LENGTH && !strchr(first, '/') &&
          !strchr(first, '\\') && effectCount > 0) {
        Item *item = &items[slot];
        snprintf(item->name, sizeof(item->name), "%s", first);
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
}

void ItemsSave(char *data, size_t size) {
  if (!size)
    return;
  data[0] = '\0';
  size_t used = 0;
  for (int i = 0; i < ITEM_COUNT; i++) {
    if (!items[i].name[0])
      continue;
    int written = snprintf(data + used, size - used, "%s%d:%s:",
                           used ? ";" : "", i, items[i].name);
    if (written < 0 || (size_t)written >= size - used)
      return;
    used += (size_t)written;
    for (int j = 0; j < items[i].effectCount; j++) {
      written = snprintf(data + used, size - used, "%s%d",
                         j ? "," : "", items[i].effects[j]);
      if (written < 0 || (size_t)written >= size - used)
        return;
      used += (size_t)written;
    }
  }
  for (int i = 0; i < QUEST_ITEM_COUNT; i++) {
    if (questItems[i].type == QUEST_EMPTY)
      continue;
    int written = snprintf(data + used, size - used, "%sQ%d:%d:%d",
                           used ? ";" : "", i, questItems[i].type,
                           questItems[i].bossFloor);
    if (written < 0 || (size_t)written >= size - used)
      return;
    used += (size_t)written;
  }
  int written = snprintf(data + used, size - used, "%sQM:%d:%u:%d:%d",
                         used ? ";" : "", letterAwarded, bossSealMask,
                         letterSealsUsed, letterRead);
  if (written < 0 || (size_t)written >= size - used)
    TraceLog(LOG_ERROR, "Sauvegarde des objets de quête trop volumineuse");
}

void ItemsGrantQuestLetter(void) {
  LoadData();
  if (letterAwarded || questItems[0].type == QUEST_LETTER)
    return;
  questItems[0] = (QuestItem){QUEST_LETTER, 0, {0}};
  letterAwarded = true;
  questRewardSlot = 0;
  rewardSlot = -1;
  rewardTimer = 7;
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
  rewardTimer = 7;
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
  int slot = FindEmpty(EQUIPPED_COUNT, ITEM_COUNT);
  if (slot < 0) {
    TraceLog(LOG_WARNING, "Inventaire plein : item de niveau non attribue");
    rewardSlot = -1;
    rewardTimer = 0;
    return;
  }
  if (itemNameCount == 0 || effectCount == 0)
    return;

  Item *item = &items[slot];
  snprintf(item->name, sizeof(item->name), "%s",
           itemNames[GetRandomValue(0, itemNameCount - 1)]);
  item->effectCount = GetRandomValue(1, NEW_ITEM_EFFECT_LIMIT);
  if (item->effectCount > effectCount)
    item->effectCount = effectCount;
  for (int i = 0; i < item->effectCount; i++) {
    int effect = PickEffectForItem(item, i);
    if (effect < 0) {
      UnloadItem(item);
      rewardSlot = -1;
      rewardTimer = 0;
      return;
    }
    item->effects[i] = effect;
  }
  for (int i = 0; i < item->effectCount; i++)
    if (strcmp(effects[item->effects[i]].rarity, "LEGENDAIRE") == 0) {
      AchievementsUnlock(ACHIEVEMENT_LEGENDARY_ITEM);
      break;
    }
  rewardSlot = slot;
  questRewardSlot = -1;
  rewardTimer = 7;
}

void ItemsApplyForGame(const char *game) {
  if (!game)
    return;
  LoadData();
  FunctionalReset();
  bool applied[EFFECT_LIMIT_COUNT] = {false};
  for (int i = 0; i < EQUIPPED_COUNT; i++) {
    if (!items[i].name[0])
      continue;
    for (int j = 0; j < items[i].effectCount; j++) {
      int effectIndex = items[i].effects[j];
      ItemEffect *effect = &effects[effectIndex];
      if (!applied[effectIndex] &&
          (strcmp(effect->game, "ALL") == 0 ||
           strcmp(effect->game, game) == 0)) {
        FunctionalAddEffect(effectIndex);
        applied[effectIndex] = true;
      }
    }
  }
}

void ItemsDestroy(int slot) {
  if (slot < 0 || slot >= ITEM_COUNT || !items[slot].name[0])
    return;
  UnloadItem(&items[slot]);
  if (rewardSlot == slot) {
    rewardSlot = -1;
    rewardTimer = 0;
  }
  if (contextSlot == slot)
    contextSlot = -1;
}

void ItemsCloseContextMenu(void) {
  contextSlot = -1;
  questContextSlot = -1;
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
  Font textFont = GetFontDefault();
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
                     (Color){42, 37, 31, 255});
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
                   (Color){42, 37, 31, 255});
      line[0] = '\0';
      y += lineHeight;
      continue;
    }
    snprintf(line, sizeof(line), "%s", candidate);
    word = end;
  }
  if (line[0] && y + lineHeight > box.y && y < box.y + box.height)
    DrawTextEx(textFont, line, (Vector2){box.x, y}, size, spacing,
               (Color){42, 37, 31, 255});
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
  float size = fmaxf(14 * scale, fminf(sh * 0.028f, 25 * scale));
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

static void DrawQuestItemContextMenu(int slot) {
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

static void DrawItemContextMenu(Item *item) {
  float scale = GameUiScale();
  int margin = GameUiPixels(12);
  int width = GetScreenWidth() - margin * 2;
  int maxWidth = GameUiPixels(410);
  if (width > maxWidth)
    width = maxWidth;
  int fontSize = GameUiPixels(17);
  int contentWidth = width - margin * 2;
  int height, lines;
  do {
    lines = 0;
    for (int i = 0; i < item->effectCount; i++) {
      char effectText[320];
      FormatEffectText(&effects[item->effects[i]], effectText,
                       sizeof(effectText));
      lines += EffectLineCount(effectText, contentWidth, fontSize);
    }
    height = GameUiPixels(106) + lines * (fontSize + GameUiPixels(3));
    if (height <= GetScreenHeight() - margin * 2 ||
        fontSize <= GameUiPixels(10))
      break;
    fontSize--;
  } while (true);
  if (height > GetScreenHeight() - margin * 2)
    height = GetScreenHeight() - margin * 2;
  Rectangle box = {(GetScreenWidth() - width) / 2.0f,
                   (GetScreenHeight() - height) / 2.0f, width, height};
  int buttonHeight = GameUiPixels(30);
  int buttonGap = GameUiPixels(6);
  Rectangle destroy = {box.x + margin, box.y + box.height - GameUiPixels(42),
                       (box.width - 2 * margin - buttonGap) / 2, buttonHeight};
  Rectangle close = {destroy.x + destroy.width + buttonGap, destroy.y,
                     destroy.width, destroy.height};

  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                (Color){0, 0, 0, 150});
  DrawRectangleRec(box, (Color){15, 15, 20, 250});
  DrawRectangleLinesEx(box, 2 * scale, (Color){225, 185, 40, 255});
  DrawText(item->name, (int)box.x + margin, (int)box.y + GameUiPixels(10),
           GameUiPixels(20),
           (Color){245, 220, 130, 255});
  int y = (int)box.y + GameUiPixels(40);
  int contentHeight = (int)destroy.y - y - GameUiPixels(6);
  if (contentHeight < 0)
    contentHeight = 0;
  BeginScissorMode((int)box.x + margin, y, contentWidth, contentHeight);
  for (int i = 0; i < item->effectCount; i++) {
    ItemEffect *effect = &effects[item->effects[i]];
    char effectText[320];
    FormatEffectText(effect, effectText, sizeof(effectText));
    DrawEffectText(effectText, (int)box.x + margin, &y, contentWidth, fontSize,
                   RarityColor(effect->rarity));
  }
  EndScissorMode();

  DrawContextButton(destroy, "JETER L'ARTEFACT", (Color){120, 35, 35, 255});
  DrawContextButton(close, "FERMER", (Color){45, 50, 58, 255});

  Vector2 mouse = GetMousePosition();
  if (IsKeyPressed(KEY_ESCAPE) ||
      (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) &&
       !CheckCollisionPointRec(mouse, box))) {
    contextSlot = -1;
  } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (CheckCollisionPointRec(mouse, destroy))
      ItemsDestroy(contextSlot);
    else if (CheckCollisionPointRec(mouse, close) ||
             !CheckCollisionPointRec(mouse, box))
      contextSlot = -1;
  }
}

void ItemsDrawInventory(Rectangle equipped[EQUIPPED_COUNT],
                        Rectangle stored[STORED_COUNT],
                        Rectangle quest[QUEST_ITEM_COUNT]) {
  if (letterReaderActive) {
    DrawLetterReader();
    return;
  }

  Rectangle *slots[ITEM_COUNT];
  for (int i = 0; i < EQUIPPED_COUNT; i++)
    slots[i] = &equipped[i];
  for (int i = 0; i < STORED_COUNT; i++)
    slots[EQUIPPED_COUNT + i] = &stored[i];

  Vector2 mouse = GetMousePosition();
  int hovered = -1, hoveredQuest = -1;
  for (int i = 0; i < ITEM_COUNT; i++) {
    if (items[i].name[0])
      DrawItem(&items[i], *slots[i]);
    if (items[i].name[0] && CheckCollisionPointRec(mouse, *slots[i]))
      hovered = i;
  }
  for (int i = 0; i < QUEST_ITEM_COUNT; i++) {
    if (questItems[i].type != QUEST_EMPTY)
      DrawQuestItem(&questItems[i], quest[i]);
    if (questItems[i].type != QUEST_EMPTY &&
        CheckCollisionPointRec(mouse, quest[i]))
      hoveredQuest = i;
  }

  if (contextSlot >= 0) {
    if (contextSlot < ITEM_COUNT && items[contextSlot].name[0])
      DrawItemContextMenu(&items[contextSlot]);
    else
      contextSlot = -1;
    return;
  }
  if (questContextSlot >= 0) {
    if (questContextSlot < QUEST_ITEM_COUNT &&
        questItems[questContextSlot].type != QUEST_EMPTY)
      DrawQuestItemContextMenu(questContextSlot);
    else
      questContextSlot = -1;
    return;
  }

  if (hovered >= 0)
    DrawItemTooltip(&items[hovered], mouse);
  else if (hoveredQuest >= 0)
    DrawQuestItemTooltip(&questItems[hoveredQuest], mouse);

  if (hovered >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    contextSlot = hovered;
  } else if (hoveredQuest >= 0 &&
             IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    questContextSlot = hoveredQuest;
  } else if (hovered >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    int destination = hovered < EQUIPPED_COUNT
                          ? FindEmpty(EQUIPPED_COUNT, ITEM_COUNT)
                          : FindEmpty(0, EQUIPPED_COUNT);
    if (destination >= 0) {
      items[destination] = items[hovered];
      items[hovered] = (Item){0};
      if (rewardSlot == hovered)
        rewardSlot = destination;
    } else {
      TraceLog(LOG_WARNING, "Aucun emplacement libre pour deplacer l'artefact");
    }
  }
}

void ItemsDrawReward(void) {
  if (rewardTimer <= 0)
    return;
  bool questReward = questRewardSlot >= 0 &&
                     questItems[questRewardSlot].type != QUEST_EMPTY;
  bool regularReward = rewardSlot >= 0 && items[rewardSlot].name[0];
  if (!questReward && !regularReward) {
    rewardTimer = 0;
    return;
  }
  rewardTimer -= GetFrameTime();
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
      DrawItemTooltip(&items[rewardSlot], GetMousePosition());
  }
}
