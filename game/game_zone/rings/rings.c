#include "rings.h"
#include "../balance.h"
#include "../modifiers.h"
#include "../../items/functionnal/functionnal.h"
#include "../../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define MAX_RINGS 64
#define DURATION 30.0f
#define THICK 6

typedef struct {
  float radius;
  float gapAngle;
  float gapWidth;
  float speed;
  bool on;
  bool hit;
} Ring;

static Ring rings[MAX_RINGS];
static Texture2D heartTex;
static Vector2 player;
static bool playerReady;
static RingsStatus state;
static int floorNumber, difficulty, lives, maxLives, damageCount, ringSpawnCount;
static int ringCount;
static float timeLeft, spawnTimer, lastGap, invincible, idleTimer, ringSpeed;
static float dashTime, dashCooldown;
static bool hazardsCleared;
static bool modifiers[MINIGAME_MODIFIER_OPTIONS];
static const char *modifierNames[MINIGAME_MODIFIER_OPTIONS] = {
    "MOUVEMENT LENT", "OUVERTURES ETROITES", "ANNEAUX RAPIDES",
    "VAGUES PLUS DENSES"};

static float Limit(float value, float min, float max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

static int RingCount(void) {
  if (difficulty <= 3)
    return GetRandomValue(6, 7);
  return BalanceRingCountMinimum(difficulty);
}

static float Speed(float k) {
  const FunctionalSettings *items = FunctionalGet();
  float base = BalanceRingSpeed(difficulty);
  float speedMin = BalanceRandomSpeedMin(floorNumber, difficulty, base);
  float speedMax = BalanceRandomSpeedMax(floorNumber, difficulty, base);
  float speed = GetRandomValue((int)(speedMin * 100),
                               (int)(speedMax * 100)) / 100.0f;
  return speed * k * items->ringSpeed * items->bulletSpeed *
         (modifiers[2] ? 1.15f : 1.0f);
}

static float RingSpacing(float k) {
  float spacing = BalanceRingSpacing(difficulty);
  float collisionClearance = (THICK + 16) * FunctionalGet()->hitbox + 2;
  if (spacing < collisionClearance)
    spacing = collisionClearance;
  return spacing * k;
}

static float OuterRadius(Rectangle a, float k) {
  return hypotf(a.width / 2, a.height / 2) + THICK * k;
}

static Vector2 Center(Rectangle a) {
  return (Vector2){a.x + a.width / 2, a.y + a.height / 2};
}

static void DrawHeart(Vector2 c, float size) {
  Rectangle source = {0, 0, (float)heartTex.width, (float)heartTex.height};
  Rectangle dest = {c.x - size / 2, c.y - size / 2, size, size};
  DrawTexturePro(heartTex, source, dest, (Vector2){0, 0}, 0, WHITE);
}

static void DrawCentered(Rectangle a, const char *text, int size, float y, Color color) {
  int fontSize = GameUiPixels(size);
  int x = (int)(a.x + (a.width - MeasureText(text, fontSize)) / 2);
  DrawText(text, x, (int)y, fontSize, color);
}

static int SpawnWave(float startRadius, float k, int count) {
  int spawned = 0;
  int widthOrder[MAX_RINGS];
  float gapWidths[MAX_RINGS];
  float gapAngles[MAX_RINGS];
  float gapMin = BalanceRingGapRandomMin(floorNumber, difficulty);
  float gapMax = BalanceRingGapRandomMax(floorNumber, difficulty);

  for (int i = 0; i < count; i++) {
    widthOrder[i] = i;
  }
  for (int i = count - 1; i > 0; i--) {
    int other = GetRandomValue(0, i);
    int swap = widthOrder[i];
    widthOrder[i] = widthOrder[other];
    widthOrder[other] = swap;
  }
  // Stratify widths so a wave cannot accidentally get identical openings.
  for (int i = 0; i < count; i++) {
    float widthFraction =
        (widthOrder[i] + GetRandomValue(250, 750) / 1000.0f) / count;
    gapWidths[i] = (gapMin + (gapMax - gapMin) * widthFraction) * DEG2RAD;
  }

  int direction = GetRandomValue(0, 1) == 0 ? -1 : 1;
  gapAngles[count - 1] =
      remainderf(lastGap + GetRandomValue(45, 180) * DEG2RAD, 2 * PI);
  for (int i = count - 2; i >= 0; i--) {
    int shift = direction * GetRandomValue(25, 55);
    gapAngles[i] = remainderf(gapAngles[i + 1] + shift * DEG2RAD, 2 * PI);
  }

  if (ringSpeed <= 0)
    ringSpeed = Speed(k);

  for (int i = 0; i < MAX_RINGS && spawned < count; i++) {
    if (!rings[i].on) {
      int sequence = spawned++;
      ringSpawnCount++;
      if (FunctionalGet()->removeRings &&
          ringSpawnCount % FunctionalGet()->ringSkipInterval == 0) {
        continue;
      }
      rings[i].radius = startRadius + (count - 1 - sequence) * RingSpacing(k);
      rings[i].gapAngle = gapAngles[sequence];
      rings[i].gapWidth = gapWidths[sequence] * (modifiers[1] ? 0.8f : 1.0f);
      rings[i].speed = ringSpeed;
      rings[i].on = true;
      rings[i].hit = false;
    }
  }
  if (spawned > 0) {
    lastGap = gapAngles[0];
  }
  return spawned;
}

void RingsStart(int floorNumberArg, int d) {
  if (heartTex.id == 0)
    heartTex = LoadTexture("assets/heart.png");

  floorNumber = floorNumberArg;
  difficulty = d;
  if (difficulty < 1)
    difficulty = 1;
  if (difficulty > 10)
    difficulty = 10;

  GameZonePickModifiers(difficulty, MINIGAME_MODIFIER_OPTIONS, modifiers,
                        modifierNames);
  ringCount = RingCount() + (modifiers[3] ? 1 : 0);
  const FunctionalSettings *items = FunctionalGet();
  lives = maxLives = 3 + items->extraLives;
  if (lives < 1)
    lives = maxLives = 1;
  damageCount = 0;
  state = RINGS_PLAYING;
  float durationMin =
      BalanceMiniGameDurationMin(floorNumber, difficulty, DURATION);
  float durationMax =
      BalanceMiniGameDurationMax(floorNumber, difficulty, DURATION);
  timeLeft = GetRandomValue((int)(durationMin * 100),
                            (int)(durationMax * 100)) / 100.0f +
             items->duration;
  spawnTimer = 0;
  ringSpeed = 0;
  lastGap = GetRandomValue(0, 359) * DEG2RAD;
  ringSpawnCount = 0;
  invincible = idleTimer = dashTime = dashCooldown = 0;
  hazardsCleared = false;
  playerReady = false;
  memset(rings, 0, sizeof(rings));
}

static void Update(Rectangle a, float k) {
  float dt = GetFrameTime();
  float margin = 15 * k;
  const FunctionalSettings *items = FunctionalGet();
  Vector2 c = Center(a);
  Vector2 mouse = GetMousePosition();

  Vector2 target = {Limit(mouse.x, a.x + margin, a.x + a.width - margin),
                    Limit(mouse.y, a.y + margin, a.y + a.height - margin)};
  float dx = target.x - player.x, dy = target.y - player.y;
  float distance = hypotf(dx, dy);
  float moveScale = modifiers[0] ? 0.8f : 1.0f;
  float step = 900 * k * items->playerSpeed * moveScale * dt;
  if (distance > step && distance > 0) {
    dx *= step / distance;
    dy *= step / distance;
  }
  player.x += dx;
  player.y += dy;
  player.x = Limit(player.x, a.x + margin, a.x + a.width - margin);
  player.y = Limit(player.y, a.y + margin, a.y + a.height - margin);

  if (items->ringDash && IsKeyPressed(KEY_SPACE) && dashCooldown <= 0) {
    dashTime = 0.35f;
    dashCooldown = items->ringDashCooldown;
  }
  dashTime -= dt;
  dashCooldown -= dt;
  invincible -= dt;

  timeLeft -= dt;
  spawnTimer -= dt;
  if (items->clearBullets && !hazardsCleared &&
      timeLeft <= items->clearBulletsAt) {
    memset(rings, 0, sizeof(rings));
    hazardsCleared = true;
  }
  if (GetMouseDelta().x != 0 || GetMouseDelta().y != 0 ||
      IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_LEFT_SHIFT))
    idleTimer = 0;
  else
    idleTimer += dt;
  if (items->regeneration > 0 && idleTimer >= items->regeneration &&
      lives < maxLives) {
    lives++;
    idleTimer = 0;
  }
  if (spawnTimer <= 0) {
    int count = ringCount;
    int spawned = SpawnWave(OuterRadius(a, k), k, count);
    if (spawned > 0) {
      spawnTimer = count * RingSpacing(k) / ringSpeed;
    } else {
      spawnTimer = 0.1f;
    }
  }

  float dist = hypotf(player.x - c.x, player.y - c.y);
  float angle = atan2f(player.y - c.y, player.x - c.x);

  for (int i = 0; i < MAX_RINGS; i++) {
    Ring *r = &rings[i];
    if (!r->on)
      continue;

    r->radius -= r->speed * dt;
    if (r->radius <= 0) {
      r->on = false;
      continue;
    }

    bool touchesRing = fabsf(dist - r->radius) <
                       (THICK / 2 + 8) * k * items->hitbox;
    float opening = r->gapWidth *
                    FunctionalGet()->ringGap;
    bool outsideGap =
        fabsf(remainderf(angle - r->gapAngle, 2 * PI)) > opening / 2;

    if (!r->hit && touchesRing && outsideGap && dashTime <= 0 &&
        invincible <= 0) {
      r->hit = true;
      if (!FunctionalBlockDamageAtLives(lives)) {
        invincible = 1.5f + items->invincibility;
        damageCount++;
        lives--;
        if (lives <= 0)
          state = RINGS_LOST;
      }
    }
  }

  if (timeLeft <= 0 && state == RINGS_PLAYING)
    state = RINGS_WON;
}

static void Draw(Rectangle a, float k) {
  Vector2 c = Center(a);
  float ui = GameUiScale();
  int hudSize = GameUiPixels(20);

  BeginScissorMode((int)a.x, (int)a.y, (int)a.width, (int)a.height);
  for (int i = 0; i < MAX_RINGS; i++) {
    Ring *r = &rings[i];
    if (!r->on)
      continue;

    float inner = r->radius - THICK * k / 2;
    if (inner < 0)
      inner = 0;
    float outer = r->radius + THICK * k / 2;
    float middle = r->gapAngle * RAD2DEG;
    float halfGap = r->gapWidth *
                    FunctionalGet()->ringGap * RAD2DEG / 2;
    Color color = WHITE;
    if (r->hit)
      color = GRAY;

    DrawRing(c, inner, outer, middle + halfGap, middle + 360 - halfGap, 0, color);
  }
  DrawHeart(player, 30 * k);
  EndScissorMode();

  for (int i = 0; i < lives; i++)
    DrawHeart((Vector2){a.x + (16 + i * 28) * ui, a.y + 16 * ui}, 20 * ui);
  if (FunctionalGet()->ringDash)
    DrawText("ESPACE : ESQUIVE", (int)a.x + GameUiPixels(10),
             (int)(a.y + a.height - GameUiPixels(24)), GameUiPixels(16),
             RAYWHITE);

  float shownTime = timeLeft;
  if (shownTime < 0)
    shownTime = 0;
  const char *timer = TextFormat("%.0f", shownTime);
  DrawText(timer, (int)(a.x + a.width) - MeasureText(timer, hudSize) -
                      GameUiPixels(10),
           (int)a.y + GameUiPixels(6), hudSize, WHITE);

  if (state != RINGS_PLAYING) {
    const char *title = "GAME OVER";
    if (state == RINGS_WON)
      title = "VICTOIRE";
    DrawCentered(a, title, 40,
                 a.y + a.height / 2 - GameUiPixels(20), WHITE);
    DrawCentered(a, "ENTREE pour continuer", 20,
                 a.y + a.height / 2 + GameUiPixels(20), GRAY);
  }
}

void RingsUpdate(Rectangle a) {
  if (state == RINGS_OFF)
    return;

  float smallSide = a.width;
  if (a.height < smallSide)
    smallSide = a.height;
  float k = smallSide / 400;

  if (!playerReady) {
    player = Center(a);
    playerReady = true;
  }

  if (state == RINGS_PLAYING)
    Update(a, k);
  if (state != RINGS_PLAYING && state != RINGS_OFF && IsKeyPressed(KEY_ENTER))
    state = RINGS_OFF;

  Draw(a, k);
}

RingsStatus RingsState(void) {
  return state;
}

int RingsDamageCount(void) {
  return damageCount;
}

void RingsUnload(void) {
  UnloadTexture(heartTex);
  heartTex = (Texture2D){0};
  playerReady = false;
}