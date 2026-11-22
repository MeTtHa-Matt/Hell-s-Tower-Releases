#include "boss4.h"
#include "../balance.h"
#include "../../items/functionnal/functionnal.h"
#include "../../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

/* Chemin de l'image du boss (renommez votre fichier sans accents). */
#define BOSS_TEXTURE_PATH "assets/boss/marika.png"

#define MAX_HAZARDS 64
#define MAX_BULLETS 260
#define DURATION 140.0f
#define TOTAL_SHARDS 12      /* 3 phases x 4 éclats */
#define PHASE_SHARDS 4
#define ACTIVE 0.35f
#define SPEAR_EXTEND 0.1f
#define DASH_TIME 0.22f
#define DASH_COOLDOWN 1.2f
#define INTRO_TIME 3.4f
#define TRANSITION_TIME 2.6f
#define DYING_TIME 3.8f
#define BURST_TIME 1.6f

enum { H_SPEAR, H_TILE, H_SPIN, H_BLAST };
enum { A_SPEARS, A_RUNES, A_TILES, A_WHEEL, A_SHATTER };
enum { FRONT, BACK, LEFT, RIGHT };

typedef struct {
  int kind;
  Vector2 pos;
  float angle, size, width, warn, t, life, speed, sign;
  int arms;
  bool on, hit, fired;
} Hazard;

typedef struct {
  Vector2 pos, vel;
  float rot;
  bool on;
} Bullet;

static const char *dirNames[4] = {"front", "back", "left", "right"};
static const char *footNames[2] = {"left", "right"};
static const char *phaseTitles[3] = {"PHASE I", "PHASE II", "PHASE III"};
static const char *phaseNames[3] = {"L'ORDRE DORE", "LA ROUE D'ELDEN",
                                    "LA BRISURE"};

static Hazard hazards[MAX_HAZARDS];
static Bullet bullets[MAX_BULLETS];
static Texture2D heartTex, shadowTex, bossTex, sprites[4][2];
static Rectangle arena;
static float unit;
static Vector2 player, dir, bossPos, burstPos, shardPos;
static float bossTargetX;
static bool ready, moving, burstBig, shardOn;
static Boss4Status state;
static int floorNumber, difficulty, lives, maxLives, damageCount, collected,
    sealWards, facing, lastAttack, streamArms;
static float timeLeft, attackTimer, invincible, step, dashTime, dashCooldown,
    introTime, transition, dying, shake, flash, bossFlash, burstT, shownHp,
    clock, streamLeft, streamTimer, streamAngle, streamSpin, shardTheta,
    shardSpeed, shardRx, shardRy, shardCx, shardCy, shardAge, shardDelay;

/* ============================== utilitaires ============================== */

static float Limit(float value, float min, float max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

static float Clamp01(float v) { return Limit(v, 0, 1); }

static float RandRange(float lo, float hi) {
  return lo + GetRandomValue(0, 1000) / 1000.0f * (hi - lo);
}

/* pseudo-aléatoire déterministe pour les effets visuels */
static float Rand01(int n) {
  unsigned x = (unsigned)n * 747796405u + 2891336453u;
  x = ((x >> ((x >> 28) + 4)) ^ x) * 277803737u;
  x = (x >> 22) ^ x;
  return (x & 0xFFFF) / 65535.0f;
}

static Vector2 Polar(Vector2 c, float angle, float dist) {
  return (Vector2){c.x + cosf(angle) * dist, c.y + sinf(angle) * dist};
}

static int Phase(void) {
  int p = collected / PHASE_SHARDS;
  return p > 2 ? 2 : p;
}

static float Intensity(void) {
  return BalanceBossIntensity(floorNumber, difficulty);
}

static void DrawCentered(Rectangle a, const char *text, int size, float y,
                         Color color) {
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

/* Temps d'avertissement (jamais en dessous de 0.3 s). */
static float Warn(float factor) {
  const FunctionalSettings *items = FunctionalGet();
  float bonus = items->sealWarning;
  if (difficulty >= 7 && items->sealHighAid)
    bonus += 0.12f;
  return fmaxf(0.3f, BalanceSealWarning(difficulty, bonus) * factor);
}

static float MaxReach(void) {
  return hypotf(arena.width, arena.height) * 1.1f;
}

/* ============================== dégâts ============================== */

static void Hurt(void) {
  const FunctionalSettings *items = FunctionalGet();
  if (FunctionalBlockDamageAtLives(lives))
    return;
  if (items->sealWard && sealWards > 0) {
    sealWards--;
    return;
  }
  invincible = 1.5f + items->invincibility;
  damageCount++;
  lives--;
  shake = fmaxf(shake, 9 * unit);
  if (lives <= 0)
    state = BOSS4_LOST;
}

/* ============================== création ============================== */

static Hazard *AddHazard(int kind, Vector2 pos, float angle, float size,
                         float width, float warn) {
  for (int i = 0; i < MAX_HAZARDS; i++) {
    if (hazards[i].on)
      continue;
    hazards[i] = (Hazard){0};
    hazards[i].kind = kind;
    hazards[i].pos = pos;
    hazards[i].angle = angle;
    hazards[i].size = size;
    hazards[i].width = width;
    hazards[i].warn = warn;
    hazards[i].life = ACTIVE;
    hazards[i].sign = 1;
    hazards[i].on = true;
    return &hazards[i];
  }
  return NULL;
}

static void AddBullet(Vector2 pos, Vector2 vel) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].on)
      continue;
    bullets[i] = (Bullet){pos, vel, RandRange(0, 90), true};
    return;
  }
}

static float AimAngle(Vector2 from, float lead) {
  Vector2 t = player;
  if (moving) {
    t.x += dir.x * lead;
    t.y += dir.y * lead;
  }
  return atan2f(t.y - from.y, t.x - from.x);
}

/* ============================== attaques ============================== */

/* Lances de lumière : traits dorés qui transpercent l'arène. */
static void AtkSpears(void) {
  int p = Phase();
  int n = 5 + p;
  int order[16];
  for (int i = 0; i < n; i++)
    order[i] = i;
  for (int i = n - 1; i > 0; i--) {   /* ordre d'impact mélangé */
    int j = GetRandomValue(0, i);
    int tmp = order[i];
    order[i] = order[j];
    order[j] = tmp;
  }
  for (int i = 0; i < n; i++) {
    float x = arena.x + arena.width * (i + RandRange(0.15f, 0.85f)) / n;
    Hazard *h = AddHazard(H_SPEAR, (Vector2){x, arena.y}, PI / 2, arena.height,
                          12 * unit, Warn(1.0f));
    if (h)
      h->t = -order[i] * 0.13f;
  }
  if (p >= 1) {                        /* lances horizontales */
    int m = 2 + (p >= 2);
    for (int j = 0; j < m; j++) {
      bool right = j % 2;
      float y = arena.y + arena.height * RandRange(0.3f, 0.95f);
      Hazard *h = AddHazard(H_SPEAR,
                            (Vector2){right ? arena.x + arena.width : arena.x, y},
                            right ? PI : 0, arena.width, 12 * unit, Warn(1.0f));
      if (h)
        h->t = -0.3f - j * 0.25f;
    }
  }
  if (p >= 2) {                        /* une lance visée sur le joueur */
    Hazard *h = AddHazard(H_SPEAR, bossPos, AimAngle(bossPos, 60 * unit),
                          MaxReach(), 14 * unit, Warn(1.0f));
    if (h)
      h->t = -0.6f;
  }
}

/* Souffle de runes : spirale de losanges dorés. */
static void AtkRunes(void) {
  streamLeft = 2.4f;
  streamTimer = 0;
  streamArms = 3 + Phase();
  streamAngle = RandRange(0, 6.28f);
  streamSpin = (GetRandomValue(0, 1) ? 1 : -1) * 0.22f;
}

static void SpawnStreamTick(void) {
  const FunctionalSettings *items = FunctionalGet();
  float lo = BalanceRandomSpeedMin(floorNumber, difficulty, 125.0f);
  float hi = BalanceRandomSpeedMax(floorNumber, difficulty, 125.0f);
  float speed = RandRange(lo, hi) * unit * items->bulletSpeed;
  for (int i = 0; i < streamArms; i++) {
    float a = streamAngle + i * 2 * PI / streamArms;
    AddBullet(Polar(bossPos, a, 26 * unit),
              (Vector2){cosf(a) * speed, sinf(a) * speed});
  }
  streamAngle += streamSpin;
}

/* Dalles de l'Ordre : une grille dont certaines cases s'embrasent. */
static void AtkTiles(void) {
  int cols = 6, rows = 4;
  float tw = arena.width / cols, th = arena.height / rows;
  float prob = 0.38f + 0.07f * Phase();
  float warn = Warn(1.3f);
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++) {
      if (RandRange(0, 1) >= prob)
        continue;
      Hazard *h = AddHazard(H_TILE, (Vector2){arena.x + c * tw, arena.y + r * th},
                            0, tw, th, warn);
      if (h)
        h->t = -c * 0.05f;   /* l'embrasement se propage de gauche à droite */
    }
}

/* Roue d'Elden : bras rotatifs au centre de l'arène. */
static void AtkWheel(void) {
  Vector2 c = {arena.x + arena.width / 2, arena.y + arena.height * 0.55f};
  Hazard *h = AddHazard(H_SPIN, c, RandRange(0, 6.28f), MaxReach(), 14 * unit,
                        Warn(1.2f));
  if (!h)
    return;
  h->arms = Phase() >= 2 ? 4 : 3;
  h->speed = (0.75f + 0.15f * Phase()) * Intensity();
  h->life = 3.2f;
}

/* Brisure : éclats de l'anneau qui tombent + lances en croix. */
static void AtkShatter(void) {
  for (int i = 0; i < 6; i++) {
    Vector2 p;
    if (i == 0)
      p = (Vector2){player.x + RandRange(-60, 60) * unit,
                    player.y + RandRange(-60, 60) * unit};
    else
      p = (Vector2){arena.x + RandRange(0.08f, 0.92f) * arena.width,
                    arena.y + RandRange(0.3f, 0.95f) * arena.height};
    AddHazard(H_BLAST, p, 0, RandRange(40, 65) * unit, 0, Warn(1.35f));
  }
  Hazard *v = AddHazard(H_SPEAR, (Vector2){player.x, arena.y}, PI / 2,
                        arena.height, 14 * unit, Warn(1.0f));
  Hazard *hz = AddHazard(H_SPEAR, (Vector2){arena.x, player.y}, 0, arena.width,
                         14 * unit, Warn(1.0f));
  if (v)
    v->t = -0.4f;
  if (hz)
    hz->t = -0.4f;
}

static void LaunchAttack(void) {
  const FunctionalSettings *items = FunctionalGet();
  static const int list0[] = {A_SPEARS, A_RUNES, A_TILES, A_SPEARS};
  static const int list1[] = {A_SPEARS, A_RUNES, A_TILES, A_WHEEL, A_SPEARS};
  static const int list2[] = {A_SPEARS, A_RUNES, A_TILES, A_WHEEL, A_SHATTER};
  int p = Phase();
  const int *list = p == 0 ? list0 : (p == 1 ? list1 : list2);
  int n = p == 0 ? 4 : 5;

  int attack = list[GetRandomValue(0, n - 1)];
  for (int tries = 0; tries < 3 && attack == lastAttack; tries++)
    attack = list[GetRandomValue(0, n - 1)];
  lastAttack = attack;

  float extra = 0;
  switch (attack) {
  case A_SPEARS:  AtkSpears(); extra = (5 + p) * 0.13f + 0.6f; break;
  case A_RUNES:   AtkRunes(); extra = 2.4f; break;
  case A_TILES:   AtkTiles(); extra = 1.0f; break;
  case A_WHEEL:   AtkWheel(); extra = 4.6f; break;
  case A_SHATTER: AtkShatter(); extra = 1.0f; break;
  }
  /* l'éclat est parfois pris pour cible */
  if (shardOn && GetRandomValue(0, 3) == 0)
    AddHazard(H_BLAST, shardPos, 0, 60 * unit, 0, Warn(1.0f));

  attackTimer = ((2.3f - 0.3f * p) / Intensity() + extra) * items->sealSpawn;
}

/* ============================== éclats d'anneau ============================== */

static void SpawnShard(void) {
  shardCx = arena.x + arena.width / 2;
  shardCy = arena.y + arena.height * 0.62f;
  shardRx = arena.width * RandRange(0.18f, 0.36f);
  shardRy = arena.height * RandRange(0.12f, 0.22f);
  shardTheta = RandRange(0, 6.28f);
  shardSpeed = RandRange(0.35f, 0.6f) * (1 + 0.25f * Phase()) *
               (GetRandomValue(0, 1) ? 1 : -1);
  shardAge = 0;
  shardOn = true;
  shardPos = (Vector2){shardCx + cosf(shardTheta) * shardRx,
                       shardCy + sinf(shardTheta) * shardRy};
}

static void CollectShard(void) {
  const FunctionalSettings *items = FunctionalGet();
  burstPos = shardPos;
  burstT = 0;
  burstBig = false;
  collected++;
  if (items->boss4ShardHeal && collected % 3 == 0 && lives < maxLives)
    lives++;
  if (items->sealWard && collected % 2 == 0)
    sealWards++;
  shardOn = false;
  shardDelay = 0.8f;
  bossFlash = 1;
  flash = 0.3f;
  shake = fmaxf(shake, 11 * unit);
  streamLeft = 0;
  memset(hazards, 0, sizeof(hazards));
  memset(bullets, 0, sizeof(bullets));

  if (collected >= TOTAL_SHARDS) {            /* mort du boss */
    dying = DYING_TIME;
    burstBig = true;
    flash = 0.9f;
  } else if (collected % PHASE_SHARDS == 0) { /* changement de phase */
    transition = TRANSITION_TIME;
    burstBig = true;
    flash = 0.8f;
    attackTimer = TRANSITION_TIME + 1.0f;
  } else {
    attackTimer = 1.2f * items->sealSpawn;
  }
}

/* ============================== démarrage ============================== */

void Boss4Start(int floorNumberArg, int d) {
  if (heartTex.id == 0)
    heartTex = LoadTexture("assets/heart.png");
  if (shadowTex.id == 0)
    shadowTex = LoadTexture("assets/player/shadow.png");
  if (bossTex.id == 0) {
    bossTex = LoadTexture(BOSS_TEXTURE_PATH);
    if (bossTex.id != 0) {   /* grande image réduite : mipmaps = rendu propre */
      GenTextureMipmaps(&bossTex);
      SetTextureFilter(bossTex, TEXTURE_FILTER_TRILINEAR);
    }
  }
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 2; j++)
      if (sprites[i][j].id == 0)
        sprites[i][j] = LoadTexture(TextFormat("assets/player/%s_%s_feet.png",
                                               dirNames[i], footNames[j]));

  floorNumber = floorNumberArg;
  difficulty = (int)Limit((float)d, 1, 10);

  const FunctionalSettings *items = FunctionalGet();
  lives = maxLives = 3 + items->extraLives;
  if (lives < 1)
    lives = maxLives = 1;
  damageCount = 0;
  collected = 0;
  sealWards = 0;
  state = BOSS4_PLAYING;
  timeLeft = DURATION + items->duration;
  attackTimer = 0.5f;
  invincible = step = dashTime = dashCooldown = 0;
  introTime = transition = dying = shake = flash = bossFlash = 0;
  streamLeft = streamTimer = 0;
  shardOn = false;
  shardDelay = 0.3f;
  burstT = -1;
  burstBig = false;
  shownHp = 1;
  lastAttack = -1;
  moving = false;
  dir = (Vector2){0, 1};
  facing = FRONT;
  ready = false;
  memset(hazards, 0, sizeof(hazards));
  memset(bullets, 0, sizeof(bullets));
}

/* ============================== mise à jour ============================== */

static void MovePlayer(float dt) {
  const FunctionalSettings *items = FunctionalGet();
  float margin = 28 * unit;
  float dirX = 0, dirY = 0;

  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
    dirX += 1;
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
    dirX -= 1;
  if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
    dirY += 1;
  if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
    dirY -= 1;

  float length = hypotf(dirX, dirY);
  moving = length > 0;
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
    dashCooldown = DASH_COOLDOWN * items->sealDashCooldown;
  }

  float speed = 200 * unit * items->playerSpeed;
  if (dashTime > 0)
    speed = 650 * unit;
  if (dashTime > 0 || length > 0) {
    player.x += dir.x * speed * dt;
    player.y += dir.y * speed * dt;
  }
  player.x = Limit(player.x, arena.x + margin, arena.x + arena.width - margin);
  player.y = Limit(player.y, arena.y + margin, arena.y + arena.height - margin);
}

static float SpearExtent(Hazard *h) {
  return h->t >= h->warn ? Clamp01((h->t - h->warn) / SPEAR_EXTEND) : 0;
}

static bool Touches(Hazard *h) {
  float r = 10 * unit * FunctionalGet()->hitbox;

  switch (h->kind) {
  case H_SPEAR: {
    Vector2 tip = Polar(h->pos, h->angle, h->size * SpearExtent(h));
    return NearSegment(player, h->pos, tip, h->width / 2 + r);
  }
  case H_TILE:
    return player.x > h->pos.x - r && player.x < h->pos.x + h->size + r &&
           player.y > h->pos.y - r && player.y < h->pos.y + h->width + r;
  case H_SPIN:
    for (int i = 0; i < h->arms; i++) {
      Vector2 end = Polar(h->pos, h->angle + i * 2 * PI / h->arms, h->size);
      if (NearSegment(player, h->pos, end, h->width / 2 + r))
        return true;
    }
    return false;
  case H_BLAST:
    return hypotf(player.x - h->pos.x, player.y - h->pos.y) < h->size + r;
  }
  return false;
}

static void UpdateHazard(Hazard *h, float dt) {
  h->t += dt;
  if (h->t < 0)
    return;
  if (h->t > h->warn + h->life) {
    h->on = false;
    return;
  }
  if (h->t >= h->warn) {
    if (!h->fired) {
      h->fired = true;
      shake = fmaxf(shake, (h->kind == H_SPIN ? 5 : (h->kind == H_TILE ? 2.5f : 4)) * unit);
    }
    if (h->kind == H_SPIN) {          /* la roue s'inverse à mi-course (phase III) */
      float since = h->t - h->warn;
      h->sign = (h->arms >= 4 && since > h->life * 0.5f) ? -1.0f : 1.0f;
      h->angle += h->speed * h->sign * dt;
    }
  }
  /* la roue peut toucher plusieurs fois, les autres dangers une seule */
  if (h->t < h->warn || (h->kind != H_SPIN && h->hit) || invincible > 0 ||
      dashTime > 0 || transition > 0 || dying > 0 || !Touches(h))
    return;
  h->hit = true;
  Hurt();
}

static void UpdateBullets(float dt) {
  const FunctionalSettings *items = FunctionalGet();
  float radius = 6 * unit * items->bulletSize + 9 * unit * items->hitbox;
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (!b->on)
      continue;
    b->pos.x += b->vel.x * dt;
    b->pos.y += b->vel.y * dt;
    b->rot += 120 * dt;
    float out = 40 * unit;
    if (b->pos.x < arena.x - out || b->pos.x > arena.x + arena.width + out ||
        b->pos.y < arena.y - out || b->pos.y > arena.y + arena.height + out) {
      b->on = false;
      continue;
    }
    if (invincible > 0 || dashTime > 0 || transition > 0 || dying > 0)
      continue;
    if (hypotf(player.x - b->pos.x, player.y - b->pos.y) < radius) {
      b->on = false;
      Hurt();
    }
  }
}

static void UpdateShard(float dt) {
  const FunctionalSettings *items = FunctionalGet();
  if (shardOn) {
    shardAge += dt;
    shardTheta += shardSpeed * dt;
    shardPos = (Vector2){shardCx + cosf(shardTheta) * shardRx,
                         shardCy + sinf(shardTheta) * shardRy};
    if (shardAge > 0.3f &&
        hypotf(player.x - shardPos.x, player.y - shardPos.y) <
            26 * unit + 10 * unit * items->hitbox)
      CollectShard();
  } else if (dying <= 0 && transition <= 0) {
    shardDelay -= dt;
    if (shardDelay <= 0)
      SpawnShard();
  }
}

static void Update(float dt) {
  invincible -= dt;
  dashTime -= dt;
  dashCooldown -= dt;
  shake *= expf(-dt * 6);
  flash = fmaxf(0, flash - dt * 1.5f);
  bossFlash = fmaxf(0, bossFlash - dt * 2.5f);
  if (burstT >= 0) {
    burstT += dt;
    if (burstT > BURST_TIME)
      burstT = -1;
  }
  shownHp += ((1.0f - (float)collected / TOTAL_SHARDS) - shownHp) * fminf(1, dt * 4);

  /* le boss flotte au centre haut, avec un léger balancement */
  bossPos.y = arena.y + arena.height * 0.28f;
  bossTargetX = arena.x + arena.width / 2 + sinf(clock * 0.5f) * arena.width * 0.12f;
  bossPos.x += (bossTargetX - bossPos.x) * fminf(1, dt * 2);

  MovePlayer(dt);

  if (dying > 0) {                      /* mort du boss : outro */
    dying -= dt;
    shake = fmaxf(shake, 3 * unit);
    if (dying <= 0)
      state = BOSS4_WON;
    return;
  }
  if (introTime < INTRO_TIME) {
    introTime += dt;
    return;
  }

  timeLeft -= dt;
  UpdateShard(dt);

  if (dying <= 0 && transition > 0) {
    transition -= dt;
  } else if (dying <= 0) {
    if (streamLeft > 0) {
      streamLeft -= dt;
      streamTimer -= dt;
      if (streamTimer <= 0) {
        SpawnStreamTick();
        streamTimer = 0.13f / Intensity();
      }
    }
    attackTimer -= dt;
    if (attackTimer <= 0 && streamLeft <= 0)
      LaunchAttack();
  }

  for (int i = 0; i < MAX_HAZARDS; i++)
    if (hazards[i].on)
      UpdateHazard(&hazards[i], dt);
  UpdateBullets(dt);

  if (timeLeft <= 0 && state == BOSS4_PLAYING)
    state = BOSS4_LOST;
}

/* ============================== rendu ============================== */

static void DrawPlayerAt(Vector2 p, Color tint, bool withShadow) {
  Texture2D tex = sprites[facing][(int)(step / 0.18f) % 2];
  if (tex.id == 0)
    return;
  float height = 56 * unit;
  float width = height * tex.width / tex.height;

  if (withShadow && shadowTex.id != 0) {
    float sw = 56 * unit;
    float sh = sw * shadowTex.height / shadowTex.width;
    Rectangle src = {0, 0, (float)shadowTex.width, (float)shadowTex.height};
    Rectangle dst = {p.x - sw / 2, p.y + 16 * unit - sh / 2, sw, sh};
    DrawTexturePro(shadowTex, src, dst, (Vector2){0, 0}, 0, WHITE);
  }
  Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
  Rectangle dst = {p.x - width / 2, p.y - height / 2, width, height};
  DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0, tint);
}

static void DrawHeart(Vector2 c, float size) {
  Rectangle src = {0, 0, (float)heartTex.width, (float)heartTex.height};
  Rectangle dst = {c.x - size / 2, c.y - size / 2, size, size};
  DrawTexturePro(heartTex, src, dst, (Vector2){0, 0}, 0, WHITE);
}

/* Halo de l'Anneau d'Elden : se fissure en phase III, éclate à la mort. */
static void DrawHalo(Vector2 boss, float h, float alpha) {
  Color gold = {255, 205, 100, 255};
  Vector2 c = {boss.x, boss.y - h * 0.24f};
  float radius = h * 0.6f;
  int phase = Phase();
  float breakAmt = dying > 0 ? (DYING_TIME - dying) : 0;

  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 0; i < 12; i++) {
    float a0 = clock * (8 + 6 * phase) + i * 30;
    float a1 = a0 + 26;
    float mid = (a0 + a1) / 2 * DEG2RAD;
    float off = breakAmt * breakAmt * 60 * unit;
    if (phase >= 2 && i % 3 == 0)
      off += (6 + 4 * sinf(clock * 3 + i)) * unit;
    Vector2 cc = Polar(c, mid, off);
    DrawRing(cc, radius * 0.96f, radius, a0, a1, 8, Fade(gold, 0.4f * alpha));
    DrawRing(cc, radius * 0.82f, radius * 0.84f, a0, a1, 8, Fade(gold, 0.2f * alpha));
    DrawLineEx(Polar(cc, mid, radius), Polar(cc, mid, radius * 1.12f), 2 * unit,
               Fade(gold, 0.5f * alpha));
  }
  EndBlendMode();
}

static void DrawBoss(void) {
  int phase = Phase();
  float h = arena.height * 0.5f;
  float w = bossTex.id ? h * bossTex.width / bossTex.height : h * 0.9f;

  float introP = Clamp01(introTime / INTRO_TIME);
  float ease = 1 - powf(1 - introP, 3);
  float alpha = Clamp01(introTime / 1.8f);
  if (dying > 0 && dying < DYING_TIME - 1.2f)
    alpha = Clamp01(dying / (DYING_TIME - 1.2f));
  float rise = (1 - ease) * arena.height * 0.12f;
  float tremble = dying > 0 ? sinf(clock * 80) * 4 * unit : 0;
  tremble += bossFlash * sinf(clock * 90) * 3 * unit;

  Vector2 c = {bossPos.x + tremble,
               bossPos.y + sinf(clock * 1.4f) * 6 * unit - rise};

  DrawHalo(c, h, alpha);

  BeginBlendMode(BLEND_ADDITIVE);   /* aura dorée, de plus en plus ardente */
  for (int l = 0; l < 6; l++)
    DrawCircleV((Vector2){c.x, c.y + h * 0.05f}, h * (0.22f + 0.1f * l),
                Fade((Color){255, 170, 60, 255}, (0.03f + 0.02f * phase) * alpha));
  for (int i = 0; i < 20; i++) {    /* poussière de grâce qui monte */
    float ph = fmodf(clock * 0.28f + Rand01(i * 3), 1.0f);
    DrawCircleV((Vector2){c.x + (Rand01(i * 3 + 1) - 0.5f) * w,
                          c.y + h * 0.4f - ph * h * 0.9f},
                (1 + 2 * Rand01(i * 3 + 2)) * unit,
                Fade((Color){255, 220, 130, 255}, (1 - ph) * 0.8f * alpha));
  }
  EndBlendMode();

  if (bossTex.id == 0) {
    DrawCircleV(c, h * 0.3f, Fade((Color){150, 20, 30, 255}, alpha));
    return;
  }
  float f = bossFlash;
  Color tint = {255, (unsigned char)(255 - 110 * f), (unsigned char)(255 - 110 * f),
                (unsigned char)(255 * alpha)};
  Rectangle src = {0, 0, (float)bossTex.width, (float)bossTex.height};
  Rectangle dst = {c.x - w / 2, c.y - h / 2, w, h};
  DrawTexturePro(bossTex, src, dst, (Vector2){0, 0}, 0, tint);

  if (bossFlash > 0) {
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(bossTex, src, dst, (Vector2){0, 0}, 0,
                   Fade(WHITE, bossFlash * 0.5f * alpha));
    EndBlendMode();
  }
}

static void DrawShard(void) {
  if (!shardOn)
    return;
  Color gold = {255, 215, 120, 255};
  float appear = Clamp01(shardAge / 0.5f);
  float pulse = 0.5f + 0.5f * sinf(clock * 5);

  /* orbite en filigrane, pour deviner la trajectoire */
  DrawEllipseLines((int)shardCx, (int)shardCy, shardRx, shardRy, Fade(gold, 0.12f));

  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 1; i <= 8; i++) {      /* traînée */
    float th = shardTheta - shardSpeed * i * 0.05f;
    Vector2 p = {shardCx + cosf(th) * shardRx, shardCy + sinf(th) * shardRy};
    DrawCircleV(p, (8 - i) * 0.9f * unit, Fade(gold, 0.35f * (1 - i / 9.0f)));
  }
  DrawCircleV(shardPos, 34 * unit * appear, Fade(gold, 0.15f + 0.12f * pulse));
  for (int i = 0; i < 6; i++) {       /* étincelles en orbite */
    float a = clock * 2.5f + i * PI / 3;
    DrawCircleV(Polar(shardPos, a, 22 * unit * appear), 2 * unit, Fade(WHITE, 0.8f));
  }
  EndBlendMode();
  DrawCircleLinesV(shardPos, (26 + 4 * pulse) * unit * appear, Fade(WHITE, 0.7f));
  DrawPoly(shardPos, 4, 15 * unit * appear, clock * 90, gold);
  DrawPolyLines(shardPos, 4, 15 * unit * appear, clock * 90, WHITE);
}

static void DrawHazard(Hazard *h) {
  if (h->t < 0)
    return;
  Color gold = {255, 215, 120, 255};
  Color crimson = {200, 30, 45, 255};
  bool firing = h->t >= h->warn;
  float p = Clamp01(h->t / h->warn);
  float since = h->t - h->warn;
  float u = Clamp01(since / h->life), al = 1 - u;

  switch (h->kind) {
  case H_SPEAR: {
    if (!firing) {
      Vector2 end = Polar(h->pos, h->angle, h->size);
      DrawLineEx(h->pos, end, (2 + 2 * p) * unit, Fade(gold, 0.35f + 0.4f * p));
      DrawCircleV(h->pos, 5 * unit * p, Fade(gold, 0.8f));
    } else {
      Vector2 tip = Polar(h->pos, h->angle, h->size * SpearExtent(h));
      BeginBlendMode(BLEND_ADDITIVE);
      DrawLineEx(h->pos, tip, h->width * 2.4f, Fade(gold, 0.4f * al));
      DrawCircleV(tip, h->width * 1.6f, Fade(WHITE, al));
      EndBlendMode();
      DrawLineEx(h->pos, tip, h->width, Fade(WHITE, al));
    }
    break;
  }
  case H_TILE: {
    Rectangle r = {h->pos.x, h->pos.y, h->size, h->width};
    if (!firing) {
      DrawRectangleRec(r, Fade(gold, 0.05f + 0.2f * p));
      DrawRectangleLinesEx(r, 2 * unit, Fade(gold, 0.35f + 0.5f * p));
    } else {
      DrawRectangleRec(r, Fade(WHITE, al));
      BeginBlendMode(BLEND_ADDITIVE);
      DrawRectangleRec((Rectangle){r.x - 4 * unit, r.y - 4 * unit, r.width + 8 * unit,
                                   r.height + 8 * unit},
                       Fade(gold, 0.5f * al));
      for (int i = 0; i < 5; i++)     /* étincelles qui montent de la dalle */
        DrawCircleV((Vector2){r.x + r.width * Rand01(i + (int)(r.x + r.y)),
                              r.y + r.height * (1 - u * (0.5f + Rand01(i + 9)))},
                    2 * unit, Fade(WHITE, al));
      EndBlendMode();
    }
    break;
  }
  case H_SPIN: {
    float fadeOut = firing ? Clamp01((h->life - since) / 0.3f) : 1;
    for (int i = 0; i < h->arms; i++) {
      Vector2 end = Polar(h->pos, h->angle + i * 2 * PI / h->arms, h->size);
      if (!firing) {
        DrawLineEx(h->pos, end, (2 + 2 * p) * unit, Fade(gold, 0.3f + 0.4f * p));
      } else {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawLineEx(h->pos, end, h->width * 2.4f, Fade(gold, 0.4f * fadeOut));
        EndBlendMode();
        DrawLineEx(h->pos, end, h->width, Fade(WHITE, fadeOut));
      }
    }
    BeginBlendMode(BLEND_ADDITIVE);   /* moyeu de la roue */
    DrawCircleV(h->pos, (18 + 14 * p) * unit, Fade(crimson, 0.5f * fadeOut));
    DrawRing(h->pos, 30 * unit, 34 * unit, 0, 360, 48, Fade(gold, 0.8f * fadeOut));
    EndBlendMode();
    break;
  }
  case H_BLAST: {
    if (!firing) {
      DrawCircleV(h->pos, h->size * p, Fade(gold, 0.3f));
      DrawCircleLinesV(h->pos, h->size, Fade(gold, 0.9f));
    } else {
      float r = h->size * (0.85f + 0.25f * u);
      BeginBlendMode(BLEND_ADDITIVE);
      DrawCircleV(h->pos, r, Fade(gold, 0.6f * al));
      for (int i = 0; i < 12; i++) {  /* pointes de l'Anneau brisé */
        float a = clock * 1.2f + i * 2 * PI / 12;
        DrawLineEx(Polar(h->pos, a, r * 0.3f), Polar(h->pos, a, r * 1.3f),
                   3 * unit, Fade(WHITE, al));
      }
      DrawCircleV(h->pos, r * 0.45f, Fade(WHITE, 0.8f * al));
      EndBlendMode();
      DrawRing(h->pos, r * 0.95f, r * 1.05f, 0, 360, 48, Fade(crimson, al));
    }
    break;
  }
  }
}

static void DrawBullets(void) {
  const FunctionalSettings *items = FunctionalGet();
  float size = 8 * unit * items->bulletSize;
  Color gold = {255, 215, 120, 255};
  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 0; i < MAX_BULLETS; i++)
    if (bullets[i].on)
      DrawCircleV(bullets[i].pos, size * 1.9f, Fade(gold, 0.25f));
  EndBlendMode();
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].on)
      continue;
    DrawPoly(bullets[i].pos, 4, size, bullets[i].rot, gold);
    DrawPolyLines(bullets[i].pos, 4, size, bullets[i].rot, (Color){200, 30, 45, 255});
  }
}

static void DrawBurst(void) {
  if (burstT < 0)
    return;
  Color gold = {255, 215, 120, 255};
  float u = burstT / BURST_TIME, fade = 1 - u;
  BeginBlendMode(BLEND_ADDITIVE);
  float rr = unit * (20 + u * 260);
  DrawRing(burstPos, rr - 5 * unit, rr + 5 * unit, 0, 360, 64, Fade(gold, fade));
  for (int i = 0; i < 60; i++) {
    float a = Rand01(i * 2) * 2 * PI;
    float sp = (120 + 380 * Rand01(i * 2 + 1)) * unit;
    Vector2 p = Polar(burstPos, a, sp * burstT * (1.1f - 0.4f * burstT));
    p.y += 200 * unit * burstT * burstT;
    DrawCircleV(p, (1.5f + 2.5f * Rand01(i)) * unit * fade,
                Fade(i % 3 == 0 ? WHITE : gold, fade));
  }
  if (burstBig) {
    float r2 = unit * (30 + u * 520);
    DrawRing(bossPos, r2 - 8 * unit, r2 + 8 * unit, 0, 360, 96,
             Fade((Color){220, 40, 50, 255}, fade));
    for (int i = 0; i < 90; i++) {
      float a = Rand01(i * 5 + 7) * 2 * PI;
      float sp = (150 + 450 * Rand01(i * 5 + 8)) * unit;
      Vector2 p = Polar(bossPos, a, sp * burstT * (1.1f - 0.4f * burstT));
      p.y += 160 * unit * burstT * burstT;
      DrawCircleV(p, (2 + 3 * Rand01(i + 9)) * unit * fade,
                  Fade(i % 2 ? (Color){255, 90, 60, 255} : gold, fade));
    }
  }
  EndBlendMode();
}

static void DrawArena(void) {
  int phase = Phase();
  Color gold = {255, 205, 100, 255};

  DrawRectangleGradientV((int)arena.x, (int)arena.y, (int)arena.width,
                         (int)arena.height, Fade(BLACK, 0),
                         Fade((Color){85, 20, 15, 255}, 0.18f + 0.1f * phase));

  /* sceau d'Ordre doré gravé au sol, qui tourne de plus en plus vite */
  Vector2 sigil = {arena.x + arena.width / 2, arena.y + arena.height * 0.62f};
  float spin = clock * (0.15f + 0.1f * phase);
  BeginBlendMode(BLEND_ADDITIVE);
  DrawCircleLinesV(sigil, 150 * unit, Fade(gold, 0.10f));
  DrawCircleLinesV(sigil, 110 * unit, Fade(gold, 0.08f));
  for (int i = 0; i < 24; i++) {
    float a = spin + i * 2 * PI / 24;
    DrawLineEx(Polar(sigil, a, 140 * unit), Polar(sigil, a, (i % 3 ? 150 : 162) * unit),
               2 * unit, Fade(gold, 0.12f));
  }
  EndBlendMode();

  /* grains dorés et pétales pourpres */
  for (int i = 0; i < 30; i++) {
    float ph = fmodf(clock * (0.05f + 0.07f * Rand01(i * 4)) + Rand01(i * 4 + 1), 1);
    float x = arena.x + arena.width * Rand01(i * 4 + 2) + sinf(clock * 1.3f + i) * 18 * unit;
    float y = arena.y + arena.height * (i % 2 ? ph : 1 - ph);
    float size = (1.5f + 2.5f * Rand01(i * 4 + 3)) * unit;
    Color col = i % 3 ? (Color){255, 215, 120, 255} : (Color){150, 25, 40, 255};
    DrawEllipse((int)x, (int)y, size * 1.5f, size, Fade(col, 0.4f));
  }

  DrawBoss();

  for (int i = 0; i < MAX_HAZARDS; i++)
    if (hazards[i].on)
      DrawHazard(&hazards[i]);
  DrawShard();
  DrawBullets();

  /* joueur : images rémanentes pendant l'esquive */
  if (dashTime > 0)
    for (int g = 3; g >= 1; g--)
      DrawPlayerAt((Vector2){player.x - dir.x * g * 16 * unit,
                             player.y - dir.y * g * 16 * unit},
                   Fade(WHITE, 0.18f), false);
  bool visible = invincible <= 0 || (int)(invincible * 10) % 2 == 0;
  if (visible)
    DrawPlayerAt(player, dashTime > 0 ? Fade(WHITE, 0.4f) : WHITE, true);

  DrawBurst();
}

static void DrawHud(Rectangle a) {
  float ui = GameUiScale();
  int hudSize = GameUiPixels(20);

  for (int i = 0; i < lives; i++)
    DrawHeart((Vector2){a.x + (16 + i * 28) * ui, a.y + 16 * ui}, 20 * ui);

  /* barre de vie du boss, avec repères de phase */
  DrawCentered(a, "DAME MARIKA, CELLE QUI REGNA", 16, a.y + GameUiPixels(6),
               (Color){241, 213, 147, 255});
  Rectangle bar = {a.x + a.width * 0.25f, a.y + GameUiPixels(28),
                   a.width * 0.5f, (float)GameUiPixels(10)};
  DrawRectangleRec(bar, (Color){30, 18, 6, 255});
  DrawRectangleRec((Rectangle){bar.x, bar.y, bar.width * Clamp01(shownHp), bar.height},
                   (Color){214, 160, 50, 255});
  for (int i = 1; i < 3; i++)
    DrawLineEx((Vector2){bar.x + bar.width * i / 3, bar.y},
               (Vector2){bar.x + bar.width * i / 3, bar.y + bar.height}, 2, WHITE);
  DrawRectangleLinesEx(bar, 2, (Color){211, 176, 98, 255});

  if (FunctionalGet()->sealWard)
    DrawCentered(a, TextFormat("PROTECTIONS %d", sealWards), 14,
                 a.y + GameUiPixels(44), GOLD);

  float shownTime = timeLeft < 0 ? 0 : timeLeft;
  const char *timer = TextFormat("%.0f", shownTime);
  DrawText(timer, (int)(a.x + a.width) - MeasureText(timer, hudSize) - GameUiPixels(10),
           (int)a.y + GameUiPixels(6), hudSize, WHITE);

  DrawText("ESPACE : ESQUIVE", (int)a.x + GameUiPixels(10),
           (int)(a.y + a.height - GameUiPixels(24)), GameUiPixels(16),
           dashCooldown > 0 ? GRAY : RAYWHITE);

  if (introTime < INTRO_TIME) {                /* titre d'intro */
    float p = introTime / INTRO_TIME;
    float al = Clamp01(fminf(p / 0.25f, (1 - p) / 0.25f));
    DrawCentered(a, "MARIKA", 56, a.y + a.height * 0.5f - GameUiPixels(44),
                 Fade((Color){241, 213, 147, 255}, al));
    DrawCentered(a, "CELLE QUI REGNA", 24, a.y + a.height * 0.5f + GameUiPixels(18),
                 Fade((Color){255, 170, 70, 255}, al));
  } else if (collected == 0 && introTime < INTRO_TIME + 6) {
    DrawCentered(a, "TOUCHEZ LES ECLATS DE L'ANNEAU POUR LA BLESSER", 16,
                 a.y + a.height - GameUiPixels(50), Fade(GOLD, 0.9f));
  }

  if (transition > 0 && dying <= 0) {          /* titre de phase */
    float p = 1 - transition / TRANSITION_TIME;
    float al = Clamp01(fminf(p / 0.2f, (1 - p) / 0.25f));
    int ph = Phase();
    DrawCentered(a, phaseTitles[ph], 40, a.y + a.height * 0.5f - GameUiPixels(40),
                 Fade((Color){241, 213, 147, 255}, al));
    DrawCentered(a, phaseNames[ph], 22, a.y + a.height * 0.5f + GameUiPixels(12),
                 Fade((Color){255, 170, 70, 255}, al));
  }

  if (state != BOSS4_PLAYING) {
    const char *title = state == BOSS4_WON ? "VICTOIRE" : "GAME OVER";
    DrawCentered(a, title, 40, a.y + a.height / 2 - GameUiPixels(20), WHITE);
    DrawCentered(a, "ENTREE pour continuer", 20,
                 a.y + a.height / 2 + GameUiPixels(20), GRAY);
  }
}

static void Draw(Rectangle a) {
  BeginScissorMode((int)a.x, (int)a.y, (int)a.width, (int)a.height);

  /* tremblement d'écran via une caméra 2D décalée */
  Camera2D cam = {0};
  cam.zoom = 1;
  cam.offset = (Vector2){sinf(clock * 91) * shake, cosf(clock * 77) * shake};
  BeginMode2D(cam);
  DrawArena();
  EndMode2D();

  if (flash > 0)
    DrawRectangleRec(a, Fade(WHITE, flash));
  EndScissorMode();

  DrawHud(a);
}

/* ============================== API publique ============================== */

void Boss4Update(Rectangle a) {
  if (state == BOSS4_OFF)
    return;

  float smallSide = a.width < a.height ? a.width : a.height;
  arena = a;
  unit = smallSide / 400;
  clock += GetFrameTime();

  if (!ready) {
    player = (Vector2){a.x + a.width / 2, a.y + a.height * 0.8f};
    bossPos = (Vector2){a.x + a.width / 2, a.y + a.height * 0.28f};
    bossTargetX = bossPos.x;
    ready = true;
  }

  if (state == BOSS4_PLAYING)
    Update(GetFrameTime());
  if (state != BOSS4_PLAYING && state != BOSS4_OFF && IsKeyPressed(KEY_ENTER))
    state = BOSS4_OFF;

  Draw(a);
}

Boss4Status Boss4State(void) {
  return state;
}

int Boss4DamageCount(void) {
  return damageCount;
}

void Boss4Unload(void) {
  UnloadTexture(heartTex);
  heartTex = (Texture2D){0};
  UnloadTexture(shadowTex);
  shadowTex = (Texture2D){0};
  UnloadTexture(bossTex);
  bossTex = (Texture2D){0};
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 2; j++) {
      UnloadTexture(sprites[i][j]);
      sprites[i][j] = (Texture2D){0};
    }
  ready = false;
  state = BOSS4_OFF;
}
