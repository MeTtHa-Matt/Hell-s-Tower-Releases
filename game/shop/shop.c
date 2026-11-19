#include "shop.h"
#include "../items/items.h"
#include "../tower.h"
#include "../ui_scale.h"
#include "../buttons/button_sounds.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FORGE_ENTRY_FEE 100
#define MAX_FORGE_EFFECTS 5
#define MAX_FORGE_ITEMS 6

typedef enum {
  SHOP_PAGE,
  SHOP_REVEAL,
  FORGE_FEE,
  FORGE_ITEM,
  FORGE_EFFECTS,
  FORGE_CIRCLE,
  FORGE_ANIMATION
} ShopPage;

static bool active;
static ShopPage page;
static int packSize = 1, packSlots[5], packCount;
static int knownOffset, effectOffset, selectedEffects[MAX_FORGE_EFFECTS];
static int selectedEffectCount, selectedItems[MAX_FORGE_ITEMS], selectedItemCount;
static int targetEffectCount, targetEffectPrice, requiredItems, contribution;
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
  DrawText(TextFormat("%d ETRIS", TowerEtris()),
           (int)(panel.x + panel.width - GameUiPixels(150)),
           (int)(panel.y + GameUiPixels(28)), GameUiPixels(18),
           (Color){239, 209, 119, 255});
}

static void DrawNotice(Rectangle panel) {
  if (notice[0])
    TextFit(notice, (Rectangle){panel.x + 12 * Scale(),
                                panel.y + panel.height - GameUiPixels(86),
                                panel.width - 24 * Scale(), GameUiPixels(32)},
            16, (Color){244, 133, 112, 255});
}

static void Close(void) {
  active = false;
  packCount = selectedEffectCount = selectedItemCount = 0;
  targetName[0] = notice[0] = '\0';
}

static void BeginReveal(void) {
  if (!TowerSpendEtris(packSize * 100 + 50 * packSize * (packSize - 1))) {
    snprintf(notice, sizeof(notice), "Vous n'avez pas assez d'Etris.");
    return;
  }
  if (!ItemsGrantShopPack(packSize, packSlots)) {
    TowerAddEtris(packSize * 100 + 50 * packSize * (packSize - 1));
    TraceLog(LOG_ERROR, "Impossible de creer les objets du pack");
    snprintf(notice, sizeof(notice), "Impossible d'ouvrir ce pack.");
    return;
  }
  packCount = packSize;
  animationTime = 0;
  notice[0] = '\0';
  page = SHOP_REVEAL;
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
  contribution = 0;
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

static void DrawShop(Rectangle panel) {
  DrawPanel(panel, "BOUTIQUE");
  float scale = Scale();
  float cardW = fminf(panel.width * 0.25f, 220 * fmaxf(scale, 1));
  float cardH = fminf(panel.height * 0.42f, 250 * fmaxf(scale, 1));
  float gap = GameUiPixels(24);
  float rowW = cardW * 3 + gap * 2;
  float x = panel.x + (panel.width - rowW) / 2;
  float y = panel.y + panel.height * 0.24f;
  for (int i = 0; i < 3; i++) {
    Rectangle card = {x + i * (cardW + gap), y, cardW, cardH};
    DrawRectangleRounded(card, 0.08f, 8, (Color){28, 29, 34, 255});
    DrawRectangleRoundedLinesEx(card, 0.08f, 8, scale * 2,
                                (Color){146, 121, 76, 255});
    TextFit(TextFormat("%d OBJET%s", i + 1, i ? "S" : ""),
            (Rectangle){card.x + 4, card.y + card.height * 0.15f,
                        card.width - 8, GameUiPixels(34)},
            23, (Color){241, 213, 147, 255});
    TextFit(TextFormat("%d ETRIS", (i + 1) * 100 + 50 * i * (i + 1)),
            (Rectangle){card.x + 4, card.y + card.height * 0.38f,
                        card.width - 8, GameUiPixels(32)},
            18, RAYWHITE);
    if (Button((Rectangle){card.x + card.width * 0.12f,
                           card.y + card.height * 0.68f, card.width * 0.76f,
                           card.height * 0.2f},
               "ACHETER", TowerEtris() >= (i + 1) * 100 + 50 * i * (i + 1) &&
                              ItemsCanStore(i + 1))) {
      packSize = i + 1;
      BeginReveal();
    }
    if (TowerEtris() < (i + 1) * 100 + 50 * i * (i + 1))
      TextFit("ETRIS INSUFFISANTS",
              (Rectangle){card.x + 5, card.y + card.height * 0.9f,
                          card.width - 10, GameUiPixels(24)},
              12, (Color){224, 116, 100, 255});
    else if (!ItemsCanStore(i + 1))
      TextFit("SAC INSUFFISAMMENT LIBRE",
              (Rectangle){card.x + 5, card.y + card.height * 0.9f,
                          card.width - 10, GameUiPixels(24)},
              12, (Color){224, 116, 100, 255});
  }
  DrawNotice(panel);
  if (Button((Rectangle){panel.x + panel.width * 0.36f,
                         panel.y + panel.height - GameUiPixels(64),
                         panel.width * 0.28f, GameUiPixels(42)},
             "FERMER", true))
    Close();
}

static void DrawReveal(Rectangle panel) {
  DrawPanel(panel, "OUVERTURE DU PACK");
  float elapsed = animationTime;
  int visible = 1 + (int)(elapsed / 0.7f);
  if (visible > packCount)
    visible = packCount;
  float cardW = fminf(panel.width * 0.16f, 175 * fmaxf(Scale(), 1));
  float cardH = fminf(panel.height * 0.48f, 300 * fmaxf(Scale(), 1));
  float gap = GameUiPixels(16);
  float total = packCount * cardW + (packCount - 1) * gap;
  float x = panel.x + (panel.width - total) / 2;
  float y = panel.y + panel.height * 0.25f;
  for (int i = 0; i < visible; i++) {
    Rectangle card = {x + i * (cardW + gap), y, cardW, cardH};
    float pulse = 1 + 0.025f * sinf(animationTime * 8 + i);
    Rectangle glow = {card.x - cardW * (pulse - 1),
                      card.y - cardH * (pulse - 1),
                      card.width * pulse, card.height * pulse};
    DrawRectangleRounded(glow, 0.09f, 8,
                         (Color){65, 53, 40, 255});
    DrawRectangleRoundedLinesEx(glow, 0.09f, 8, Scale() * 3,
                                (Color){234, 194, 111, 255});
    ItemsDrawPreview(packSlots[i],
                     (Rectangle){card.x + card.width * 0.12f,
                                 card.y + card.height * 0.13f,
                                 card.width * 0.76f, card.height * 0.58f});
    TextFit(ItemsItemName(packSlots[i]),
            (Rectangle){card.x + 3, card.y + card.height * 0.75f,
                        card.width - 6, GameUiPixels(30)},
            17, (Color){241, 213, 147, 255});
  }
  if (visible == packCount && elapsed > packCount * 0.7f + 0.5f &&
      Button((Rectangle){panel.x + panel.width * 0.36f,
                         panel.y + panel.height - GameUiPixels(68),
                         panel.width * 0.28f, GameUiPixels(44)},
             "CONTINUER", true))
    page = SHOP_PAGE;
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
    Close();
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
  int rows = 7;
  if (count == 0)
    TextFit("Vous n'avez encore obtenu aucun artefact.",
            (Rectangle){panel.x + 20, panel.y + panel.height * 0.42f,
                        panel.width - 40, GameUiPixels(42)},
            20, RAYWHITE);
  int rowHeight = (int)fminf(GameUiPixels(50), panel.height * 0.082f);
  int startY = (int)(panel.y + panel.height * 0.2f);
  for (int i = 0; i < rows && knownOffset + i < count; i++) {
    Rectangle row = {panel.x + panel.width * 0.16f, startY + i * rowHeight,
                     panel.width * 0.68f, rowHeight - GameUiPixels(5)};
    if (Button(row, ItemsKnownItem(knownOffset + i), true))
      SelectTarget(ItemsKnownItem(knownOffset + i));
  }
  if (count > rows) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(),
          (Rectangle){panel.x, startY, panel.width, rows * rowHeight}))
      knownOffset = (int)fmaxf(0, fminf(count - rows, knownOffset - wheel));
    if (IsKeyPressed(KEY_DOWN) && knownOffset < count - rows)
      knownOffset++;
    if (IsKeyPressed(KEY_UP) && knownOffset > 0)
      knownOffset--;
  }
  DrawNotice(panel);
  if (Button((Rectangle){panel.x + panel.width * 0.36f,
                         panel.y + panel.height - GameUiPixels(64),
                         panel.width * 0.28f, GameUiPixels(42)},
             "ANNULER", true))
    Close();
}

static bool EffectSelected(int effect) {
  for (int i = 0; i < selectedEffectCount; i++)
    if (selectedEffects[i] == effect)
      return true;
  return false;
}

static void ToggleEffect(int effect) {
  for (int i = 0; i < selectedEffectCount; i++)
    if (selectedEffects[i] == effect) {
      for (int j = i + 1; j < selectedEffectCount; j++)
        selectedEffects[j - 1] = selectedEffects[j];
      selectedEffectCount--;
      return;
    }
  if (selectedEffectCount < MAX_FORGE_EFFECTS)
    selectedEffects[selectedEffectCount++] = effect;
}

static void DrawEffectChoice(Rectangle panel) {
  DrawPanel(panel, TextFormat("EFFETS POUR %s", targetName));
  int count = ItemsKnownEffectCount(), rows = 7;
  int rowHeight = (int)fminf(GameUiPixels(54), panel.height * 0.082f);
  int startY = (int)(panel.y + panel.height * 0.2f);
  for (int i = 0; i < rows && effectOffset + i < count; i++) {
    int effect = ItemsKnownEffect(effectOffset + i);
    Rectangle row = {panel.x + panel.width * 0.1f, startY + i * rowHeight,
                     panel.width * 0.8f, rowHeight - GameUiPixels(4)};
    bool selected = EffectSelected(effect);
    Color fill = selected ? (Color){78, 60, 33, 255}
                          : (Color){36, 37, 42, 255};
    DrawRectangleRounded(row, 0.08f, 6, fill);
    DrawRectangleRoundedLinesEx(row, 0.08f, 6, Scale(),
                                (Color){145, 124, 84, 255});
    const char *label = TextFormat("%s  |  %s%s",
                                   ItemsEffectRarity(effect),
                                   ItemsEffectText(effect),
                                   selected ? "  [CHOISI]" : "");
    TextFit(label, (Rectangle){row.x + 6, row.y, row.width - 12, row.height},
            15, RAYWHITE);
    if (CheckCollisionPointRec(GetMousePosition(), row) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      ToggleEffect(effect);
  }
  if (count > rows) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(),
          (Rectangle){panel.x, startY, panel.width, rows * rowHeight}))
      effectOffset = (int)fmaxf(0, fminf(count - rows, effectOffset - wheel));
    if (IsKeyPressed(KEY_DOWN) && effectOffset < count - rows)
      effectOffset++;
    if (IsKeyPressed(KEY_UP) && effectOffset > 0)
      effectOffset--;
  }
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

static void DrawForgeInventory(Rectangle panel) {
  float gap = GameUiPixels(8);
  float size = fminf(GameUiPixels(58),
                     (panel.width * 0.46f - 6 * gap) / 5);
  float x = panel.x + GameUiPixels(24);
  float y = panel.y + GameUiPixels(106);
  int hoverSlot = -1;
  for (int i = 0; i < 30; i++) {
    Rectangle slot = {x + (i % 5) * (size + gap),
                      y + (i / 5) * (size + gap), size, size};
    bool occupied = ItemsItemName(i)[0] != '\0';
    bool selected = HasSelectedItem(i);
    DrawRectangleRounded(slot, 0.08f, 4,
                         selected ? (Color){79, 63, 37, 255}
                                  : (Color){31, 37, 41, 255});
    DrawRectangleRoundedLinesEx(slot, 0.08f, 4, Scale(),
                                selected ? (Color){255, 218, 120, 255}
                                         : (Color){110, 117, 118, 255});
    if (occupied)
      ItemsDrawPreview(i, slot);
    if (occupied && CheckCollisionPointRec(GetMousePosition(), slot))
      hoverSlot = i;
  }
  if (hoverSlot >= 0) {
    TextFit(TextFormat("%s  |  CLIQUEZ POUR %s",
                       ItemsItemName(hoverSlot),
                       HasSelectedItem(hoverSlot) ? "RETIRER" : "AJOUTER"),
            (Rectangle){x, y + 6 * (size + gap), panel.width * 0.46f,
                        GameUiPixels(28)},
            14, (Color){239, 216, 159, 255});
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      ToggleSacrifice(hoverSlot);
  }
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
  const char *priceLabel = TextFormat("%d / %d", contribution, price);
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

  Rectangle slider = {center.x - radius * 0.52f,
                      center.y + radius * 0.2f, radius * 1.04f,
                      GameUiPixels(14)};
  DrawRectangleRounded(slider, 0.5f, 6, (Color){61, 58, 52, 255});
  int maximum = TowerEtris() < price ? TowerEtris() : price;
  float progress = maximum > 0 ? (float)contribution / maximum : 0;
  DrawRectangleRounded((Rectangle){slider.x, slider.y, slider.width * progress,
                                   slider.height},
                       0.5f, 6, (Color){220, 180, 91, 255});
  if (CheckCollisionPointRec(GetMousePosition(),
                             (Rectangle){slider.x, slider.y - GameUiPixels(15),
                                         slider.width, slider.height * 3})) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && maximum > 0) {
      float t = (GetMouseX() - slider.x) / slider.width;
      contribution = (int)(fmaxf(0, fminf(1, t)) * maximum);
    }
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
      int step = maximum / 100;
      if (step < 1)
        step = 1;
      contribution += wheel > 0 ? step : -step;
      if (contribution < 0)
        contribution = 0;
      if (contribution > maximum)
        contribution = maximum;
    }
  }
  const char *slotsText = TextFormat("ARTEFACTS : %d / %d",
                                     selectedItemCount, requiredItems);
  TextFit(slotsText,
          (Rectangle){center.x - radius, center.y + radius * 0.55f,
                      radius * 2, GameUiPixels(27)},
          15, RAYWHITE);

  for (int i = 0; i < requiredItems; i++) {
    float angle = -PI / 2 + 2 * PI * i / requiredItems;
    Vector2 node = {center.x + cosf(angle) * radius * 1.35f,
                    center.y + sinf(angle) * radius * 1.35f};
    float nodeRadius = fminf(GameUiPixels(28), radius * 0.21f);
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
      if (CheckCollisionPointCircle(GetMousePosition(), node, nodeRadius) &&
          IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        ToggleSacrifice(itemSlot);
    }
  }
}

static void CommitForge(void) {
  if (selectedItemCount != requiredItems || contribution < ForgePrice() ||
      TowerEtris() < ForgePrice() ||
      !ItemsForgeCanComplete(targetName, selectedEffects,
                             targetEffectCount, selectedItems,
                             selectedItemCount))
    return;
  if (!TowerSpendEtris(ForgePrice()))
    return;
  if (!ItemsForgeComplete(targetName, selectedEffects, targetEffectCount,
                          selectedItems, selectedItemCount)) {
    TowerAddEtris(ForgePrice());
    TraceLog(LOG_ERROR, "La forge n'a pas pu creer l'artefact");
    snprintf(notice, sizeof(notice), "La forge a echoue, Etris restaures.");
    return;
  }
  forgeRewardSlot = ItemsRewardSlot();
  animationTime = 0;
  page = FORGE_ANIMATION;
}

static void DrawForgeCirclePage(Rectangle panel) {
  DrawPanel(panel, "CERCLE DE FORGE");
  DrawForgeInventory(panel);
  DrawForgeCircle(panel);
  if (Button((Rectangle){panel.x + panel.width * 0.55f,
                         panel.y + panel.height - GameUiPixels(62),
                         panel.width * 0.22f, GameUiPixels(42)},
             "ANNULER", true))
    page = SHOP_PAGE;
  bool ready = selectedItemCount == requiredItems &&
               contribution >= ForgePrice() && TowerEtris() >= ForgePrice();
  if (Button((Rectangle){panel.x + panel.width * 0.79f,
                         panel.y + panel.height - GameUiPixels(62),
                         panel.width * 0.17f, GameUiPixels(42)},
             "FORGER", ready))
    CommitForge();
  DrawNotice(panel);
}

static void DrawForgeAnimation(Rectangle panel) {
  DrawPanel(panel, "LA FORGE S'ANIME");
  Vector2 center = {panel.x + panel.width / 2, panel.y + panel.height * 0.55f};
  float radius = fminf(panel.width, panel.height) * 0.19f;
  for (int i = 0; i < 12; i++) {
    float angle = animationTime * 2 + i * 2 * PI / 12;
    float pulse = 0.65f + 0.35f * sinf(animationTime * 8 + i);
    DrawCircleV((Vector2){center.x + cosf(angle) * radius,
                          center.y + sinf(angle) * radius},
                GameUiPixels(5) * pulse, (Color){241, 188, 94, 255});
  }
  if (animationTime < 2.2f)
    return;
  ItemsDrawPreview(forgeRewardSlot,
                   (Rectangle){center.x - radius * 0.55f,
                               center.y - radius * 0.55f,
                               radius * 1.1f, radius * 1.1f});
  TextFit(targetName, (Rectangle){center.x - radius, center.y + radius * 0.7f,
                                  radius * 2, GameUiPixels(34)},
          22, (Color){245, 220, 130, 255});
  if (Button((Rectangle){panel.x + panel.width * 0.36f,
                         panel.y + panel.height - GameUiPixels(68),
                         panel.width * 0.28f, GameUiPixels(44)},
             "CONTINUER", true)) {
    page = SHOP_PAGE;
  }
}

void ShopOpen(void) {
  active = true;
  page = SHOP_PAGE;
  packSize = 1;
  notice[0] = '\0';
}

void ShopOpenForge(void) {
  active = true;
  page = FORGE_FEE;
  notice[0] = '\0';
}

bool ShopActive(void) { return active; }

void ShopDraw(void) {
  if (!active)
    return;
  if (IsKeyPressed(KEY_ESCAPE) && page != SHOP_REVEAL &&
      page != FORGE_ANIMATION) {
    Close();
    return;
  }
  if (page == SHOP_REVEAL || page == FORGE_ANIMATION)
    animationTime += GetFrameTime();
  Rectangle panel = Panel();
  switch (page) {
  case SHOP_PAGE:
    DrawShop(panel);
    break;
  case SHOP_REVEAL:
    DrawReveal(panel);
    break;
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
