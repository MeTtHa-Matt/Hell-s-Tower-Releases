#include "shop.h"
#include "../items/items.h"
#include "../tower.h"
#include "../ui_scale.h"
#include "../buttons/button_sounds.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef enum { SHOP_PAGE, SHOP_REVEAL } ShopPage;

static bool active;
static ShopPage page;
static int packSize = 1, packSlots[5], packCount;
static char notice[160];
static float animationTime;

static float Scale(void) { return GameUiScale(); }

static void TextFit(const char *text, Rectangle box, float size, Color color) {
  int fontSize = (int)(size * Scale());
  int minSize = GameUiPixels(10);
  while (fontSize > minSize &&
         MeasureText(text, fontSize) > box.width - 12 * Scale())
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
  packCount = 0;
  notice[0] = '\0';
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

/* ===================== ANIMATION GACHA ===================== */
#define GACHA_CHARGE  1.8f   /* durée de la charge de l'orbe */
#define GACHA_FLASH   0.35f  /* durée du flash */
#define GACHA_STAGGER 0.8f   /* délai entre chaque carte retournée */
#define GACHA_FLIP    0.5f   /* durée du retournement d'une carte */

static float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

/* pseudo-aléatoire déterministe : pas besoin d'état supplémentaire */
static float Rand01(int n) {
  unsigned x = (unsigned)n * 747796405u + 2891336453u;
  x = ((x >> ((x >> 28) + 4)) ^ x) * 277803737u;
  x = (x >> 22) ^ x;
  return (x & 0xFFFF) / 65535.0f;
}

static void DrawGachaCharge(Vector2 c, float t, float base, float s) {
  float k = Clamp01(t / GACHA_CHARGE);
  float k2 = k * k;
  Color gold = {255, 200, 90, 255};
  c.x += sinf(t * 57) * k2 * 7 * s;   /* tremblement */
  c.y += cosf(t * 71) * k2 * 7 * s;

  BeginBlendMode(BLEND_ADDITIVE);
  for (int l = 0; l < 6; l++)
    DrawCircleV(c, base * (1 + l * 0.45f) * (0.6f + 0.4f * k),
                Fade(gold, 0.05f + 0.04f * k));
  for (int i = 0; i < 14; i++) {   /* rayons rotatifs */
    float a = t * 0.8f + i * 2 * PI / 14;
    float len = base * (1.2f + 2.2f * k2) * (0.7f + 0.3f * sinf(t * 10 + i));
    DrawLineEx(c, (Vector2){c.x + cosf(a) * len, c.y + sinf(a) * len},
               2 * s, Fade(gold, 0.35f * k));
  }
  for (int i = 0; i < 48; i++) {   /* particules aspirées vers l'orbe */
    float ph = fmodf(t * 0.9f + Rand01(i * 3), 1.0f);
    float dist = (1 - ph) * base * 5 * (0.5f + Rand01(i * 3 + 1));
    float a = Rand01(i * 3 + 2) * 2 * PI + t * 0.5f;
    DrawCircleV((Vector2){c.x + cosf(a) * dist, c.y + sinf(a) * dist},
                (1 + 2 * ph) * s, Fade(RAYWHITE, ph * k));
  }
  EndBlendMode();
  DrawCircleV(c, base * (0.55f + 0.35f * k), (Color){255, 226, 150, 255});
  DrawCircleV(c, base * (0.3f + 0.2f * k), WHITE);
}

static void DrawGachaBurst(Vector2 c, float u, float base, float s) {
  if (u < 0 || u > 1.2f)
    return;
  float fade = 1 - u / 1.2f;
  BeginBlendMode(BLEND_ADDITIVE);
  float ring = base * (1 + u * 9);   /* onde de choc */
  DrawRing(c, ring - 6 * s, ring + 6 * s, 0, 360, 48,
           Fade((Color){255, 220, 130, 255}, fade));
  for (int i = 0; i < 70; i++) {     /* gerbe de particules */
    float a = Rand01(i * 2) * 2 * PI;
    float speed = (150 + 400 * Rand01(i * 2 + 1)) * s;
    float d = speed * u * (1.2f - 0.5f * u);
    DrawCircleV((Vector2){c.x + cosf(a) * d, c.y + sinf(a) * d},
                (2 + 3 * Rand01(i)) * s * fade,
                Fade((Color){255, 215, 120, 255}, fade));
  }
  EndBlendMode();
}

static void DrawCardBack(Rectangle card, float s) {
  DrawRectangleRounded(card, 0.09f, 8, (Color){40, 28, 58, 255});
  DrawRectangleRoundedLinesEx(card, 0.09f, 8, s * 3, (Color){234, 194, 111, 255});
  Rectangle inner = {card.x + card.width * 0.1f, card.y + card.height * 0.06f,
                     card.width * 0.8f, card.height * 0.88f};
  if (inner.width > 4)
    DrawRectangleRoundedLinesEx(inner, 0.09f, 8, s, (Color){150, 120, 70, 255});
  if (card.width > GameUiPixels(40))
    TextFit("?", card, 64, (Color){234, 194, 111, 255});
}

static void DrawReveal(Rectangle panel) {
  DrawPanel(panel, "OUVERTURE DU PACK");
  float t = animationTime, s = Scale();
  float revealStart = GACHA_CHARGE + GACHA_FLASH + 0.4f;
  float done = revealStart + packCount * GACHA_STAGGER + 0.3f;
  Vector2 center = {panel.x + panel.width / 2, panel.y + panel.height * 0.5f};
  float base = fminf(panel.width, panel.height) * 0.06f;

  /* --- Phase 1 : charge --- */
  if (t < GACHA_CHARGE)
    DrawGachaCharge(center, t, base, s);

  /* --- Phase 3 : cartes --- */
  int hoverSlot = -1;
  if (t >= GACHA_CHARGE + GACHA_FLASH * 0.5f) {
    float cardW = fminf(panel.width * 0.16f, 175 * fmaxf(s, 1));
    float cardH = fminf(panel.height * 0.48f, 300 * fmaxf(s, 1));
    float gap = GameUiPixels(16);
    float total = packCount * cardW + (packCount - 1) * gap;
    float x0 = panel.x + (panel.width - total) / 2;
    float y = panel.y + panel.height * 0.25f;

    float drop = Clamp01((t - GACHA_CHARGE - GACHA_FLASH * 0.5f) / 0.4f);
    drop = 1 - (1 - drop) * (1 - drop);                 /* ease-out */
    float yOffset = (1 - drop) * -panel.height * 0.15f;

    for (int i = 0; i < packCount; i++) {
      float f = Clamp01((t - (revealStart + i * GACHA_STAGGER)) / GACHA_FLIP);
      float sx = fabsf(cosf(f * PI));                   /* flip horizontal */
      float cx = x0 + i * (cardW + gap) + cardW / 2;
      Rectangle card = {cx - cardW * sx / 2, y + yOffset, cardW * sx, cardH};

      if (f < 0.5f) {
        DrawCardBack(card, s);
      } else {
        DrawRectangleRounded(card, 0.09f, 8, (Color){65, 53, 40, 255});
        DrawRectangleRoundedLinesEx(card, 0.09f, 8, s * 3,
                                    (Color){234, 194, 111, 255});
        Rectangle preview = {card.x + card.width * 0.12f,
                             card.y + card.height * 0.13f,
                             card.width * 0.76f, card.height * 0.58f};
        ItemsDrawPreview(packSlots[i], preview);
        if (sx > 0.85f)
          TextFit(ItemsItemName(packSlots[i]),
                  (Rectangle){card.x + 3, card.y + card.height * 0.75f,
                              card.width - 6, GameUiPixels(30)},
                  17, (Color){241, 213, 147, 255});
        if (f >= 1 && CheckCollisionPointRec(GetMousePosition(), card))
          hoverSlot = packSlots[i];
      }

      /* halo + éclat au moment où la carte se révèle */
      float since = t - (revealStart + i * GACHA_STAGGER + GACHA_FLIP * 0.5f);
      if (since > 0 && since < 0.7f) {
        float a = 1 - since / 0.7f;
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRectangleRounded((Rectangle){cx - cardW / 2 - since * 40 * s,
                                         y - since * 40 * s,
                                         cardW + since * 80 * s,
                                         cardH + since * 80 * s},
                             0.09f, 8, Fade((Color){255, 210, 110, 255}, a * 0.35f));
        EndBlendMode();
      }
    }
  }

  /* --- Phase 2 : explosion + flash (dessinés par-dessus) --- */
  float u = t - GACHA_CHARGE;
  DrawGachaBurst(center, u, base, s);
  if (u >= 0 && u < GACHA_FLASH)
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(WHITE, 1 - u / GACHA_FLASH));

  /* --- Fin / skip --- */
  if (t < done) {
    TextFit("Cliquez pour passer",
            (Rectangle){panel.x, panel.y + panel.height - GameUiPixels(50),
                        panel.width, GameUiPixels(28)},
            14, (Color){150, 150, 150, 255});
  } else if (Button((Rectangle){panel.x + panel.width * 0.36f,
                                panel.y + panel.height - GameUiPixels(68),
                                panel.width * 0.28f, GameUiPixels(44)},
                    "CONTINUER", true)) {
    page = SHOP_PAGE;
  }
  if (hoverSlot >= 0)
    ItemsDrawTooltip(hoverSlot, GetMousePosition());

  /* le skip est traité en dernier pour ne pas déclencher CONTINUER au même clic */
  if (t < done && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    animationTime = (t < GACHA_CHARGE + GACHA_FLASH) ? GACHA_CHARGE + 0.01f : done;
}
/* =================== FIN ANIMATION GACHA =================== */

void ShopOpen(void) {
  active = true;
  page = SHOP_PAGE;
  packSize = 1;
  notice[0] = '\0';
}

bool ShopActive(void) { return active; }

void ShopDraw(void) {
  if (!active)
    return;
  if (IsKeyPressed(KEY_ESCAPE) && page != SHOP_REVEAL) {
    Close();
    return;
  }
  if (page == SHOP_REVEAL)
    animationTime += GetFrameTime();
  Rectangle panel = Panel();
  switch (page) {
  case SHOP_PAGE:
    DrawShop(panel);
    break;
  case SHOP_REVEAL:
    DrawReveal(panel);
    break;
  }
}
