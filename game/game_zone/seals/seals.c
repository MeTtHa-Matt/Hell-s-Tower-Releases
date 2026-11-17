#include "seals.h"
#include "../../items/functionnal/functionnal.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define MAX_HAZARDS 32
#define DURATION 50.0f
#define GOAL 4
#define SEAL_TIME 2.0f
#define ACTIVE 0.35f
#define DASH_TIME 0.22f
#define DASH_COOLDOWN 1.2f

enum { BLAST, ROW, COLUMN };
enum { FRONT, BACK, LEFT, RIGHT };

typedef struct {
  int kind;
  Vector2 pos;
  float size;
  float t;
  bool on;
  bool hit;
} Hazard;

static const char *dirNames[4] = {"front", "back", "left", "right"};
static const char *footNames[2] = {"left", "right"};

static Hazard hazards[MAX_HAZARDS];
static Texture2D heartTex, sealTex, blastTex, shadowTex, sprites[4][2];
static Vector2 player, rune, dir;
static bool ready;
static SealsStatus state;
static int difficulty, lives, maxLives, damageCount, sealed, facing;
static float timeLeft, spawnTimer, invincible, charge, step, dashTime, dashCooldown;

static float Limit(float value, float min, float max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

static float Jitter(float range) {
  return GetRandomValue(-100, 100) / 100.0f * range;
}

static Vector2 Center(Rectangle a) {
  return (Vector2){a.x + a.width / 2, a.y + a.height / 2};
}

static void DrawCentered(Rectangle a, const char *text, int size, float y, Color color) {
  int x = (int)(a.x + (a.width - MeasureText(text, size)) / 2);
  DrawText(text, x, (int)y, size, color);
}

// Temps d'avertissement avant qu'un danger devienne actif.
static float Warn(void) {
  return 1.2f - 0.07f * difficulty;
}

static void AddHazard(int kind, Vector2 pos, float size) {
  for (int i = 0; i < MAX_HAZARDS; i++) {
    if (hazards[i].on)
      continue;
    hazards[i] = (Hazard){kind, pos, size, 0, true, false};
    return;
  }
}

static void SpawnHazards(float k) {
  int roll = GetRandomValue(0, 3);
  Vector2 pos = {player.x + Jitter(50 * k), player.y + Jitter(50 * k)};

  if (roll < 2)
    AddHazard(BLAST, pos, 60 * k);
  else if (roll == 2)
    AddHazard(ROW, pos, 70 * k);
  else
    AddHazard(COLUMN, pos, 70 * k);

  // Le sceau est pris pour cible quand on commence à le charger.
  if (charge > 0 && GetRandomValue(0, 2) == 0)
    AddHazard(BLAST, rune, 70 * k);
}

static void PlaceRune(Rectangle a, float k) {
  float margin = 60 * k;

  for (int i = 0; i < 20; i++) {
    rune.x = a.x + margin + GetRandomValue(0, 1000) / 1000.0f * (a.width - 2 * margin);
    rune.y = a.y + margin + GetRandomValue(0, 1000) / 1000.0f * (a.height - 2 * margin);
    if (hypotf(rune.x - player.x, rune.y - player.y) > 150 * k)
      return;
  }
}

void SealsStart(int d) {
  if (heartTex.id == 0)
    heartTex = LoadTexture("assets/heart.png");
  if (sealTex.id == 0)
    sealTex = LoadTexture("assets/seal.png");
  if (blastTex.id == 0)
    blastTex = LoadTexture("assets/seal_blast.png");
  if (shadowTex.id == 0)
    shadowTex = LoadTexture("assets/player/shadow.png");
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 2; j++) {
      if (sprites[i][j].id == 0)
        sprites[i][j] = LoadTexture(TextFormat("assets/player/%s_%s_feet.png",
                                               dirNames[i], footNames[j]));
    }
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
  sealed = 0;
  state = SEALS_PLAYING;
  timeLeft = DURATION + items->duration;
  spawnTimer = 1.5f;
  invincible = charge = step = dashTime = dashCooldown = 0;
  dir = (Vector2){0, 1};
  facing = FRONT;
  ready = false;
  memset(hazards, 0, sizeof(hazards));
}

static void MovePlayer(Rectangle a, float k, float dt) {
  const FunctionalSettings *items = FunctionalGet();
  float margin = 12 * k;
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

  float length = hypotf(dirX, dirY);
  if (length > 0 && dashTime <= 0) {
    dir = (Vector2){dirX / length, dirY / length};
    step += dt;
    if (dirX > 0)
      facing = RIGHT;
    if (dirX < 0)
      facing = LEFT;
    if (dirX == 0 && dirY > 0)
      facing = FRONT;
    if (dirX == 0 && dirY < 0)
      facing = BACK;
  } else if (length == 0) {
    step = 0;
  }

  if (IsKeyPressed(KEY_SPACE) && dashCooldown <= 0 && dashTime <= 0) {
    dashTime = DASH_TIME;
    dashCooldown = DASH_COOLDOWN;
  }

  float speed = 200 * k * items->playerSpeed;
  if (dashTime > 0)
    speed = 650 * k;
  if (dashTime > 0 || length > 0) {
    player.x += dir.x * speed * dt;
    player.y += dir.y * speed * dt;
  }

  player.x = Limit(player.x, a.x + margin, a.x + a.width - margin);
  player.y = Limit(player.y, a.y + margin, a.y + a.height - margin);
}

static bool Touches(Hazard *h, float k) {
  float radius = 10 * k * FunctionalGet()->hitbox;

  if (h->kind == BLAST)
    return hypotf(player.x - h->pos.x, player.y - h->pos.y) < h->size + radius;
  if (h->kind == ROW)
    return fabsf(player.y - h->pos.y) < h->size / 2 + radius;
  return fabsf(player.x - h->pos.x) < h->size / 2 + radius;
}

static void UpdateHazard(Hazard *h, float k, float dt) {
  const FunctionalSettings *items = FunctionalGet();

  h->t += dt;
  if (h->t > Warn() + ACTIVE) {
    h->on = false;
    return;
  }
  if (h->t < Warn() || h->hit || invincible > 0 || dashTime > 0 || !Touches(h, k))
    return;

  h->hit = true;
  if (FunctionalBlockDamage())
    return;
  invincible = 1.5f + items->invincibility;
  damageCount++;
  lives--;
  if (lives <= 0)
    state = SEALS_LOST;
}

static void UpdateRune(Rectangle a, float k, float dt) {
  if (hypotf(player.x - rune.x, player.y - rune.y) < 45 * k)
    charge += dt;
  else
    charge -= dt * 1.5f;
  charge = Limit(charge, 0, SEAL_TIME);

  if (charge < SEAL_TIME)
    return;

  sealed++;
  charge = 0;
  spawnTimer = 1.0f;
  memset(hazards, 0, sizeof(hazards));
  if (sealed >= GOAL)
    state = SEALS_WON;
  else
    PlaceRune(a, k);
}

static void Update(Rectangle a, float k) {
  float dt = GetFrameTime();
  const FunctionalSettings *items = FunctionalGet();

  timeLeft -= dt;
  invincible -= dt;
  dashTime -= dt;
  dashCooldown -= dt;

  MovePlayer(a, k, dt);
  UpdateRune(a, k, dt);

  spawnTimer -= dt;
  if (spawnTimer <= 0 && state == SEALS_PLAYING) {
    float delay = 1.2f - 0.05f * difficulty - 0.1f * sealed;
    spawnTimer = Limit(delay, 0.3f, 2) * items->bulletSpawn;
    for (int i = 0; i <= sealed / 2; i++)
      SpawnHazards(k);
  }

  for (int i = 0; i < MAX_HAZARDS; i++) {
    if (hazards[i].on)
      UpdateHazard(&hazards[i], k, dt);
  }

  if (timeLeft <= 0 && state == SEALS_PLAYING)
    state = SEALS_LOST;
}

static void DrawHazard(Hazard *h, Rectangle a) {
  float warn = Warn();
  bool firing = h->t >= warn;

  if (h->kind == BLAST) {
    if (firing) {
      if (blastTex.id != 0) {
        Rectangle source = {0, 0, (float)blastTex.width, (float)blastTex.height};
        Rectangle dest = {h->pos.x - h->size, h->pos.y - h->size,
                          h->size * 2, h->size * 2};
        DrawTexturePro(blastTex, source, dest, (Vector2){0, 0}, 0, WHITE);
      }
    } else {
      DrawCircleV(h->pos, h->size * h->t / warn, Fade(RED, 0.5f));
      DrawCircleLinesV(h->pos, h->size, RED);
    }
    return;
  }

  Color color = Fade(RED, 0.15f + 0.3f * h->t / warn);
  if (firing)
    color = WHITE;

  Rectangle band = {a.x, h->pos.y - h->size / 2, a.width, h->size};
  if (h->kind == COLUMN)
    band = (Rectangle){h->pos.x - h->size / 2, a.y, h->size, a.height};
  DrawRectangleRec(band, color);
}

static void DrawPlayer(float k) {
  Texture2D tex = sprites[facing][(int)(step / 0.18f) % 2];
  if (tex.id == 0)
    return;

  float height = 56 * k;
  float width = height * tex.width / tex.height;
  Color tint = WHITE;
  if (dashTime > 0)
    tint = Fade(WHITE, 0.4f);

  if (shadowTex.id != 0) {
    float shadowWidth = 56 * k;
    float shadowHeight = shadowWidth * shadowTex.height / shadowTex.width;
    Rectangle source = {0, 0, (float)shadowTex.width, (float)shadowTex.height};
    Rectangle dest = {player.x - shadowWidth / 2,
                      player.y + 16 * k - shadowHeight / 2,
                      shadowWidth, shadowHeight};
    DrawTexturePro(shadowTex, source, dest, (Vector2){0, 0}, 0, WHITE);
  }

  Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
  Rectangle dest = {player.x - width / 2, player.y - height / 2, width, height};
  DrawTexturePro(tex, source, dest, (Vector2){0, 0}, 0, tint);
}

static void DrawHeart(Vector2 c, float size) {
  Rectangle source = {0, 0, (float)heartTex.width, (float)heartTex.height};
  Rectangle dest = {c.x - size / 2, c.y - size / 2, size, size};
  DrawTexturePro(heartTex, source, dest, (Vector2){0, 0}, 0, WHITE);
}

static void Draw(Rectangle a, float k) {
  BeginScissorMode((int)a.x, (int)a.y, (int)a.width, (int)a.height);

  if (sealTex.id != 0) {
    Rectangle source = {0, 0, (float)sealTex.width, (float)sealTex.height};
    Rectangle dest = {rune.x - 50 * k, rune.y - 50 * k, 100 * k, 100 * k};
    DrawTexturePro(sealTex, source, dest, (Vector2){0, 0}, 0, WHITE);
  }
  DrawRing(rune, 52 * k, 58 * k, 0, 360 * charge / SEAL_TIME, 0, GOLD);

  for (int i = 0; i < MAX_HAZARDS; i++) {
    if (hazards[i].on)
      DrawHazard(&hazards[i], a);
  }

  bool visible = invincible <= 0 || (int)(invincible * 10) % 2 == 0;
  if (visible)
    DrawPlayer(k);
  EndScissorMode();

  for (int i = 0; i < lives; i++)
    DrawHeart((Vector2){a.x + 16 + i * 28, a.y + 16}, 20);

  DrawCentered(a, TextFormat("SCEAUX %d/%d", sealed, GOAL), 20, a.y + 6, GOLD);

  float shownTime = timeLeft;
  if (shownTime < 0)
    shownTime = 0;
  const char *timer = TextFormat("%.0f", shownTime);
  DrawText(timer, (int)(a.x + a.width) - MeasureText(timer, 20) - 10, (int)a.y + 6, 20, WHITE);

  Color dashColor = RAYWHITE;
  if (dashCooldown > 0)
    dashColor = GRAY;
  DrawText("ESPACE : ESQUIVE", (int)a.x + 10, (int)(a.y + a.height - 24), 16, dashColor);

  if (state != SEALS_PLAYING) {
    const char *title = "GAME OVER";
    if (state == SEALS_WON)
      title = "VICTOIRE";
    DrawCentered(a, title, 40, a.y + a.height / 2 - 20, WHITE);
    DrawCentered(a, "ENTREE pour continuer", 20, a.y + a.height / 2 + 20, GRAY);
  }
}

void SealsUpdate(Rectangle a) {
  if (state == SEALS_OFF)
    return;

  float smallSide = a.width;
  if (a.height < smallSide)
    smallSide = a.height;
  float k = smallSide / 400;

  if (!ready) {
    player = Center(a);
    PlaceRune(a, k);
    ready = true;
  }

  if (state == SEALS_PLAYING)
    Update(a, k);
  if (state != SEALS_PLAYING && state != SEALS_OFF && IsKeyPressed(KEY_ENTER))
    state = SEALS_OFF;

  Draw(a, k);
}

SealsStatus SealsState(void) {
  return state;
}

int SealsDamageCount(void) {
  return damageCount;
}

void SealsUnload(void) {
  UnloadTexture(heartTex);
  heartTex = (Texture2D){0};
  UnloadTexture(sealTex);
  sealTex = (Texture2D){0};
  UnloadTexture(blastTex);
  blastTex = (Texture2D){0};
  UnloadTexture(shadowTex);
  shadowTex = (Texture2D){0};
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 2; j++) {
      UnloadTexture(sprites[i][j]);
      sprites[i][j] = (Texture2D){0};
    }
  }
  ready = false;
  state = SEALS_OFF;
}
