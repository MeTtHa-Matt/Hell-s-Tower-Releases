#include "bullethell.h"
#include "../balance.h"
#include "../../items/functionnal/functionnal.h"
#include "../../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define MAX_BULLETS 300
#define DURATION 30.0f
#define TURN 0.4f

typedef struct {
  Vector2 pos;
  Vector2 vel;
  bool on;
} Bullet;

static Bullet bullets[MAX_BULLETS];
static Texture2D heartTex, bulletTex;
static Vector2 player;
static bool playerReady;
static HellState state;
static int difficulty, lives, maxLives, damageCount;
static float timeLeft, spawnTimer, invincible, idleTimer;
static bool bulletsCleared;

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

static float Jitter(float range) {
  return GetRandomValue(-100, 100) / 100.0f * range;
}

static void DrawSprite(Texture2D tex, Vector2 c, float size) {
  Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
  Rectangle dest = {c.x - size / 2, c.y - size / 2, size, size};
  DrawTexturePro(tex, source, dest, (Vector2){0, 0}, 0, WHITE);
}

static void DrawBullet(Bullet *b, float size) {
  float height = size * bulletTex.height / bulletTex.width;
  float angle = atan2f(b->vel.y, b->vel.x) * RAD2DEG;

  Rectangle source = {0, 0, (float)bulletTex.width, (float)bulletTex.height};
  Rectangle dest = {b->pos.x - size / 2, b->pos.y - height / 2, size, height};
  Vector2 origin = {size / 2, height / 2};
  DrawTexturePro(bulletTex, source, dest, origin, angle, WHITE);
}

static void DrawCentered(Rectangle a, const char *text, int size, float y, Color color) {
  int fontSize = GameUiPixels(size);
  int x = (int)(a.x + (a.width - MeasureText(text, fontSize)) / 2);
  DrawText(text, x, (int)y, fontSize, color);
}

static bool NearSegment(Vector2 p, Vector2 start, Vector2 end, float radius) {
  float sx = end.x - start.x;
  float sy = end.y - start.y;
  float length2 = sx * sx + sy * sy;

  float t = 0;
  if (length2 > 0)
    t = ((p.x - start.x) * sx + (p.y - start.y) * sy) / length2;
  t = Limit(t, 0, 1);

  float dx = start.x + sx * t - p.x;
  float dy = start.y + sy * t - p.y;
  return dx * dx + dy * dy <= radius * radius;
}

static void Spawn(Rectangle a, float k) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].on)
      continue;

    Vector2 c = Center(a);
    float angle = GetRandomValue(0, 359) * DEG2RAD;
    Vector2 pos = {c.x + cosf(angle) * a.width, c.y + sinf(angle) * a.width};

    float spread = BalanceBulletHellSpread(difficulty) * k;
    float speed = BalanceBulletHellSpeed(difficulty) * k;

    float dirX = player.x + Jitter(spread) - pos.x;
    float dirY = player.y + Jitter(spread) - pos.y;
    float length = hypotf(dirX, dirY);
    if (length > 0) {
      dirX /= length;
      dirY /= length;
    }

    bullets[i].pos = pos;
    bullets[i].vel = (Vector2){dirX * speed, dirY * speed};
    bullets[i].on = true;
    return;
  }
}

void BulletHellStart(int d) {
  if (heartTex.id == 0) {
    heartTex = LoadTexture("assets/heart.png");
    bulletTex = LoadTexture("assets/projectile.png");
  }

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
  state = HELL_PLAYING;
  timeLeft = DURATION + items->duration;
  spawnTimer = 0;
  invincible = 0;
  idleTimer = 0;
  bulletsCleared = false;
  playerReady = false;
  memset(bullets, 0, sizeof(bullets));
}

static void MovePlayer(Rectangle a, float k, float dt) {
  float margin = 10 * k;

  float dirX = 0;
  float dirY = 0;
  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
    dirX += 1;
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
    dirX -= 1;
  if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
    dirY += 1;
  if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
    dirY -= 1;

  Vector2 mouse = GetMouseDelta();
  const FunctionalSettings *items = FunctionalGet();
  player.x += (dirX * 220 * k * dt + mouse.x) * items->playerSpeed;
  player.y += (dirY * 220 * k * dt + mouse.y) * items->playerSpeed;

  player.x = Limit(player.x, a.x + margin, a.x + a.width - margin);
  player.y = Limit(player.y, a.y + margin, a.y + a.height - margin);
}

static void UpdateBullet(Bullet *b, Rectangle a, float k, float dt) {
  const FunctionalSettings *items = FunctionalGet();
  float wantX = player.x - b->pos.x;
  float wantY = player.y - b->pos.y;
  float dot = b->vel.x * wantX + b->vel.y * wantY;
  if (dot > 0) {
    float cross = b->vel.x * wantY - b->vel.y * wantX;
    float turn = Limit(atan2f(cross, dot), -TURN * dt, TURN * dt);
    float vx = b->vel.x * cosf(turn) - b->vel.y * sinf(turn);
    float vy = b->vel.x * sinf(turn) + b->vel.y * cosf(turn);
    b->vel = (Vector2){vx, vy};
  }

  Vector2 previous = b->pos;
  float speed = items->bulletSpeed;
  if (hypotf(player.x - b->pos.x, player.y - b->pos.y) < 100 * k)
    speed *= items->nearbyBulletSpeed;
  if (difficulty >= 7 && items->bulletHellHighAid)
    speed *= 0.88f;
  b->pos.x += b->vel.x * speed * dt;
  b->pos.y += b->vel.y * speed * dt;

  Vector2 c = Center(a);
  if (NearSegment(player, previous, b->pos,
                  27 * k * items->hitbox * items->bulletSize)) {
    b->on = false;
    if (invincible <= 0) {
      if (!FunctionalBlockDamageAtLives(lives)) {
        invincible = 1.5f + items->invincibility;
        damageCount++;
        lives--;
        if (lives <= 0)
          state = HELL_LOST;
      }
    }
  } else if (fabsf(b->pos.x - c.x) > a.width || fabsf(b->pos.y - c.y) > a.width) {
    b->on = false;
  }
}

static void Update(Rectangle a, float k) {
  float dt = GetFrameTime();
  const FunctionalSettings *items = FunctionalGet();

  MovePlayer(a, k, dt);

  timeLeft -= dt;
  invincible -= dt;
  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_A) || IsKeyDown(KEY_W) ||
      IsKeyDown(KEY_S) || GetMouseDelta().x != 0 || GetMouseDelta().y != 0)
    idleTimer = 0;
  else
    idleTimer += dt;
  if (items->regeneration > 0 && idleTimer >= items->regeneration &&
      lives < maxLives) {
    lives++;
    idleTimer = 0;
  }
  if (items->clearBullets && !bulletsCleared &&
      timeLeft <= items->clearBulletsAt) {
    memset(bullets, 0, sizeof(bullets));
    bulletsCleared = true;
  }
  spawnTimer -= dt;
  if (spawnTimer <= 0) {
    spawnTimer = BalanceBulletHellSpawnInterval(difficulty) * items->bulletSpawn;
    Spawn(a, k);
  }

  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].on)
      UpdateBullet(&bullets[i], a, k, dt);
  }

  if (timeLeft <= 0 && state == HELL_PLAYING)
    state = HELL_WON;
}

static void Draw(Rectangle a, float k) {
  float ui = GameUiScale();
  int hudSize = GameUiPixels(20);
  BeginScissorMode((int)a.x, (int)a.y, (int)a.width, (int)a.height);
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].on)
      DrawBullet(&bullets[i], 24 * k * FunctionalGet()->bulletSize);
  }

  bool visible = invincible <= 0 || (int)(invincible * 10) % 2 == 0;
  if (visible)
    DrawSprite(heartTex, player, 30 * k);
  EndScissorMode();

  for (int i = 0; i < lives; i++)
    DrawSprite(heartTex,
               (Vector2){a.x + (16 + i * 28) * ui, a.y + 16 * ui},
               20 * ui);

  float shownTime = timeLeft;
  if (shownTime < 0)
    shownTime = 0;
  const char *timer = TextFormat("%.0f", shownTime);
  DrawText(timer, (int)(a.x + a.width) - MeasureText(timer, hudSize) -
                      GameUiPixels(10),
           (int)a.y + GameUiPixels(6), hudSize, WHITE);

  if (state != HELL_PLAYING) {
    const char *title = "GAME OVER";
    if (state == HELL_WON)
      title = "VICTOIRE";
    DrawCentered(a, title, 40,
                 a.y + a.height / 2 - GameUiPixels(20), WHITE);
    DrawCentered(a, "ENTREE pour continuer", 20,
                 a.y + a.height / 2 + GameUiPixels(20), GRAY);
  }
}

void BulletHellUpdate(Rectangle a) {
  if (state == HELL_OFF)
    return;

  float smallSide = a.width;
  if (a.height < smallSide)
    smallSide = a.height;
  float k = smallSide / 400;

  if (!playerReady) {
    player = Center(a);
    playerReady = true;
  }

  if (state == HELL_PLAYING)
    Update(a, k);
  if (state != HELL_PLAYING && state != HELL_OFF && IsKeyPressed(KEY_ENTER))
    state = HELL_OFF;

  Draw(a, k);
}

HellState BulletHellState(void) {
  return state;
}

int BulletHellDamageCount(void) {
  return damageCount;
}

void BulletHellUnload(void) {
  UnloadTexture(heartTex);
  UnloadTexture(bulletTex);
  heartTex = (Texture2D){0};
  bulletTex = (Texture2D){0};
  playerReady = false;
}