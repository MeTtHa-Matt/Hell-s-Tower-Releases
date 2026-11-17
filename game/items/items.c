#include "items.h"
#include "functionnal/functionnal.h"
#include "raylib.h"
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

static Item items[ITEM_COUNT];
static ItemEffect effects[EFFECT_LIMIT_COUNT];
static char itemNames[128][ITEM_NAME_LENGTH];
static int effectCount, itemNameCount;
static bool dataLoaded;
static int rewardSlot = -1;
static float rewardTimer;

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

static void DrawEffectText(const char *text, int x, int *y, int width,
                           int fontSize, Color color) {
  char line[256] = "";
  const char *word = text;
  while (*word) {
    const char *end = strchr(word, ' ');
    size_t wordLength = end ? (size_t)(end - word) : strlen(word);
    char candidate[256];
    snprintf(candidate, sizeof(candidate), "%s%.*s",
             line[0] ? " " : "", (int)wordLength, word);
    if (line[0] && MeasureText(TextFormat("%s%s", line, candidate), fontSize) > width) {
      DrawText(line, x, *y, fontSize, color);
      *y += fontSize + 3;
      line[0] = '\0';
    }
    strncat(line, candidate, sizeof(line) - strlen(line) - 1);
    if (!end)
      break;
    word = end + 1;
  }
  if (line[0]) {
    DrawText(line, x, *y, fontSize, color);
    *y += fontSize + 3;
  }
}

static int EffectLineCount(const char *text, int width, int fontSize) {
  char line[256] = "";
  int count = 0;
  const char *word = text;
  while (*word) {
    const char *end = strchr(word, ' ');
    size_t wordLength = end ? (size_t)(end - word) : strlen(word);
    char candidate[256];
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
  const int width = 390, fontSize = 17;
  int height = 34;
  for (int i = 0; i < item->effectCount; i++)
    height += EffectLineCount(effects[item->effects[i]].text, width - 20,
                              fontSize) * (fontSize + 3);
  height += 12;

  Rectangle box = {position.x + 14, position.y + 14, width, height};
  if (box.x + box.width > GetScreenWidth() - 6)
    box.x = position.x - box.width - 14;
  if (box.y + box.height > GetScreenHeight() - 6)
    box.y = GetScreenHeight() - box.height - 6;
  if (box.x < 6)
    box.x = 6;
  if (box.y < 6)
    box.y = 6;

  DrawRectangleRec(box, (Color){15, 15, 20, 245});
  DrawRectangleLinesEx(box, 2, (Color){225, 185, 40, 255});
  DrawText(item->name, (int)box.x + 10, (int)box.y + 8, 19,
           (Color){245, 220, 130, 255});
  int y = (int)box.y + 34;
  for (int i = 0; i < item->effectCount; i++)
    DrawEffectText(effects[item->effects[i]].text, (int)box.x + 10, &y,
                   width - 20, fontSize,
                   RarityColor(effects[item->effects[i]].rarity));
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
  rewardSlot = -1;
  rewardTimer = 0;
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
    int effect;
    bool duplicate;
    do {
      effect = GetRandomValue(0, effectCount - 1);
      duplicate = false;
      for (int j = 0; j < i; j++)
        if (item->effects[j] == effect)
          duplicate = true;
    } while (duplicate && effectCount >= item->effectCount);
    item->effects[i] = effect;
  }
  rewardSlot = slot;
  rewardTimer = 7;
}

void ItemsConsumeForGame(const char *game) {
  if (!game)
    return;
  LoadData();
  FunctionalReset();
  for (int i = 0; i < EQUIPPED_COUNT; i++) {
    if (!items[i].name[0])
      continue;
    bool applies = false;
    for (int j = 0; j < items[i].effectCount; j++) {
      ItemEffect *effect = &effects[items[i].effects[j]];
      if (strcmp(effect->game, "ALL") == 0 ||
          strcmp(effect->game, game) == 0) {
        applies = true;
        FunctionalAddEffect(items[i].effects[j]);
      }
    }
    if (applies) {
      if (rewardSlot == i) {
        rewardSlot = -1;
        rewardTimer = 0;
      }
      UnloadItem(&items[i]);
    }
  }
}

void ItemsDrawInventory(Rectangle equipped[EQUIPPED_COUNT],
                        Rectangle stored[STORED_COUNT]) {
  Rectangle *slots[ITEM_COUNT];
  for (int i = 0; i < EQUIPPED_COUNT; i++)
    slots[i] = &equipped[i];
  for (int i = 0; i < STORED_COUNT; i++)
    slots[EQUIPPED_COUNT + i] = &stored[i];

  Vector2 mouse = GetMousePosition();
  int hovered = -1;
  for (int i = 0; i < ITEM_COUNT; i++) {
    if (items[i].name[0])
      DrawItem(&items[i], *slots[i]);
    if (items[i].name[0] && CheckCollisionPointRec(mouse, *slots[i]))
      hovered = i;
  }

  if (hovered >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    int destination = hovered < EQUIPPED_COUNT
                          ? FindEmpty(EQUIPPED_COUNT, ITEM_COUNT)
                          : FindEmpty(0, EQUIPPED_COUNT);
    if (destination >= 0) {
      items[destination] = items[hovered];
      items[hovered] = (Item){0};
      if (rewardSlot == hovered)
        rewardSlot = destination;
      hovered = destination;
    }
  }

  if (hovered >= 0)
    DrawItemTooltip(&items[hovered], mouse);
}

void ItemsDrawReward(void) {
  if (rewardTimer <= 0 || rewardSlot < 0 || !items[rewardSlot].name[0])
    return;
  rewardTimer -= GetFrameTime();
  float size = GetScreenHeight() * 0.09f;
  if (size < 56)
    size = 56;
  if (size > 96)
    size = 96;
  Rectangle box = {GetScreenWidth() / 2.0f - size / 2,
                   GetScreenHeight() * 0.94f -
                       GetScreenHeight() * 0.56f / 2 - size / 2,
                   size, size};
  DrawRectangleRec(box, (Color){20, 20, 24, 235});
  DrawRectangleLinesEx(box, 3, (Color){245, 205, 70, 255});
  DrawItem(&items[rewardSlot], (Rectangle){box.x + 6, box.y + 6,
                                           box.width - 12, box.height - 12});
  if (CheckCollisionPointRec(GetMousePosition(), box))
    DrawItemTooltip(&items[rewardSlot], GetMousePosition());
}
