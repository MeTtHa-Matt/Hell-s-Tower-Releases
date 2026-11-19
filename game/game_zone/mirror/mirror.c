#include "mirror.h"
#include "../../items/functionnal/functionnal.h"
#include "../../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define MAX_BULLETS 200
#define DURATION 30.0f
#define TURN 0.4f

typedef struct {
  Vector2 pos;
  Vector2 vel;
  int side;
  bool on;
} Bullet;

static Bullet bullets[MAX_BULLETS];
static Texture2D heartTex, bulletTex;
static Vector2 player;
static bool playerReady;
static MirrorStatus state;
static int difficulty, lives, maxLives, damageCount, protectedSide;
static float timeLeft, spawnTimer, invincible, idleTimer, protectedTime;
static bool bulletsCleared;

static float Limit(float value, float min, float max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

static Rectangle Half(Rectangle a, int side) {
  return (Rectangle){a.x + side * a.width / 2, a.y, a.width / 2, a.height};
}

static Vector2 Center(Rectangle r) {
  return (Vector2){r.x + r.width / 2, r.y + r.height / 2};
}

static Vector2 Heart(Rectangle a, int side) {
  return (Vector2){player.x + side * a.width / 2, player.y};
}

static float Jitter(float range) {
  return GetRandomValue(-100, 100) / 100.0f * range;
}

static void DrawSprite(Texture2D tex, Vector2 c, float size, float rotation) {
  Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
  Rectangle dest = {c.x, c.y, size, size};
  Vector2 origin = {size / 2, size / 2};
  DrawTexturePro(tex, source, dest, origin, rotation, WHITE);
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

static Vector2 SpawnPosition(Rectangle half, float angle, float k) {
  float dirX = cosf(angle);
  float dirY = sinf(angle);
  float inset = 16 * k;

  float rangeX = half.width / 2 - inset;
  float rangeY = half.height / 2 - inset;
  if (rangeX < 0)
    rangeX = 0;
  if (rangeY < 0)
    rangeY = 0;

  float distX = INFINITY;
  float distY = INFINITY;
  if (fabsf(dirX) > 0.001f)
    distX = rangeX / fabsf(dirX);
  if (fabsf(dirY) > 0.001f)
    distY = rangeY / fabsf(dirY);

  float dist = distX;
  if (distY < distX)
    dist = distY;

  Vector2 c = Center(half);
  return (Vector2){c.x + dirX * dist, c.y + dirY * dist};
}

static void SpawnOne(Rectangle a, float k, int side, float angle) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].on)
      continue;

    Vector2 pos = SpawnPosition(Half(a, side), angle, k);
    Vector2 heart = Heart(a, side);
    float progress = (difficulty - 1) / 9.0f;
    float spread = (75 - 40 * progress) * k;

    float dirX = heart.x + Jitter(spread) - pos.x;
    float dirY = heart.y + Jitter(spread) - pos.y;
    float length = hypotf(dirX, dirY);
    if (length > 0) {
      dirX /= length;
      dirY /= length;
    }

    float speed = (100 + 155 * progress) * k;
    bullets[i].pos = pos;
    bullets[i].vel = (Vector2){dirX * speed, dirY * speed};
    bullets[i].side = side;
    bullets[i].on = true;
    return;
  }
}

static void SpawnPair(Rectangle a, float k) {
  float angle = GetRandomValue(0, 359) * DEG2RAD;
  SpawnOne(a, k, 0, angle);
  SpawnOne(a, k, 1, PI - angle);
}

void MirrorStart(int d) {
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
  state = MIRROR_PLAYING;
  timeLeft = DURATION + items->duration;
  spawnTimer = 0;
  invincible = 0;
  idleTimer = 0;
  protectedTime = items->mirrorInvincible ? 5 : 0;
  protectedSide = GetRandomValue(0, 1);
  bulletsCleared = false;
  playerReady = false;
  memset(bullets, 0, sizeof(bullets));
}

static void MovePlayer(Rectangle a, float k, float dt) {
  Rectangle left = Half(a, 0);
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

  player.x = Limit(player.x, left.x + margin, left.x + left.width - margin);
  player.y = Limit(player.y, left.y + margin, left.y + left.height - margin);
}

static void UpdateBullet(Bullet *b, Rectangle a, float k, float dt) {
  const FunctionalSettings *items = FunctionalGet();
  Vector2 heart = Heart(a, b->side);

  float wantX = heart.x - b->pos.x;
  float wantY = heart.y - b->pos.y;
  float dot = b->vel.x * wantX + b->vel.y * wantY;
  if (dot > 0) {
    float cross = b->vel.x * wantY - b->vel.y * wantX;
    float turn = Limit(atan2f(cross, dot), -TURN * dt, TURN * dt);
    float vx = b->vel.x * cosf(turn) - b->vel.y * sinf(turn);
    float vy = b->vel.x * sinf(turn) + b->vel.y * cosf(turn);
    b->vel = (Vector2){vx, vy};
  }

  if (items->mirrorRepel && b->side == 0 &&
      hypotf(heart.x - b->pos.x, heart.y - b->pos.y) < 100 * k) {
    float length = hypotf(b->pos.x - heart.x, b->pos.y - heart.y);
    if (length > 0) {
      float speed = hypotf(b->vel.x, b->vel.y);
      b->vel.x = (b->pos.x - heart.x) / length * speed;
      b->vel.y = (b->pos.y - heart.y) / length * speed;
    }
  }

  Vector2 previous = b->pos;
  float speed = items->bulletSpeed;
  if (b->side == 1)
    speed *= items->mirrorRightSpeed;
  if (difficulty >= 7 && items->mirrorHighAid)
    speed *= 0.88f;
  b->pos.x += b->vel.x * speed * dt;
  b->pos.y += b->vel.y * speed * dt;

  if (NearSegment(heart, previous, b->pos, 20 * k * items->hitbox)) {
    b->on = false;
    if (invincible <= 0 && !(protectedTime > 0 && b->side == protectedSide)) {
      if (!FunctionalBlockDamage()) {
        invincible = 1.5f + items->invincibility;
        damageCount++;
        lives--;
        if (lives <= 0)
          state = MIRROR_LOST;
      }
    }
  } else if (!CheckCollisionPointRec(b->pos, Half(a, b->side))) {
    b->on = false;
  }
}

static void Update(Rectangle a, float k) {
  float dt = GetFrameTime();
  const FunctionalSettings *items = FunctionalGet();

  MovePlayer(a, k, dt);

  timeLeft -= dt;
  invincible -= dt;
  protectedTime -= dt;
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
  if (items->clearBullets && !bulletsCleared && timeLeft <= 15) {
    memset(bullets, 0, sizeof(bullets));
    bulletsCleared = true;
  }
  spawnTimer -= dt;
  if (spawnTimer <= 0) {
    float progress = (difficulty - 1) / 9.0f;
    spawnTimer = 0.85f - 0.30f * progress;
    SpawnPair(a, k);
  }

  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].on)
      UpdateBullet(&bullets[i], a, k, dt);
  }

  if (timeLeft <= 0 && state == MIRROR_PLAYING)
    state = MIRROR_WON;
}

static void Draw(Rectangle a, float k) {
  float ui = GameUiScale();
  int hudSize = GameUiPixels(20);
  for (int side = 0; side < 2; side++) {
    Rectangle h = Half(a, side);
    Vector2 heart = Heart(a, side);

    BeginScissorMode((int)h.x, (int)h.y, (int)h.width, (int)h.height);
    for (int i = 0; i < MAX_BULLETS; i++) {
      Bullet *b = &bullets[i];
      if (!b->on || b->side != side)
        continue;
      float angle = atan2f(heart.y - b->pos.y, heart.x - b->pos.x) * RAD2DEG;
      DrawSprite(bulletTex, b->pos, 24 * k, angle);
    }

    bool visible = invincible <= 0 || (int)(invincible * 10) % 2 == 0;
    if (visible)
      DrawSprite(heartTex, heart, 30 * k, 0);
    EndScissorMode();
  }

  int middle = (int)(a.x + a.width / 2);
  DrawLine(middle, (int)a.y, middle, (int)(a.y + a.height), WHITE);

  for (int i = 0; i < lives; i++)
    DrawSprite(heartTex,
               (Vector2){a.x + (16 + i * 28) * ui, a.y + 16 * ui},
               20 * ui, 0);

  float shownTime = timeLeft;
  if (shownTime < 0)
    shownTime = 0;
  const char *timer = TextFormat("%.0f", shownTime);
  DrawText(timer, (int)(a.x + a.width) - MeasureText(timer, hudSize) -
                      GameUiPixels(10),
           (int)a.y + GameUiPixels(6), hudSize, WHITE);

  if (state != MIRROR_PLAYING) {
    const char *title = "GAME OVER";
    if (state == MIRROR_WON)
      title = "VICTOIRE";
    DrawCentered(a, title, 40,
                 a.y + a.height / 2 - GameUiPixels(20), WHITE);
    DrawCentered(a, "ENTREE pour continuer", 20,
                 a.y + a.height / 2 + GameUiPixels(20), GRAY);
  }
}

void MirrorUpdate(Rectangle a) {
  if (state == MIRROR_OFF)
    return;

  float smallSide = a.width / 2;
  if (a.height < smallSide)
    smallSide = a.height;
  float k = smallSide / 400;

  if (!playerReady) {
    player = Center(Half(a, 0));
    playerReady = true;
  }

  if (state == MIRROR_PLAYING)
    Update(a, k);
  if (state != MIRROR_PLAYING && state != MIRROR_OFF && IsKeyPressed(KEY_ENTER))
    state = MIRROR_OFF;

  Draw(a, k);
}

MirrorStatus MirrorState(void) {
  return state;
}

int MirrorDamageCount(void) {
  return damageCount;
}

void MirrorUnload(void) {
  UnloadTexture(heartTex);
  UnloadTexture(bulletTex);
  heartTex = (Texture2D){0};
  bulletTex = (Texture2D){0};
  playerReady = false;
}