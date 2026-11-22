#include "forge.h"
#include "../items/items.h"
#include "../tower.h"
#include "../ui_scale.h"
#include "../buttons/button_sounds.h"
#include "raylib.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FORGE_ENTRY_FEE 100
#define MAX_FORGE_EFFECTS 5
#define MAX_FORGE_ITEMS 6

typedef enum {
  FORGE_FEE,
  FORGE_ITEM,
  FORGE_EFFECTS,
  FORGE_CIRCLE,
  FORGE_ANIMATION
} ForgePage;

static bool active;
static ForgePage page;
static int knownOffset, effectOffset, selectedEffects[MAX_FORGE_EFFECTS];
static int selectedEffectCount, selectedItems[MAX_FORGE_ITEMS], selectedItemCount;
static int targetEffectCount, targetEffectPrice, requiredItems;
static int itemSortMode, effectSortMode;
static int forgeInventoryOffset;
static float forgeInventoryWheelRemainder;
static int forgeRewardSlot = -1;
static char targetName[128], notice[160];
static float animationTime;

static float Scale(void) { return GameUiScale(); }

static void TextFit(const char *text, Rectangle box, float size, Color color) {
  int fontSize = (int)(size * Scale());
  int minSize = GameUiPixels(10);
  while (fontSize > minSize && MeasureText(text, fontSize) > box.width - 12 * Scale())
    fontSize--;
  DrawText(text, (int)(box.x + (box.width - MeasureText(text, fontSize)) / 2),
           (int)(box.y + (box.height - fontSize) / 2), fontSize, color);
}

static void DrawTextWrappedFit(const char *text, Rectangle box, float size,
                               Color color) {
  int fontSize = (int)(size * Scale());
  int minSize = GameUiPixels(8);
  for (; fontSize >= minSize; fontSize--) {
    char lines[8][384];
    int lineCount = 0;
    char line[384] = "";
    const char *word = text;
    bool fits = true;
    while (*word) {
      const char *end = strchr(word, ' ');
      size_t wordLength = end ? (size_t)(end - word) : strlen(word);
      char candidate[384];
      snprintf(candidate, sizeof(candidate), "%s%.*s",
               line[0] ? " " : "", (int)wordLength, word);
      char combined[384];
      snprintf(combined, sizeof(combined), "%s%s", line, candidate);
      if (MeasureText(combined, fontSize) > box.width - 8 * Scale()) {
        if (!line[0] || lineCount >= 7) {
          fits = false;
          break;
        }
        snprintf(lines[lineCount], sizeof(lines[lineCount]), "%s", line);
        lineCount++;
        line[0] = '\0';
        snprintf(line, sizeof(line), "%.*s", (int)wordLength, word);
      } else {
        snprintf(line, sizeof(line), "%s", combined);
      }
      if (!end)
        break;
      word = end + 1;
    }
    if (*line && lineCount < 8) {
      snprintf(lines[lineCount], sizeof(lines[lineCount]), "%s", line);
      lineCount++;
    }
    int lineHeight = fontSize + GameUiPixels(2);
    if (fits && lineCount > 0 && lineCount * lineHeight <= box.height) {
      int y = (int)(box.y + (box.height - lineCount * lineHeight) / 2);
      BeginScissorMode((int)box.x, (int)box.y, (int)box.width,
                       (int)box.height);
      for (int i = 0; i < lineCount; i++)
        DrawText(lines[i],
                 (int)(box.x + (box.width - MeasureText(lines[i], fontSize)) / 2),
                 y + i * lineHeight, fontSize, color);
      EndScissorMode();
      return;
    }
  }
}

static void RarityCardColors(const char *rarity, Color *fill, Color *border) {
  if (strcmp(rarity, "COMMUN") == 0) {
    *fill = (Color){32, 42, 35, 255}; *border = (Color){110, 210, 120, 255};
  } else if (strcmp(rarity, "RARE") == 0) {
    *fill = (Color){30, 42, 58, 255}; *border = (Color){100, 175, 255, 255};
  } else if (strcmp(rarity, "EPIQUE") == 0) {
    *fill = (Color){52, 35, 61, 255}; *border = (Color){205, 125, 255, 255};
  } else if (strcmp(rarity, "LEGENDAIRE") == 0) {
    *fill = (Color){66, 52, 28, 255}; *border = (Color){255, 205, 70, 255};
  } else if (strcmp(rarity, "MAUDIT") == 0) {
    *fill = (Color){48, 32, 55, 255}; *border = (Color){185, 105, 175, 255};
  } else if (strcmp(rarity, "UNIQUE") == 0) {
    *fill = (Color){62, 28, 34, 255}; *border = (Color){255, 65, 75, 255};
  } else {
    *fill = (Color){36, 37, 42, 255}; *border = (Color){145, 124, 84, 255};
  }
}

static bool Button(Rectangle box, const char *label, bool enabled) {
  bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), box);
  Color fill = enabled ? (hover ? (Color){77, 60, 35, 255}
                                 : (Color){37, 34, 30, 255})
                       : (Color){30, 30, 32, 255};
  Color edge = enabled ? (hover ? (Color){255, 220, 135, 255}
                                 : (Color){180, 145, 75, 255})
                       : (Color){75, 75, 75, 255};
  DrawRectangleRounded(box, 0.12f, 8, fill);
  DrawRectangleRoundedLinesEx(box, 0.12f, 8, Scale() * 2, edge);
  TextFit(label, box, 17, enabled ? RAYWHITE : (Color){130, 130, 130, 255});
  bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (clicked)
    PlayButtonClickSound();
  return clicked;
}

static Rectangle Panel(void) {
  float sw = GetScreenWidth(), sh = GetScreenHeight();
  float width = fminf(sw * 0.94f, 1180 * fmaxf(Scale(), 1));
  float height = fminf(sh * 0.9f, 780 * fmaxf(Scale(), 1));
  return (Rectangle){(sw - width) / 2, (sh - height) / 2, width, height};
}

static void DrawPanel(Rectangle panel, const char *title) {
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 190});
  DrawRectangleRounded(panel, 0.025f, 12, (Color){18, 19, 24, 250});
  DrawRectangleRoundedLinesEx(panel, 0.025f, 12, Scale() * 3,
                              (Color){211, 176, 98, 255});
  TextFit(title, (Rectangle){panel.x + panel.width * 0.2f,
                             panel.y + GameUiPixels(14), panel.width * 0.6f,
                             GameUiPixels(42)},
          28, (Color){241, 213, 147, 255});
}

static void DrawNotice(Rectangle panel) {
  if (notice[0])
    TextFit(notice, (Rectangle){panel.x + 12 * Scale(),
                                panel.y + panel.height - GameUiPixels(86),
                                panel.width - 24 * Scale(), GameUiPixels(32)},
            16, (Color){244, 133, 112, 255});
}

static int EffectPrice(int effect) {
  const char *rarity = ItemsEffectRarity(effect);
  if (strcmp(rarity, "COMMUN") == 0)
    return 40;
  if (strcmp(rarity, "RARE") == 0)
    return 80;
  if (strcmp(rarity, "EPIQUE") == 0)
    return 150;
  if (strcmp(rarity, "LEGENDAIRE") == 0)
    return 300;
  if (strcmp(rarity, "MAUDIT") == 0)
    return 450;
  if (strcmp(rarity, "UNIQUE") == 0)
    return 1000;
  return 0;
}

static void SelectTarget(const char *name) {
  snprintf(targetName, sizeof(targetName), "%s", name);
  selectedEffectCount = 0;
  effectOffset = 0;
  notice[0] = '\0';
  page = FORGE_EFFECTS;
}

static void PrepareForge(void) {
  targetEffectCount = selectedEffectCount;
  targetEffectPrice = 80;
  for (int i = 0; i < selectedEffectCount; i++)
    targetEffectPrice += EffectPrice(selectedEffects[i]);
  if (selectedEffectCount > 1)
    targetEffectPrice += (selectedEffectCount - 1) * 60;
  requiredItems = 2 + (targetEffectPrice > 350) +
                  (targetEffectPrice > 700) +
                  (targetEffectPrice > 1200) +
                  (targetEffectPrice > 1800);
  selectedItemCount = 0;
  page = FORGE_CIRCLE;
}

static bool HasSelectedItem(int slot) {
  for (int i = 0; i < selectedItemCount; i++)
    if (selectedItems[i] == slot)
      return true;
  return false;
}

static void ToggleSacrifice(int slot) {
  for (int i = 0; i < selectedItemCount; i++)
    if (selectedItems[i] == slot) {
      for (int j = i + 1; j < selectedItemCount; j++)
        selectedItems[j - 1] = selectedItems[j];
      selectedItemCount--;
      return;
    }
  if (selectedItemCount < requiredItems && ItemsItemName(slot)[0])
    selectedItems[selectedItemCount++] = slot;
}

static void DrawFee(Rectangle panel) {
  DrawPanel(panel, "FORGER UN ARTEFACT");
  TextFit("La forge demande un droit d'entree non remboursable.",
          (Rectangle){panel.x + 20, panel.y + panel.height * 0.32f,
                      panel.width - 40, GameUiPixels(44)},
          20, RAYWHITE);
  TextFit(TextFormat("PAYER %d ETRIS", FORGE_ENTRY_FEE),
          (Rectangle){panel.x + panel.width * 0.28f,
                      panel.y + panel.height * 0.48f, panel.width * 0.44f,
                      GameUiPixels(46)},
          23, (Color){245, 220, 130, 255});
  if (Button((Rectangle){panel.x + panel.width * 0.12f,
                         panel.y + panel.height - GameUiPixels(74),
                         panel.width * 0.34f, GameUiPixels(48)},
             "ANNULER", true))
    ForgeClose();
  if (Button((Rectangle){panel.x + panel.width * 0.54f,
                         panel.y + panel.height - GameUiPixels(74),
                         panel.width * 0.34f, GameUiPixels(48)},
             "PAYER ET CONTINUER", TowerEtris() >= FORGE_ENTRY_FEE)) {
    TowerSpendEtris(FORGE_ENTRY_FEE);
    page = FORGE_ITEM;
    knownOffset = 0;
  }
  if (TowerEtris() < FORGE_ENTRY_FEE)
    TextFit("ETRIS INSUFFISANTS",
            (Rectangle){panel.x + 10, panel.y + panel.height - GameUiPixels(112),
                        panel.width - 20, GameUiPixels(28)},
            14, (Color){224, 116, 100, 255});
}

static void DrawItemChoice(Rectangle panel) {
  DrawPanel(panel, "CHOISIR UN ARTEFACT");
  int count = ItemsKnownItemCount();
  int sorted[128];
  for (int i = 0; i < count; i++)
    sorted[i] = i;
  for (int i = 1; i < count; i++) {
    int current = sorted[i], j = i - 1;
    const char *name = ItemsKnownItem(current);
    while (j >= 0) {
      const char *other = ItemsKnownItem(sorted[j]);
      int cmp = strcmp(name, other);
      if (itemSortMode == 1)
        cmp = -cmp;
      if (cmp >= 0)
        break;
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = current;
  }
  if (Button((Rectangle){panel.x + panel.width * 0.68f,
                         panel.y + GameUiPixels(58),
                         panel.width * 0.26f, GameUiPixels(34)},
             itemSortMode == 0 ? "TRI : NOM A-Z" : "TRI : NOM Z-A", true)) {
    itemSortMode = 1 - itemSortMode;
    knownOffset = 0;
  }
  int columns = 3, rows = 3;
  if (count == 0)
    TextFit("Vous n'avez encore obtenu aucun artefact.",
            (Rectangle){panel.x + 20, panel.y + panel.height * 0.42f,
                        panel.width - 40, GameUiPixels(42)},
            20, RAYWHITE);
  int visibleCount = columns * rows;
  float gap = GameUiPixels(12);
  float availableWidth = panel.width * 0.68f - gap * (columns - 1);
  float availableHeight = panel.height * 0.58f - gap * (rows - 1);
  float cardSize = fminf(availableWidth / columns, availableHeight / rows);
  if (cardSize < GameUiPixels(110))
    cardSize = GameUiPixels(110);
  float cardW = cardSize, cardH = cardSize;
  int startY = (int)(panel.y + panel.height * 0.2f);
  float startX = panel.x + (panel.width - (cardW * columns + gap * (columns - 1))) / 2;
  for (int i = 0; i < visibleCount && knownOffset + i < count; i++) {
    int column = i % columns, row = i / columns;
    Rectangle card = {startX + column * (cardW + gap),
                      startY + row * (cardH + gap), cardW, cardH};
    bool hover = CheckCollisionPointRec(GetMousePosition(), card);
    DrawRectangleRounded(card, 0.08f, 8,
                         hover ? (Color){77, 60, 35, 255}
                               : (Color){36, 37, 42, 255});
    DrawRectangleRoundedLinesEx(card, 0.08f, 8, Scale(),
                                hover ? (Color){255, 220, 135, 255}
                                      : (Color){145, 124, 84, 255});
    Rectangle preview = {card.x + card.width * 0.25f,
                         card.y + GameUiPixels(8),
                         card.width * 0.5f,
                         card.height - GameUiPixels(42)};
    const char *name = ItemsKnownItem(sorted[knownOffset + i]);
    ItemsDrawKnownPreview(name, preview);
    TextFit(name,
            (Rectangle){card.x + 4, card.y + card.height - GameUiPixels(34),
                        card.width - 8, GameUiPixels(28)},
            14, (Color){241, 213, 147, 255});
    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      SelectTarget(name);
  }
  if (count > visibleCount) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(),
          (Rectangle){panel.x, startY, panel.width, rows * (cardH + gap)}))
      knownOffset = (int)fmaxf(0, fminf(count - visibleCount,
                                         knownOffset - (int)wheel * columns));
    if (IsKeyPressed(KEY_DOWN) && knownOffset < count - visibleCount)
      knownOffset += columns;
    if (IsKeyPressed(KEY_UP) && knownOffset > 0)
      knownOffset -= columns;
  }
  DrawNotice(panel);
  if (Button((Rectangle){panel.x + panel.width * 0.36f,
                         panel.y + panel.height - GameUiPixels(64),
                         panel.width * 0.28f, GameUiPixels(42)},
             "ANNULER", true))
    ForgeClose();
}

static bool EffectSelected(int effect) {
  for (int i = 0; i < selectedEffectCount; i++)
    if (selectedEffects[i] == effect)
      return true;
  return false;
}

static void ToggleEffect(int effect) {
  if (!ItemsEffectCanBeForged(effect))
    return;
  for (int i = 0; i < selectedEffectCount; i++)
    if (selectedEffects[i] == effect) {
      for (int j = i + 1; j < selectedEffectCount; j++)
        selectedEffects[j - 1] = selectedEffects[j];
      selectedEffectCount--;
      notice[0] = '\0';
      return;
    }
  bool selectedUnique = false;
  for (int i = 0; i < selectedEffectCount; i++)
    if (strcmp(ItemsEffectRarity(selectedEffects[i]), "UNIQUE") == 0)
      selectedUnique = true;
  bool choosingUnique =
      strcmp(ItemsEffectRarity(effect), "UNIQUE") == 0;
  if ((choosingUnique && selectedEffectCount > 0) || selectedUnique) {
    snprintf(notice, sizeof(notice),
             "Un effet UNIQUE doit etre forge seul.");
    return;
  }
  notice[0] = '\0';
  if (selectedEffectCount < MAX_FORGE_EFFECTS)
    selectedEffects[selectedEffectCount++] = effect;
}

static void DrawEffectChoice(Rectangle panel) {
  DrawPanel(panel, TextFormat("EFFETS POUR %s  |  %d / %d",
                              targetName, selectedEffectCount,
                              MAX_FORGE_EFFECTS));
  int knownCount = ItemsKnownEffectCount(), count = 0, columns = 3, rows = 3;
  int sorted[128];
  for (int i = 0; i < knownCount; i++) {
    int effect = ItemsKnownEffect(i);
    if (ItemsEffectCanBeForged(effect))
      sorted[count++] = effect;
  }
  for (int i = 1; i < count; i++) {
    int current = sorted[i], j = i - 1;
    while (j >= 0) {
      int cmp = 0;
      if (effectSortMode == 0)
        cmp = ItemsRarityRank(ItemsEffectRarity(current)) -
              ItemsRarityRank(ItemsEffectRarity(sorted[j]));
      else if (effectSortMode == 1)
        cmp = strcmp(ItemsEffectGame(current), ItemsEffectGame(sorted[j]));
      else
        cmp = strcmp(ItemsEffectText(current), ItemsEffectText(sorted[j]));
      if (effectSortMode == 0 ? cmp <= 0 : cmp >= 0)
        break;
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = current;
  }
  static const char *effectSortLabels[] = {
      "TRI : RARETE", "TRI : MINI-JEU", "TRI : EFFET"};
  if (Button((Rectangle){panel.x + panel.width * 0.68f,
                         panel.y + GameUiPixels(58),
                         panel.width * 0.26f, GameUiPixels(34)},
             effectSortLabels[effectSortMode], true)) {
    effectSortMode = (effectSortMode + 1) % 3;
    effectOffset = 0;
  }
  int visibleCount = columns * rows;
  float gap = GameUiPixels(10);
  float availableWidth = panel.width * 0.82f - gap * (columns - 1);
  float availableHeight = panel.height * 0.58f - gap * (rows - 1);
  float cardSize = fminf(availableWidth / columns, availableHeight / rows);
  if (cardSize < GameUiPixels(130))
    cardSize = GameUiPixels(130);
  int startY = (int)(panel.y + panel.height * 0.2f);
  float startX = panel.x + (panel.width -
                           (cardSize * columns + gap * (columns - 1))) / 2;
  for (int i = 0; i < visibleCount && effectOffset + i < count; i++) {
    int effect = sorted[effectOffset + i];
    int column = i % columns, row = i / columns;
    Rectangle card = {startX + column * (cardSize + gap),
                      startY + row * (cardSize + gap),
                      cardSize, cardSize};
    bool selected = EffectSelected(effect);
    Color fill, border;
    RarityCardColors(ItemsEffectRarity(effect), &fill, &border);
    if (selected)
      fill = (Color){78, 60, 33, 255};
    DrawRectangleRounded(card, 0.08f, 6, fill);
    DrawRectangleRoundedLinesEx(card, 0.08f, 6, Scale(),
                                border);
    const char *game = ItemsEffectGame(effect);
    const char *gameLabel = strcmp(game, "ALL") == 0 ? "TOUS LES MINI-JEUX"
                            : strcmp(game, "BULLET_HELL") == 0 ? "BULLET HELL"
                            : strcmp(game, "RING") == 0 ? "RINGS"
                            : strcmp(game, "MIRROR") == 0 ? "MIRROR"
                            : strcmp(game, "SEALS") == 0 ? "SEALS"
                            : strcmp(game, "FINALBOSS") == 0 ? "BOSS FINAL"
                            : strcmp(game, "BOSS4") == 0 ? "DAME MARIKA"
                            : strcmp(game, "BOSS3") == 0 ? "AGARES"
                            : strcmp(game, "BOSS2") == 0 ? "SELENA"
                            : strcmp(game, "BOSS1") == 0 ? "BEHEMOT"
                            : game;
    const char *rarity = ItemsEffectRarity(effect);
    TextFit(TextFormat("(%c)", rarity[0] ? rarity[0] : ' '),
            (Rectangle){card.x + 4, card.y + 6, card.width - 8,
                        GameUiPixels(28)}, 16,
            ItemsRarityColor(ItemsEffectRarity(effect)));
    TextFit(gameLabel,
            (Rectangle){card.x + 4, card.y + GameUiPixels(34),
                        card.width - 8, GameUiPixels(30)}, 14,
            (Color){180, 210, 245, 255});
    DrawTextWrappedFit(ItemsEffectText(effect),
                       (Rectangle){card.x + 6, card.y + GameUiPixels(68),
                                   card.width - 12,
                                   card.height - GameUiPixels(92)},
                       16, RAYWHITE);
    if (selected)
      TextFit("[CHOISI]",
              (Rectangle){card.x + 4, card.y + card.height - GameUiPixels(20),
                          card.width - 8, GameUiPixels(18)}, 10,
              (Color){255, 220, 135, 255});
    if (CheckCollisionPointRec(GetMousePosition(), card) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      ToggleEffect(effect);
  }
  if (count > visibleCount) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(),
          (Rectangle){panel.x, startY, panel.width,
                      rows * (cardSize + gap)}))
      effectOffset = (int)fmaxf(0, fminf(count - visibleCount,
                                         effectOffset - (int)wheel * columns));
    if (IsKeyPressed(KEY_DOWN) && effectOffset < count - visibleCount)
      effectOffset += columns;
    if (IsKeyPressed(KEY_UP) && effectOffset > 0)
      effectOffset -= columns;
  }
  DrawNotice(panel);
  if (Button((Rectangle){panel.x + panel.width * 0.12f,
                         panel.y + panel.height - GameUiPixels(64),
                         panel.width * 0.32f, GameUiPixels(42)},
             "RETOUR", true))
    page = FORGE_ITEM;
  if (Button((Rectangle){panel.x + panel.width * 0.56f,
                         panel.y + panel.height - GameUiPixels(64),
                         panel.width * 0.32f, GameUiPixels(42)},
             "CHOISIR LES RESSOURCES", selectedEffectCount > 0))
    PrepareForge();
}

static int ForgePrice(void) { return targetEffectPrice; }

static void DrawForgeScrollArrow(Rectangle grid, bool up) {
  float scale = Scale();
  Vector2 center = {grid.x + grid.width / 2,
                    up ? grid.y + 8 * scale
                       : grid.y + grid.height - 8 * scale};
  Rectangle background = {center.x - 12 * scale, center.y - 8 * scale,
                          24 * scale, 16 * scale};
  DrawRectangleRounded(background, 0.3f, 4, (Color){12, 16, 20, 220});
  Vector2 tip = {center.x, center.y + (up ? -5 : 5) * scale};
  Vector2 left = {center.x - 6 * scale, center.y + (up ? 3 : -3) * scale};
  Vector2 right = {center.x + 6 * scale, center.y + (up ? 3 : -3) * scale};
  DrawTriangle(tip, left, right, (Color){245, 220, 130, 255});
  DrawTriangleLines(tip, left, right, (Color){12, 16, 20, 255});
}

static int DrawForgeInventory(Rectangle panel) {
  float gap = GameUiPixels(8);
  float leftWidth = panel.width * 0.48f;
  float availableHeight = panel.height - GameUiPixels(184);
  float size = fminf((leftWidth - GameUiPixels(48) - 4 * gap) / 5,
                     (availableHeight - 5 * gap) / 6);
  if (size < GameUiPixels(58))
    size = GameUiPixels(58);
  float x = panel.x + GameUiPixels(24);
  float y = panel.y + GameUiPixels(106);
  int highestSlot = ItemsHighestOccupiedSlot();
  int extraSlots = highestSlot - 29;
  int rowsPastView =
      extraSlots > 0 ? extraSlots / 5 + (extraSlots % 5 != 0) : 0;
  int maxOffset = rowsPastView > (INT_MAX - 29) / 5
                      ? ((INT_MAX - 29) / 5) * 5
                      : rowsPastView * 5;
  if (forgeInventoryOffset > maxOffset)
    forgeInventoryOffset = maxOffset;
  Rectangle grid = {x, y, 5 * size + 4 * gap, 6 * size + 5 * gap};
  if (CheckCollisionPointRec(GetMousePosition(), grid)) {
    forgeInventoryWheelRemainder += GetMouseWheelMove();
    int wheelSteps = (int)forgeInventoryWheelRemainder;
    if (wheelSteps != 0) {
      forgeInventoryWheelRemainder -= wheelSteps;
      forgeInventoryOffset -= wheelSteps * 5;
      if (forgeInventoryOffset < 0)
        forgeInventoryOffset = 0;
      if (forgeInventoryOffset > maxOffset)
        forgeInventoryOffset = maxOffset;
    }
  }
  int hoverSlot = -1;
  for (int i = 0; i < 30; i++) {
    int itemSlot = forgeInventoryOffset + i;
    Rectangle slot = {x + (i % 5) * (size + gap),
                      y + (i / 5) * (size + gap), size, size};
    bool occupied = itemSlot <= highestSlot &&
                    ItemsItemName(itemSlot)[0] != '\0';
    bool selected = HasSelectedItem(itemSlot);
    DrawRectangleRounded(slot, 0.08f, 4,
                         selected ? (Color){79, 63, 37, 255}
                                  : (Color){31, 37, 41, 255});
    DrawRectangleRoundedLinesEx(slot, 0.08f, 4, Scale(),
                                selected ? (Color){255, 218, 120, 255}
                                         : (Color){110, 117, 118, 255});
    if (occupied)
      ItemsDrawPreview(itemSlot, slot);
    if (occupied && selected) {
      DrawRectangleRounded(slot, 0.08f, 4,
                           (Color){255, 218, 120, 70});
    }
    if (occupied && CheckCollisionPointRec(GetMousePosition(), slot))
      hoverSlot = itemSlot;
  }
  if (forgeInventoryOffset > 0)
    DrawForgeScrollArrow(grid, true);
  if (highestSlot > forgeInventoryOffset + 29)
    DrawForgeScrollArrow(grid, false);
  if (hoverSlot >= 0) {
    TextFit(ItemsItemName(hoverSlot),
            (Rectangle){x, y + 6 * (size + gap), panel.width * 0.46f,
                        GameUiPixels(28)},
            14, ItemsRarityColor(
                    ItemsItemRarestEffectRarity(hoverSlot)));
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      ToggleSacrifice(hoverSlot);
  }
  return hoverSlot;
}

static void DrawForgeCircle(Rectangle panel) {
  Vector2 center = {panel.x + panel.width * 0.75f,
                    panel.y + panel.height * 0.5f};
  float radius = fminf(panel.width * 0.17f, panel.height * 0.26f);
  DrawCircleLines((int)center.x, (int)center.y, radius,
                  (Color){190, 151, 77, 255});
  DrawCircleLines((int)center.x, (int)center.y, radius * 1.35f,
                  (Color){104, 87, 62, 255});
  int price = ForgePrice();
  bool affordable = TowerEtris() >= price;
  DrawCircleV(center, radius * 0.72f,
              (Color){25, 27, 31, 245});
  DrawCircleLines((int)center.x, (int)center.y, radius * 0.72f,
                  (Color){224, 185, 102, 255});
  const char *priceLabel = TextFormat("REQUIS : %d", price);
  int priceSize = GameUiPixels(22);
  DrawText(priceLabel, (int)(center.x - MeasureText(priceLabel, priceSize) / 2),
           (int)(center.y - GameUiPixels(30)), priceSize,
           affordable ? (Color){239, 216, 159, 255}
                      : (Color){242, 98, 82, 255});
  DrawText(affordable ? "ETRIS" : "INSUFFISANT",
           (int)(center.x - MeasureText(affordable ? "ETRIS" : "INSUFFISANT",
                                        GameUiPixels(14)) / 2),
           (int)(center.y - GameUiPixels(2)), GameUiPixels(14),
           affordable ? RAYWHITE : (Color){242, 98, 82, 255});

  for (int i = 0; i < requiredItems; i++) {
    float angle = -PI / 2 + 2 * PI * i / requiredItems;
    Vector2 node = {center.x + cosf(angle) * radius * 1.35f,
                    center.y + sinf(angle) * radius * 1.35f};
    float nodeRadius = fminf(GameUiPixels(46), radius * 0.31f);
    DrawCircleV(node, nodeRadius,
                (Color){38, 39, 43, 255});
    DrawCircleLines((int)node.x, (int)node.y, nodeRadius,
                    (Color){209, 173, 101, 255});
    if (i < selectedItemCount) {
      int itemSlot = selectedItems[i];
      ItemsDrawPreview(itemSlot,
                       (Rectangle){node.x - nodeRadius * 0.72f,
                                   node.y - nodeRadius * 0.72f,
                                   nodeRadius * 1.44f, nodeRadius * 1.44f});
      if (CheckCollisionPointCircle(GetMousePosition(), node, nodeRadius))
        ItemsDrawTooltip(itemSlot, GetMousePosition());
      if (CheckCollisionPointCircle(GetMousePosition(), node, nodeRadius) &&
          IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        ToggleSacrifice(itemSlot);
    }
  }
}

static void CommitForge(void) {
  if (selectedItemCount != requiredItems || TowerEtris() < ForgePrice() ||
      !ItemsForgeCanComplete(targetName, selectedEffects,
                             targetEffectCount, selectedItems,
                             selectedItemCount))
    return;
  if (!TowerSpendEtris(ForgePrice()))
    return;
  int createdSlot = ItemsForgeComplete(targetName, selectedEffects,
                                       targetEffectCount, selectedItems,
                                       selectedItemCount);
  if (createdSlot < 0) {
    TowerAddEtris(ForgePrice());
    TraceLog(LOG_ERROR, "La forge n'a pas pu creer l'artefact");
    snprintf(notice, sizeof(notice), "La forge a echoue, Etris restaures.");
    return;
  }
  forgeRewardSlot = createdSlot;
  animationTime = 0;
  page = FORGE_ANIMATION;
}

static void DrawForgeCirclePage(Rectangle panel) {
  DrawPanel(panel, "CERCLE DE FORGE");
  int hoverSlot = DrawForgeInventory(panel);
  DrawForgeCircle(panel);
  if (hoverSlot >= 0)
    ItemsDrawTooltip(hoverSlot, GetMousePosition());
  if (Button((Rectangle){panel.x + panel.width * 0.55f,
                         panel.y + panel.height - GameUiPixels(62),
                         panel.width * 0.22f, GameUiPixels(42)},
             "ANNULER", true))
    ForgeClose();
  bool ready = selectedItemCount == requiredItems &&
               TowerEtris() >= ForgePrice();
  if (Button((Rectangle){panel.x + panel.width * 0.79f,
                         panel.y + panel.height - GameUiPixels(62),
                         panel.width * 0.17f, GameUiPixels(42)},
             "FORGER", ready))
    CommitForge();
  DrawNotice(panel);
}

/* ================= ANIMATION DE FORGE (statics uniquement) ================= */
#define FANIM_CHARGE 2.6f   /* montée en puissance du cercle runique */
#define FANIM_FLASH  0.35f  /* durée du flash blanc à l'impact */
#define FANIM_DONE   (FANIM_CHARGE + 2.0f) /* bouton CONTINUER disponible */

static float ForgeClamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

/* pseudo-aléatoire déterministe : aucun état supplémentaire */
static float ForgeRand01(int n) {
  unsigned x = (unsigned)n * 747796405u + 2891336453u;
  x = ((x >> ((x >> 28) + 4)) ^ x) * 277803737u;
  x = (x >> 22) ^ x;
  return (x & 0xFFFF) / 65535.0f;
}

/* apparition avec petit dépassement élastique */
static float ForgeEaseOutBack(float x) {
  float c1 = 1.70158f, c3 = c1 + 1;
  float p = x - 1;
  return 1 + c3 * p * p * p + c1 * p * p;
}

static Vector2 ForgePolar(Vector2 c, float angle, float dist) {
  return (Vector2){c.x + cosf(angle) * dist, c.y + sinf(angle) * dist};
}

/* anneau de runes : graduations qui tournent */
static void ForgeDrawRuneRing(Vector2 c, float r, int ticks, float rot,
                              float alpha, float s, Color color) {
  DrawCircleLines((int)c.x, (int)c.y, r, Fade(color, alpha));
  DrawCircleLines((int)c.x, (int)c.y, r * 0.93f, Fade(color, alpha * 0.5f));
  for (int i = 0; i < ticks; i++) {
    float a = rot + i * 2 * PI / ticks;
    float inner = (i % 3 == 0) ? 0.82f : 0.9f;
    DrawLineEx(ForgePolar(c, a, r * inner), ForgePolar(c, a, r * 1.04f),
               (i % 3 == 0 ? 3 : 2) * s, Fade(color, alpha));
  }
}

/* étoile à six branches (deux triangles) */
static void ForgeDrawHexagram(Vector2 c, float r, float rot, float alpha,
                              float s, Color color) {
  for (int tri = 0; tri < 2; tri++) {
    for (int j = 0; j < 3; j++) {
      float a1 = rot + tri * PI + j * 2 * PI / 3;
      float a2 = rot + tri * PI + (j + 1) * 2 * PI / 3;
      DrawLineEx(ForgePolar(c, a1, r), ForgePolar(c, a2, r), 2 * s,
                 Fade(color, alpha));
    }
  }
}

static void DrawForgeAnimation(Rectangle panel) {
  float t = animationTime, s = Scale();
  float u = t - FANIM_CHARGE;                 /* temps depuis l'impact */
  bool revealed = u >= 0;
  DrawPanel(panel, revealed ? "ARTEFACT FORGE" : "LA FORGE S'ANIME");

  Color rarityColor =
      ItemsRarityColor(ItemsItemRarestEffectRarity(forgeRewardSlot));
  Color gold = rarityColor;
  Color fire = rarityColor;
  float radius = fminf(panel.width, panel.height) * 0.21f;
  Vector2 center = {panel.x + panel.width / 2, panel.y + panel.height * 0.5f};

  /* tremblement : monte pendant la charge, s'éteint après l'impact */
  float k = ForgeClamp01(t / FANIM_CHARGE);
  float shake = revealed ? 10 * expf(-u * 7) : 5 * k * k * k;
  center.x += sinf(t * 63) * shake * s;
  center.y += cosf(t * 79) * shake * s;

  BeginScissorMode((int)panel.x, (int)panel.y, (int)panel.width,
                   (int)panel.height);
  BeginBlendMode(BLEND_ADDITIVE);

  /* braises qui montent depuis le bas de la forge */
  for (int i = 0; i < 46; i++) {
    float ph = fmodf(t * (0.18f + 0.22f * ForgeRand01(i * 5)) +
                     ForgeRand01(i * 5 + 1), 1.0f);
    float x = panel.x + panel.width * ForgeRand01(i * 5 + 2) +
              sinf(t * 2 + i) * 14 * s;
    float y = panel.y + panel.height * (1.02f - ph * 1.05f);
    float size = (1 + 2.2f * ForgeRand01(i * 5 + 3)) * s * (1 - ph);
    DrawCircleV((Vector2){x, y}, size, Fade(fire, 0.6f * (1 - ph)));
  }

  /* lueur de fond qui s'intensifie */
  float glow = revealed ? 0.35f * expf(-u * 1.2f) + 0.12f : 0.05f + 0.25f * k * k;
  for (int l = 0; l < 7; l++)
    DrawCircleV(center, radius * (0.35f + l * 0.32f), Fade(gold, glow * 0.22f));

  /* ---------- CHARGE : cercle runique, rayons, particules aspirées ---------- */
  float ringAlpha = revealed ? ForgeClamp01(1 - u / 0.9f) : ForgeClamp01(t / 0.5f);
  if (ringAlpha > 0) {
    float spin = t * (0.6f + 2.4f * k * k);   /* accélère */
    float pulse = revealed ? 1 : 1 - 0.06f * k * k * sinf(t * 18);
    ForgeDrawRuneRing(center, radius * 1.55f * pulse, 36, spin * 0.5f,
                      ringAlpha, s, gold);
    ForgeDrawRuneRing(center, radius * 1.15f * pulse, 24, -spin * 0.8f,
                      ringAlpha, s, rarityColor);
    ForgeDrawHexagram(center, radius * 1.05f * pulse, spin, ringAlpha * 0.9f, s,
                      gold);
    ForgeDrawHexagram(center, radius * 0.7f * pulse, -spin * 1.4f,
                      ringAlpha * 0.7f, s, fire);

    if (!revealed) {
      for (int i = 0; i < 12; i++) {          /* faisceaux vers le noyau */
        float a = spin * 0.3f + i * 2 * PI / 12;
        float flicker = 0.6f + 0.4f * sinf(t * 14 + i * 1.7f);
        DrawLineEx(ForgePolar(center, a, radius * 1.55f), center,
                   (1 + 2 * k) * s, Fade(gold, 0.45f * k * flicker));
      }
      for (int i = 0; i < 70; i++) {          /* particules en spirale */
        float ph = fmodf(t * 0.8f + ForgeRand01(i * 3), 1.0f);
        float dist = (1 - ph) * radius * 2.3f * (0.6f + ForgeRand01(i * 3 + 1));
        float a = ForgeRand01(i * 3 + 2) * 2 * PI + (1 - ph) * 5 + t * 0.4f;
        DrawCircleV(ForgePolar(center, a, dist), (1 + 2.5f * ph) * s,
                    Fade(RAYWHITE, ph * (0.3f + 0.7f * k)));
      }
      float coreR = radius * (0.12f + 0.38f * k * k) *
                    (1 + 0.1f * sinf(t * (8 + 24 * k)));
      for (int l = 0; l < 5; l++)             /* noyau incandescent */
        DrawCircleV(center, coreR * (1 + l * 0.55f), Fade(fire, 0.12f + 0.1f * k));
      DrawCircleV(center, coreR, Fade(WHITE, 0.9f));
    }
  }

  /* ---------- IMPACT : onde de choc + gerbe d'étincelles ---------- */
  if (revealed && u < 1.6f) {
    float fade = 1 - u / 1.6f;
    for (int w = 0; w < 3; w++) {             /* trois ondes décalées */
      float uw = u - w * 0.12f;
      if (uw <= 0)
        continue;
      float rr = radius * (0.5f + uw * 5.5f);
      DrawRing(center, rr - 5 * s, rr + 5 * s, 0, 360, 64,
               Fade(gold, fade * (1 - w * 0.25f)));
    }
    for (int i = 0; i < 110; i++) {           /* étincelles avec gravité */
      float a = ForgeRand01(i * 2) * 2 * PI;
      float speed = (180 + 520 * ForgeRand01(i * 2 + 1)) * s;
      float d = speed * u * (1.1f - 0.4f * u);
      Vector2 p = ForgePolar(center, a, d);
      p.y += 260 * s * u * u;
      DrawCircleV(p, (1.5f + 3 * ForgeRand01(i)) * s * fade,
                  Fade(i % 3 == 0 ? WHITE : gold, fade));
    }
  }

  /* ---------- RÉVÉLATION : rayons de gloire derrière l'objet ---------- */
  float appear = ForgeClamp01(u / 0.8f);
  if (revealed) {
    float rayAlpha = ForgeClamp01(u / 0.5f) * 0.5f;
    for (int i = 0; i < 18; i++) {
      float a = u * 0.5f + i * 2 * PI / 18;
      float len = radius * (1.9f + 0.5f * sinf(t * 3 + i * 2.1f));
      DrawLineEx(ForgePolar(center, a, radius * 0.5f),
                 ForgePolar(center, a, len), (i % 2 ? 4 : 9) * s,
                 Fade(gold, rayAlpha * (i % 2 ? 0.5f : 0.3f)));
    }
    for (int i = 0; i < 16; i++) {            /* étincelles en orbite */
      float a = t * (0.8f + 0.3f * ForgeRand01(i)) + i * 2 * PI / 16;
      float d = radius * (0.95f + 0.25f * sinf(t * 2 + i));
      DrawCircleV(ForgePolar(center, a, d),
                  (2 + 2 * ForgeRand01(i + 40)) * s * appear,
                  Fade(RAYWHITE, 0.8f * appear));
    }
  }
  EndBlendMode();

  /* objet forgé : apparition élastique + léger flottement */
  Rectangle rewardPreview = {0};
  if (revealed) {
    float scale = ForgeEaseOutBack(appear);
    float size = radius * 1.15f * scale;
    float bob = sinf(t * 2.5f) * 4 * s * appear;
    rewardPreview = (Rectangle){center.x - size / 2,
                                center.y - size / 2 + bob, size, size};
    ItemsDrawPreview(forgeRewardSlot, rewardPreview);
  }
  EndScissorMode();

  /* flash blanc plein écran, par-dessus tout */
  if (revealed && u < FANIM_FLASH)
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(WHITE, 1 - u / FANIM_FLASH));

  /* nom de l'objet */
  if (revealed && u > 0.5f)
    TextFit(targetName,
            (Rectangle){center.x - radius * 1.6f, center.y + radius * 1.1f,
                        radius * 3.2f, GameUiPixels(40)},
            26, Fade(rarityColor, ForgeClamp01((u - 0.5f) / 0.5f)));

  /* fin / passage */
  if (t < FANIM_DONE) {
    TextFit("Cliquez pour passer",
            (Rectangle){panel.x, panel.y + panel.height - GameUiPixels(50),
                        panel.width, GameUiPixels(28)},
            14, (Color){150, 150, 150, 255});
  } else if (Button((Rectangle){panel.x + panel.width * 0.36f,
                                panel.y + panel.height - GameUiPixels(68),
                                panel.width * 0.28f, GameUiPixels(44)},
                    "CONTINUER", true)) {
    ForgeClose();
    return;
  }
  if (revealed && u > 0.8f &&
      CheckCollisionPointRec(GetMousePosition(), rewardPreview))
    ItemsDrawTooltip(forgeRewardSlot, GetMousePosition());

  /* skip traité en dernier pour ne pas déclencher CONTINUER au même clic */
  if (t < FANIM_DONE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    animationTime = (t < FANIM_CHARGE + 0.5f) ? FANIM_CHARGE + 0.5f : FANIM_DONE;
}
/* =============== FIN ANIMATION DE FORGE =============== */

void ForgeClose(void) {
  active = false;
  selectedEffectCount = selectedItemCount = 0;
  forgeInventoryOffset = 0;
  forgeInventoryWheelRemainder = 0;
  targetName[0] = notice[0] = '\0';
  forgeRewardSlot = -1;
}


void ForgeOpen(void) {
  active = true;
  page = FORGE_FEE;
  knownOffset = effectOffset = 0;
  forgeInventoryOffset = 0;
  forgeInventoryWheelRemainder = 0;
  selectedEffectCount = selectedItemCount = 0;
  itemSortMode = effectSortMode = 0;
  targetName[0] = '\0';
  notice[0] = '\0';
  animationTime = 0;
  forgeRewardSlot = -1;
}

bool ForgeActive(void) { return active; }

void ForgeDraw(void) {
  if (!active)
    return;
  if (IsKeyPressed(KEY_ESCAPE) && page != FORGE_ANIMATION) {
    ForgeClose();
    return;
  }
  if (page == FORGE_ANIMATION)
    animationTime += GetFrameTime();

  Rectangle panel = Panel();
  switch (page) {
  case FORGE_FEE:
    DrawFee(panel);
    break;
  case FORGE_ITEM:
    DrawItemChoice(panel);
    break;
  case FORGE_EFFECTS:
    DrawEffectChoice(panel);
    break;
  case FORGE_CIRCLE:
    DrawForgeCirclePage(panel);
    break;
  case FORGE_ANIMATION:
    DrawForgeAnimation(panel);
    break;
  }
}