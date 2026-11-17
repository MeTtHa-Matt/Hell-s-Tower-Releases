#include "rings.h"
#include "../../items/functionnal/functionnal.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define MAX_RINGS 64
#define DURATION 30.0f
#define THICK 6

typedef struct {
  float radius;
  float gapAngle;
  bool on;
  bool hit;
} Ring;

static Ring rings[MAX_RINGS];
static Texture2D heartTex;
static Vector2 player;
static bool playerReady;
static RingsStatus state;
static int difficulty, lives, maxLives, damageCount, ringSpawnCount;
static float timeLeft, spawnTimer, lastGap, invincible, idleTimer;
static float dashTime, dashCooldown;
static bool hazardsCleared;

static float Limit(float value, float min, float max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

static float GapWidth(void) {
  return (90 - 55 * (difficulty - 1) / 9.0f) *
         FunctionalGet()->ringGap * DEG2RAD;
}

static int RingCount(void) {
  if (difficulty <= 3)
    return GetRandomValue(6, 7);
  return 7 + difficulty / 3;
}

static float Speed(float k) {
  const FunctionalSettings *items = FunctionalGet();
  return (30 + 7 * difficulty) * k * items->ringSpeed * items->bulletSpeed;
}

static float RingSpacing(float k) {
  return (60 + 5 * difficulty) * k;
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
  int x = (int)(a.x + (a.width - MeasureText(text, size)) / 2);
  DrawText(text, x, (int)y, size, color);
}

static int SpawnWave(float startRadius, float k, int count) {
  int spawned = 0;
  for (int i = 0; i < MAX_RINGS && spawned < count; i++) {
    if (!rings[i].on) {
      int sequence = spawned++;
      ringSpawnCount++;
      if (FunctionalGet()->removeRings && ringSpawnCount % 4 == 0) {
        continue;
      }
      lastGap = remainderf(lastGap + 137.507764f * DEG2RAD, 2 * PI);
      rings[i].radius = startRadius + (count - 1 - sequence) * RingSpacing(k);
      rings[i].gapAngle = lastGap;
      rings[i].on = true;
      rings[i].hit = false;
    }
  }
  return spawned;
}

void RingsStart(int d) {
  if (heartTex.id == 0)
    heartTex = LoadTexture("assets/heart.png");

  difficulty = d;
  if (difficulty < 1)
    difficulty = 1;
  if (difficulty > 10)
    difficulty = 10;

  const FunctionalSettings *items = FunctionalGet();
  lives = maxLives = 3 + items->extraLives;
  if (lives < 1)
    lives = maxLives = 1;
  damageCount = 0;
  state = RINGS_PLAYING;
  timeLeft = DURATION + items->duration;
  spawnTimer = 0;
  lastGap = GetRandomValue(0, 359) * DEG2RAD;
  ringSpawnCount = 0;
  invincible = idleTimer = dashTime = dashCooldown = 0;
  hazardsCleared = false;
  playerReady = false;
  memset(rings, 0, sizeof(rings));
}

static void Update(Rectangle a, float k) {
  float dt = GetFrameTime();
  float margin = 10 * k;
  const FunctionalSettings *items = FunctionalGet();
  Vector2 c = Center(a);
  Vector2 mouse = GetMousePosition();

  Vector2 target = {Limit(mouse.x, a.x + margin, a.x + a.width - margin),
                    Limit(mouse.y, a.y + margin, a.y + a.height - margin)};
  float dx = target.x - player.x, dy = target.y - player.y;
  float distance = hypotf(dx, dy);
  float step = 900 * k * items->playerSpeed * dt;
  if (distance > step && distance > 0) {
    dx *= step / distance;
    dy *= step / distance;
  }
  player.x += dx;
  player.y += dy;

  if (items->ringDash && IsKeyPressed(KEY_SPACE) && dashCooldown <= 0) {
    dashTime = 0.35f;
    dashCooldown = 2;
  }
  dashTime -= dt;
  dashCooldown -= dt;
  invincible -= dt;

  timeLeft -= dt;
  spawnTimer -= dt;
  if (items->clearBullets && !hazardsCleared && timeLeft <= 15) {
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
    int count = SpawnWave(OuterRadius(a, k), k, RingCount());
    if (count > 0)
      spawnTimer = count * RingSpacing(k) / Speed(k);
    else
      spawnTimer = 0.1f;
  }

  float dist = hypotf(player.x - c.x, player.y - c.y);
  float angle = atan2f(player.y - c.y, player.x - c.x);

  for (int i = 0; i < MAX_RINGS; i++) {
    Ring *r = &rings[i];
    if (!r->on)
      continue;

    r->radius -= Speed(k) * dt;
    if (r->radius <= 0) {
      r->on = false;
      continue;
    }

    bool touchesRing = fabsf(dist - r->radius) <
                       (THICK / 2 + 8) * k * items->hitbox;
    bool outsideGap = fabsf(remainderf(angle - r->gapAngle, 2 * PI)) > GapWidth() / 2;

    if (!r->hit && touchesRing && outsideGap && dashTime <= 0 &&
        invincible <= 0) {
      r->hit = true;
      if (!FunctionalBlockDamage()) {
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
  float halfGap = GapWidth() / 2 * RAD2DEG;

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
    Color color = WHITE;
    if (r->hit)
      color = GRAY;

    DrawRing(c, inner, outer, middle + halfGap, middle + 360 - halfGap, 0, color);
  }
  DrawHeart(player, 30 * k);
  EndScissorMode();

  for (int i = 0; i < lives; i++)
    DrawHeart((Vector2){a.x + 16 + i * 28, a.y + 16}, 20);
  if (FunctionalGet()->ringDash)
    DrawText("ESPACE : ESQUIVE", (int)a.x + 10,
             (int)(a.y + a.height - 24), 16, RAYWHITE);

  float shownTime = timeLeft;
  if (shownTime < 0)
    shownTime = 0;
  const char *timer = TextFormat("%.0f", shownTime);
  DrawText(timer, (int)(a.x + a.width) - MeasureText(timer, 20) - 10, (int)a.y + 6, 20, WHITE);

  if (state != RINGS_PLAYING) {
    const char *title = "GAME OVER";
    if (state == RINGS_WON)
      title = "VICTOIRE";
    DrawCentered(a, title, 40, a.y + a.height / 2 - 20, WHITE);
    DrawCentered(a, "ENTREE pour continuer", 20, a.y + a.height / 2 + 20, GRAY);
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
    player.x += smallSide / 4;
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