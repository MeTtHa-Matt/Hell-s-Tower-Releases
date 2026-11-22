#include "spikes.h"
#include "../balance.h"
#include "../modifiers.h"
#include "../../items/functionnal/functionnal.h"
#include "../../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define MAX_BALLS 40
#define DURATION 30.0f
#define SPIKE_COUNT 10
#define MAX_EXTRA_SPAWNS 3

typedef struct {
  Vector2 pos;
  Vector2 vel;
  float radius;
  float angle;
  float spin;
  float cooldown;
  bool on;
} Ball;

static Ball balls[MAX_BALLS];
static Texture2D heartTex;
static Vector2 player;
static bool playerReady;
static SpikesStatus state;
static int floorNumber, difficulty, lives, maxLives, damageCount;
static float timeLeft, invincible, idleTimer;
static bool hazardsCleared;
static bool modifiers[MINIGAME_MODIFIER_OPTIONS];
static const char *modifierNames[MINIGAME_MODIFIER_OPTIONS] = {
    "MOUVEMENT LENT", "BOULES PLUS RAPIDES", "PLUS DE REBONDS",
    "CONTACT PLUS DANGEREUX"};

static float Limit(float value, float min, float max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

static Vector2 Center(Rectangle a) {
  return (Vector2){a.x + a.width / 2, a.y + a.height / 2};
}

static float Rand01(void) { return GetRandomValue(0, 10000) / 10000.0f; }

static void DrawHeart(Vector2 c, float size) {
  Rectangle source = {0, 0, (float)heartTex.width, (float)heartTex.height};
  Rectangle dest = {c.x - size / 2, c.y - size / 2, size, size};
  DrawTexturePro(heartTex, source, dest, (Vector2){0, 0}, 0, WHITE);
}

static void DrawCentered(Rectangle a, const char *text, int size, float y,
                         Color color) {
  int fontSize = GameUiPixels(size);
  int x = (int)(a.x + (a.width - MeasureText(text, fontSize)) / 2);
  DrawText(text, x, (int)y, fontSize, color);
}

/* Chance qu'un rebond crée une nouvelle boule. */
static float BounceChance(void) {
  return 0.06f + 0.04f * (difficulty - 1) + (modifiers[2] ? 0.1f : 0.0f);
}

/* Chance d'ajouter une boule supplémentaire après un rebond générateur. */
static float ExtraChance(void) {
  return 0.08f + 0.08f * (difficulty - 1) / 9.0f;
}

static float BallSpeed(float k) {
  const FunctionalSettings *items = FunctionalGet();
  float base = 90.0f + 10.0f * difficulty;
  float speedMin = BalanceRandomSpeedMin(floorNumber, difficulty, base);
  float speedMax = BalanceRandomSpeedMax(floorNumber, difficulty, base);
  float speed = GetRandomValue((int)(speedMin * 100),
                               (int)(speedMax * 100)) / 100.0f;
  return speed * k * items->bulletSpeed * (modifiers[1] ? 1.15f : 1.0f);
}

static bool SpawnBall(Rectangle a, Vector2 pos, float k) {
  const FunctionalSettings *items = FunctionalGet();
  float smallSide = fminf(a.width, a.height);
  int activeBalls = 0;
  int maxBalls = 4 + difficulty + (modifiers[2] ? 2 : 0);

  for (int i = 0; i < MAX_BALLS; i++)
    activeBalls += balls[i].on;
  if (activeBalls >= maxBalls)
    return false;

  for (int i = 0; i < MAX_BALLS; i++) {
    if (balls[i].on)
      continue;

    float radius = GetRandomValue(14, 36) * k * items->bulletSize;
    radius = Limit(radius, 8 * k, smallSide * 0.2f);

    /* pas de boule qui apparait sur le coeur */
    if (hypotf(pos.x - player.x, pos.y - player.y) < radius + 70 * k)
      return false;

    float dir = GetRandomValue(0, 359) * DEG2RAD;
    float speed = BallSpeed(k);
    balls[i].pos = (Vector2){Limit(pos.x, a.x + radius, a.x + a.width - radius),
                             Limit(pos.y, a.y + radius, a.y + a.height - radius)};
    balls[i].vel = (Vector2){cosf(dir) * speed, sinf(dir) * speed};
    balls[i].radius = radius;
    balls[i].angle = GetRandomValue(0, 359);
    balls[i].spin = (float)GetRandomValue(-300, 300);
    balls[i].cooldown = 0;
    balls[i].on = true;
    return true;
  }
  return false;
}

static void SpawnInitial(Rectangle a, float k) {
  int count = 2 + difficulty / 5;
  for (int n = 0; n < count; n++) {
    for (int tries = 0; tries < 30; tries++) {
      Vector2 pos = {a.x + Rand01() * a.width, a.y + Rand01() * a.height};
      if (hypotf(pos.x - player.x, pos.y - player.y) > 160 * k &&
          SpawnBall(a, pos, k))
        break;
    }
  }
}

static void OnBounce(Rectangle a, Vector2 pos, float k) {
  float chance = BounceChance();
  if (Rand01() >= chance)
    return;

  int count = 1;
  while (count < 1 + MAX_EXTRA_SPAWNS && Rand01() < ExtraChance())
    count++;
  for (int i = 0; i < count; i++)
    SpawnBall(a, pos, k);
}

void SpikesStart(int floorNumberArg, int d) {
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

  const FunctionalSettings *items = FunctionalGet();
  lives = maxLives = 3 + items->extraLives;
  if (lives < 1)
    lives = maxLives = 1;
  damageCount = 0;
  state = SPIKES_PLAYING;
  float durationMin =
      BalanceMiniGameDurationMin(floorNumber, difficulty, DURATION);
  float durationMax =
      BalanceMiniGameDurationMax(floorNumber, difficulty, DURATION);
  timeLeft = GetRandomValue((int)(durationMin * 100),
                            (int)(durationMax * 100)) / 100.0f +
             items->duration;
  invincible = idleTimer = 0;
  hazardsCleared = false;
  playerReady = false;
  memset(balls, 0, sizeof(balls));
}

static void Hurt(void) {
  if (!FunctionalBlockDamageAtLives(lives)) {
    invincible = 1.5f + FunctionalGet()->invincibility;
    damageCount++;
    lives--;
    if (lives <= 0)
      state = SPIKES_LOST;
  }
}

static void Update(Rectangle a, float k) {
  float dt = GetFrameTime();
  float margin = 15 * k;
  const FunctionalSettings *items = FunctionalGet();

  /* déplacement du coeur vers la souris */
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
  player.x = Limit(player.x + dx, a.x + margin, a.x + a.width - margin);
  player.y = Limit(player.y + dy, a.y + margin, a.y + a.height - margin);

  timeLeft -= dt;
  invincible -= dt;

  if (GetMouseDelta().x != 0 || GetMouseDelta().y != 0)
    idleTimer = 0;
  else
    idleTimer += dt;
  if (items->regeneration > 0 && idleTimer >= items->regeneration &&
      lives < maxLives) {
    lives++;
    idleTimer = 0;
  }
  if (items->clearBullets && !hazardsCleared &&
      timeLeft <= items->clearBulletsAt) {
    memset(balls, 0, sizeof(balls));
    hazardsCleared = true;
  }

  for (int i = 0; i < MAX_BALLS; i++) {
    Ball *b = &balls[i];
    if (!b->on)
      continue;

    b->pos.x += b->vel.x * dt;
    b->pos.y += b->vel.y * dt;
    b->angle += b->spin * dt;
    b->cooldown -= dt;

    bool bounced = false;
    if (b->pos.x - b->radius < a.x) {
      b->pos.x = a.x + b->radius;
      b->vel.x = fabsf(b->vel.x);
      bounced = true;
    } else if (b->pos.x + b->radius > a.x + a.width) {
      b->pos.x = a.x + a.width - b->radius;
      b->vel.x = -fabsf(b->vel.x);
      bounced = true;
    }
    if (b->pos.y - b->radius < a.y) {
      b->pos.y = a.y + b->radius;
      b->vel.y = fabsf(b->vel.y);
      bounced = true;
    } else if (b->pos.y + b->radius > a.y + a.height) {
      b->pos.y = a.y + a.height - b->radius;
      b->vel.y = -fabsf(b->vel.y);
      bounced = true;
    }
    if (bounced)
      OnBounce(a, b->pos, k);

    float hitRadius = (b->radius * 0.9f + 11 * k * items->hitbox) *
                      (modifiers[3] ? 1.12f : 1.0f);
    if (b->cooldown <= 0 && invincible <= 0 &&
        hypotf(player.x - b->pos.x, player.y - b->pos.y) <= hitRadius) {
      b->cooldown = 1.0f;
      Hurt();
    }
  }

  if (timeLeft <= 0 && state == SPIKES_PLAYING)
    state = SPIKES_WON;
}

static void DrawBall(Ball *b) {
  float body = b->radius * 0.72f;
  float halfWidth = PI / SPIKE_COUNT * 0.55f;

  for (int s = 0; s < SPIKE_COUNT; s++) {
    float ang = (b->angle * DEG2RAD) + s * 2 * PI / SPIKE_COUNT;
    Vector2 tip = {b->pos.x + cosf(ang) * b->radius,
                   b->pos.y + sinf(ang) * b->radius};
    Vector2 left = {b->pos.x + cosf(ang - halfWidth) * body,
                    b->pos.y + sinf(ang - halfWidth) * body};
    Vector2 right = {b->pos.x + cosf(ang + halfWidth) * body,
                     b->pos.y + sinf(ang + halfWidth) * body};
    /* les deux ordres : raylib exige un sens de rotation précis */
    DrawTriangle(tip, left, right, LIGHTGRAY);
    DrawTriangle(tip, right, left, LIGHTGRAY);
  }
  DrawCircleV(b->pos, body, (Color){55, 55, 65, 255});
  DrawCircleLines((int)b->pos.x, (int)b->pos.y, body, WHITE);
}

static void Draw(Rectangle a, float k) {
  float ui = GameUiScale();
  int hudSize = GameUiPixels(20);

  BeginScissorMode((int)a.x, (int)a.y, (int)a.width, (int)a.height);
  for (int i = 0; i < MAX_BALLS; i++)
    if (balls[i].on)
      DrawBall(&balls[i]);

  bool visible = invincible <= 0 || (int)(invincible * 10) % 2 == 0;
  if (visible)
    DrawHeart(player, 30 * k);
  EndScissorMode();

  for (int i = 0; i < lives; i++)
    DrawHeart((Vector2){a.x + (16 + i * 28) * ui, a.y + 16 * ui}, 20 * ui);
  float shownTime = timeLeft;
  if (shownTime < 0)
    shownTime = 0;
  const char *timer = TextFormat("%.0f", shownTime);
  DrawText(timer, (int)(a.x + a.width) - MeasureText(timer, hudSize) -
                      GameUiPixels(10),
           (int)a.y + GameUiPixels(6), hudSize, WHITE);

  if (state != SPIKES_PLAYING) {
    const char *title = "GAME OVER";
    if (state == SPIKES_WON)
      title = "VICTOIRE";
    DrawCentered(a, title, 40, a.y + a.height / 2 - GameUiPixels(20), WHITE);
    DrawCentered(a, "ENTREE pour continuer", 20,
                 a.y + a.height / 2 + GameUiPixels(20), GRAY);
  }
}

void SpikesUpdate(Rectangle a) {
  if (state == SPIKES_OFF)
    return;

  float smallSide = a.width;
  if (a.height < smallSide)
    smallSide = a.height;
  float k = smallSide / 400;

  if (!playerReady) {
    player = Center(a);
    playerReady = true;
    SpawnInitial(a, k);
  }

  if (state == SPIKES_PLAYING)
    Update(a, k);
  if (state != SPIKES_PLAYING && state != SPIKES_OFF && IsKeyPressed(KEY_ENTER))
    state = SPIKES_OFF;

  Draw(a, k);
}

SpikesStatus SpikesState(void) {
  return state;
}

int SpikesDamageCount(void) {
  return damageCount;
}

void SpikesUnload(void) {
  UnloadTexture(heartTex);
  heartTex = (Texture2D){0};
  playerReady = false;
}
