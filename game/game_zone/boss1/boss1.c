#include "boss1.h"
#include "../balance.h"
#include "../../items/functionnal/functionnal.h"
#include "../../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

/* Chemin de l'image du boss (renommez votre fichier sans accents). */
#define BOSS_TEXTURE_PATH "assets/boss/behemot.png"

#define MAX_HAZARDS 48
#define MAX_BULLETS 160
#define PILLARS 3
#define DURATION 150.0f
#define TOTAL_RAMS 9         /* 3 phases x 3 humiliations */
#define PHASE_RAMS 3
#define ACTIVE 0.35f
#define LOCK_BEFORE 0.45f    /* la charge se verrouille juste avant le depart */
#define DASH_TIME 0.22f
#define DASH_COOLDOWN 1.2f
#define INTRO_TIME 3.4f
#define TRANSITION_TIME 2.6f
#define DYING_TIME 3.8f
#define BURST_TIME 1.6f

enum { H_BLAST, H_COLUMN, H_BAND };
enum { A_SLAM, A_JUDGE, A_CROWN, A_QUAKE };
enum { C_NONE, C_AIM, C_RUSH };
enum { FRONT, BACK, LEFT, RIGHT };

typedef struct {
  int kind;
  Vector2 pos;
  float size, width, warn, t, life;
  bool on, hit, fired;
} Hazard;

typedef struct {
  Vector2 pos, vel;
  bool on;
} Bullet;

typedef struct {
  Vector2 pos;
  float respawn, rise;
  bool alive;
} Pillar;

static const char *dirNames[4] = {"front", "back", "left", "right"};
static const char *footNames[2] = {"left", "right"};
static const char *phaseTitles[3] = {"PHASE I", "PHASE II", "PHASE III"};
static const char *phaseNames[3] = {"LA COURONNE LOURDE", "LE JUGEMENT DORE",
                                    "L'ORGUEIL ECROULE"};

static Hazard hazards[MAX_HAZARDS];
static Bullet bullets[MAX_BULLETS];
static Pillar pillars[PILLARS];
static Texture2D heartTex, shadowTex, bossTex, sprites[4][2];
static Rectangle arena;
static float unit;
static Vector2 player, dir, bossPos, burstPos, popupPos, chargeDir, chargeFrom;
static bool ready, moving, burstBig;
static Boss1Status state;
static int floorNumber, difficulty, lives, maxLives, damageCount, collected,
    sealWards, facing, lastAttack, regularLeft, crownWaves, chargeMode,
    chargeRam;
static float timeLeft, attackTimer, invincible, step, dashTime, dashCooldown,
    introTime, transition, dying, shake, flash, bossFlash, burstT, shownHp,
    clock, popupT, bossTargetX, stunT, crownTimer, chargeT, chargeWarn,
    chargeLen, chargeTravel;
static bool chargeLocked;

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

/* pseudo-aleatoire deterministe pour les effets visuels */
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
  int p = collected / PHASE_RAMS;
  return p > 2 ? 2 : p;
}

static float Intensity(void) {
  return BalanceBossIntensity(floorNumber, difficulty);
}
static float PillarR(void) { return 24 * unit; }
static float BossR(void) { return 30 * unit; }
static int RegularsPerCharge(void) { return Phase() >= 2 ? 2 : 1; }

static void DrawCentered(Rectangle a, const char *text, int size, float y,
                         Color color) {
  int fontSize = GameUiPixels(size);
  int x = (int)(a.x + (a.width - MeasureText(text, fontSize)) / 2);
  DrawText(text, x, (int)y, fontSize, color);
}

/* Temps d'avertissement (jamais en dessous de 0.3 s). */
static float Warn(float factor) {
  const FunctionalSettings *items = FunctionalGet();
  float bonus = items->sealWarning;
  if (difficulty >= 7 && items->sealHighAid)
    bonus += 0.12f;
  return fmaxf(0.3f, BalanceSealWarning(difficulty, bonus) * factor);
}

/* ============================== degats ============================== */

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
    state = BOSS1_LOST;
}

/* ============================== creation ============================== */

static Hazard *AddHazard(int kind, Vector2 pos, float size, float width,
                         float warn) {
  for (int i = 0; i < MAX_HAZARDS; i++) {
    if (hazards[i].on)
      continue;
    hazards[i] = (Hazard){0};
    hazards[i].kind = kind;
    hazards[i].pos = pos;
    hazards[i].size = size;
    hazards[i].width = width;
    hazards[i].warn = warn;
    hazards[i].life = ACTIVE;
    hazards[i].on = true;
    return &hazards[i];
  }
  return NULL;
}

static void AddBullet(Vector2 pos, Vector2 vel) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].on)
      continue;
    bullets[i] = (Bullet){pos, vel, true};
    return;
  }
}

/* ============================== piliers ============================== */

static void SpawnPillar(int idx) {
  Vector2 pos = {arena.x + arena.width / 2, arena.y + arena.height * 0.7f};
  for (int tries = 0; tries < 25; tries++) {
    pos = (Vector2){arena.x + RandRange(0.14f, 0.86f) * arena.width,
                    arena.y + RandRange(0.48f, 0.88f) * arena.height};
    bool ok = hypotf(pos.x - player.x, pos.y - player.y) > 70 * unit;
    for (int j = 0; j < PILLARS; j++)
      if (j != idx && pillars[j].alive &&
          hypotf(pos.x - pillars[j].pos.x, pos.y - pillars[j].pos.y) < 110 * unit)
        ok = false;
    if (ok)
      break;
  }
  pillars[idx].pos = pos;
  pillars[idx].alive = true;
  pillars[idx].rise = 0;
  pillars[idx].respawn = 0;
}

static void UpdatePillars(float dt) {
  for (int i = 0; i < PILLARS; i++) {
    if (pillars[i].alive) {
      pillars[i].rise = fminf(1, pillars[i].rise + dt * 2);
    } else {
      pillars[i].respawn -= dt;
      if (pillars[i].respawn <= 0)
        SpawnPillar(i);
    }
  }
}

static bool PillarSolid(int i) {
  return pillars[i].alive && pillars[i].rise >= 0.8f;
}

/* ============================== humiliation ============================== */

/* Le boss s'est fracasse sur un pilier. */
static void Strike(Vector2 at) {
  const FunctionalSettings *items = FunctionalGet();
  burstPos = at;
  burstT = 0;
  burstBig = false;
  collected++;
  popupT = 1.0f;
  popupPos = at;
  if (items->sealWard && collected % 2 == 0)
    sealWards++;
  bossFlash = 1;
  flash = 0.3f;
  shake = fmaxf(shake, 13 * unit);
  crownWaves = 0;
  chargeMode = C_NONE;
  stunT = 1.4f;
  regularLeft = RegularsPerCharge();
  memset(hazards, 0, sizeof(hazards));
  memset(bullets, 0, sizeof(bullets));

  if (collected >= TOTAL_RAMS) {                  /* mort du boss */
    dying = DYING_TIME;
    burstBig = true;
    flash = 0.9f;
  } else if (collected % PHASE_RAMS == 0) {       /* changement de phase */
    transition = TRANSITION_TIME;
    burstBig = true;
    flash = 0.8f;
    attackTimer = TRANSITION_TIME + 1.0f;
  } else {
    attackTimer = 1.2f * items->sealSpawn;
  }
}

/* Calcule la trajectoire de la charge : elle s'arrete au premier pilier. */
static void ComputeLane(void) {
  Vector2 from = bossPos;
  float dx = player.x - from.x, dy = player.y - from.y;
  float len = hypotf(dx, dy);
  if (len < 1) {
    dx = 0;
    dy = 1;
    len = 1;
  }
  chargeDir = (Vector2){dx / len, dy / len};

  float tx = 1e9f, ty = 1e9f;
  if (chargeDir.x > 0.001f)
    tx = (arena.x + arena.width - from.x) / chargeDir.x;
  else if (chargeDir.x < -0.001f)
    tx = (arena.x - from.x) / chargeDir.x;
  if (chargeDir.y > 0.001f)
    ty = (arena.y + arena.height - from.y) / chargeDir.y;
  else if (chargeDir.y < -0.001f)
    ty = (arena.y - from.y) / chargeDir.y;
  chargeLen = fmaxf(0, fminf(tx, ty));
  chargeRam = -1;

  float R = PillarR() + BossR();
  for (int i = 0; i < PILLARS; i++) {
    if (!PillarSolid(i))
      continue;
    float mx = from.x - pillars[i].pos.x, my = from.y - pillars[i].pos.y;
    float b = mx * chargeDir.x + my * chargeDir.y;
    float disc = b * b - (mx * mx + my * my - R * R);
    if (disc < 0)
      continue;
    float t = -b - sqrtf(disc);
    if (t < 0 || t >= chargeLen)
      continue;
    chargeLen = t;
    chargeRam = i;
  }
}

static void StartCharge(void) {
  chargeMode = C_AIM;
  chargeT = 0;
  chargeLocked = false;
  chargeWarn = fmaxf(1.3f, Warn(1.8f));
  ComputeLane();
}

static void EndCharge(void) {
  const FunctionalSettings *items = FunctionalGet();
  chargeMode = C_NONE;
  stunT = 0.5f;
  attackTimer = 0.8f * items->sealSpawn;
}

static void UpdateCharge(float dt) {
  if (chargeMode == C_NONE)
    return;

  if (chargeMode == C_AIM) {
    chargeT += dt;
    if (!chargeLocked) {                /* le boss suit le joueur, puis verrouille */
      ComputeLane();
      if (chargeT >= chargeWarn - LOCK_BEFORE) {
        chargeLocked = true;
        shake = fmaxf(shake, 4 * unit);
      }
    }
    if (chargeT >= chargeWarn) {
      chargeMode = C_RUSH;
      chargeTravel = 0;
      chargeFrom = bossPos;
      shake = fmaxf(shake, 7 * unit);
    }
    return;
  }

  /* la charge */
  float speed = (420 + 60 * Phase()) * unit * Intensity();
  chargeTravel = fminf(chargeLen, chargeTravel + speed * dt);
  bossPos = (Vector2){chargeFrom.x + chargeDir.x * chargeTravel,
                      chargeFrom.y + chargeDir.y * chargeTravel};
  shake = fmaxf(shake, 3 * unit);

  float r = 10 * unit * FunctionalGet()->hitbox;
  if (transition <= 0 && dying <= 0 && invincible <= 0 && dashTime <= 0 &&
      hypotf(player.x - bossPos.x, player.y - bossPos.y) < BossR() + r)
    Hurt();

  if (chargeTravel >= chargeLen) {
    if (chargeRam >= 0 && pillars[chargeRam].alive) {
      Vector2 at = pillars[chargeRam].pos;
      pillars[chargeRam].alive = false;
      pillars[chargeRam].respawn = 3.5f + 0.5f * Phase();
      Strike(at);
    } else {
      EndCharge();
    }
  }
}

/* ============================== attaques ============================== */

/* Ecrasement : la dalle d'or s'abat sur le joueur. */
static void AtkSlam(void) {
  int p = Phase();
  int n = 2 + p;
  for (int i = 0; i < n; i++) {
    Vector2 pos = {player.x + RandRange(-25, 25) * unit +
                       (moving ? dir.x * i * 50 * unit : 0),
                   player.y + RandRange(-25, 25) * unit +
                       (moving ? dir.y * i * 50 * unit : 0)};
    Hazard *h = AddHazard(H_BLAST, pos, (56 + 4 * p) * unit, 0, Warn(1.2f));
    if (h)
      h->t = -i * 0.55f;
  }
}

/* Jugement : colonnes de lumiere autour du joueur. */
static void AtkJudge(void) {
  int p = Phase();
  static const float off0[3] = {0, -110, 110};
  static const float off1[5] = {0, -100, 100, -200, 200};
  static const float off2[5] = {0, -75, 75, -150, 150};
  const float *offs = p == 0 ? off0 : (p == 1 ? off1 : off2);
  int n = p == 0 ? 3 : 5;
  float width = (p >= 2 ? 34 : 40) * unit;
  for (int i = 0; i < n; i++) {
    float x = Limit(player.x + offs[i] * unit, arena.x + width / 2,
                    arena.x + arena.width - width / 2);
    Hazard *h = AddHazard(H_COLUMN, (Vector2){x, arena.y}, 0, width, Warn(1.1f));
    if (h)
      h->t = -(i + 1) / 2 * 0.25f;
  }
}

/* Couronne : eclat d'epines dorees en tous sens. */
static void AtkCrown(void) {
  crownWaves = 2 + (Phase() >= 2);
  crownTimer = 0;
}

static void SpawnCrownWave(void) {
  const FunctionalSettings *items = FunctionalGet();
  int n = 12 + 2 * Phase();
  float lo = BalanceRandomSpeedMin(floorNumber, difficulty, 115.0f);
  float hi = BalanceRandomSpeedMax(floorNumber, difficulty, 115.0f);
  float speed = RandRange(lo, hi) * unit * items->bulletSpeed;
  float off = RandRange(0, 6.28f);
  for (int i = 0; i < n; i++) {
    float a = off + i * 2 * PI / n;
    AddBullet(Polar(bossPos, a, 30 * unit),
              (Vector2){cosf(a) * speed, sinf(a) * speed});
  }
  shake = fmaxf(shake, 3 * unit);
  crownWaves--;
  crownTimer = 0.8f;
}

/* Seisme : bandes horizontales qui secouent le sol. */
static void AtkQuake(void) {
  int n = 1 + Phase();
  float ys[3];
  for (int i = 0; i < n; i++) {
    float y = arena.y + arena.height * 0.5f;
    for (int tries = 0; tries < 15; tries++) {
      y = arena.y + RandRange(0.35f, 0.95f) * arena.height;
      bool ok = true;
      for (int j = 0; j < i; j++)
        if (fabsf(y - ys[j]) < 95 * unit)
          ok = false;
      if (ok)
        break;
    }
    ys[i] = y;
    Hazard *h = AddHazard(H_BAND, (Vector2){arena.x, y}, 44 * unit, 0, Warn(1.3f));
    if (h)
      h->t = -i * 0.3f;
  }
}

static void LaunchAttack(void) {
  const FunctionalSettings *items = FunctionalGet();
  static const int list0[] = {A_SLAM, A_JUDGE, A_CROWN, A_SLAM};
  static const int list1[] = {A_SLAM, A_JUDGE, A_CROWN, A_QUAKE, A_JUDGE};
  static const int list2[] = {A_SLAM, A_JUDGE, A_CROWN, A_QUAKE, A_SLAM};
  int p = Phase();

  /* apres la (ou les) attaque(s) normale(s), le boss charge */
  if (regularLeft <= 0) {
    StartCharge();
    regularLeft = RegularsPerCharge();
    return;
  }
  regularLeft--;

  const int *list = p == 0 ? list0 : (p == 1 ? list1 : list2);
  int n = p == 0 ? 4 : 5;
  int attack = list[GetRandomValue(0, n - 1)];
  for (int tries = 0; tries < 3 && attack == lastAttack; tries++)
    attack = list[GetRandomValue(0, n - 1)];
  lastAttack = attack;

  bossTargetX = arena.x + arena.width / 2 + RandRange(-0.15f, 0.15f) * arena.width;

  float extra = 0;
  switch (attack) {
  case A_SLAM:  AtkSlam(); extra = (2 + p) * 0.55f + 0.8f; break;
  case A_JUDGE: AtkJudge(); extra = 1.6f; break;
  case A_CROWN: AtkCrown(); extra = (2 + (p >= 2)) * 0.8f + 1.2f; break;
  case A_QUAKE: AtkQuake(); extra = 1.2f + 0.3f * p; break;
  }
  attackTimer = ((1.7f - 0.2f * p) / Intensity() + extra) * items->sealSpawn;
}

/* ============================== demarrage ============================== */

void Boss1Start(int floorNumberArg, int d) {
  if (heartTex.id == 0)
    heartTex = LoadTexture("assets/heart.png");
  if (shadowTex.id == 0)
    shadowTex = LoadTexture("assets/player/shadow.png");
  if (bossTex.id == 0) {
    bossTex = LoadTexture(BOSS_TEXTURE_PATH);
    if (bossTex.id != 0) {   /* grande image reduite : mipmaps = rendu propre */
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
  state = BOSS1_PLAYING;
  timeLeft = DURATION + items->duration;
  attackTimer = 0.5f;
  invincible = step = dashTime = dashCooldown = 0;
  introTime = transition = dying = shake = flash = bossFlash = 0;
  crownWaves = 0;
  crownTimer = 0;
  popupT = 0;
  stunT = 0;
  chargeMode = C_NONE;
  chargeLocked = false;
  chargeRam = -1;
  regularLeft = 1;
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
  memset(pillars, 0, sizeof(pillars));
}

/* ============================== mise a jour ============================== */

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

  /* les piliers sont solides */
  for (int i = 0; i < PILLARS; i++) {
    if (!PillarSolid(i))
      continue;
    float dx = player.x - pillars[i].pos.x, dy = player.y - pillars[i].pos.y;
    float d = hypotf(dx, dy), minD = PillarR() + 12 * unit;
    if (d < minD && d > 0.01f) {
      player.x = pillars[i].pos.x + dx / d * minD;
      player.y = pillars[i].pos.y + dy / d * minD;
    }
  }
  player.x = Limit(player.x, arena.x + margin, arena.x + arena.width - margin);
  player.y = Limit(player.y, arena.y + margin, arena.y + arena.height - margin);
}

static bool Touches(Hazard *h) {
  float r = 10 * unit * FunctionalGet()->hitbox;
  switch (h->kind) {
  case H_BLAST:
    return hypotf(player.x - h->pos.x, player.y - h->pos.y) < h->size + r;
  case H_COLUMN:
    return fabsf(player.x - h->pos.x) < h->width / 2 + r;
  case H_BAND:
    return fabsf(player.y - h->pos.y) < h->size / 2 + r;
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
  bool firing = h->t >= h->warn;
  if (firing && !h->fired) {
    h->fired = true;
    shake = fmaxf(shake, (h->kind == H_BLAST ? 7 : 4) * unit);
  }
  if (!firing || h->hit || invincible > 0 || dashTime > 0 || transition > 0 ||
      dying > 0 || !Touches(h))
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
    float out = 40 * unit;
    if (b->pos.x < arena.x - out || b->pos.x > arena.x + arena.width + out ||
        b->pos.y < arena.y - out || b->pos.y > arena.y + arena.height + out) {
      b->on = false;
      continue;
    }
    bool blocked = false;           /* les piliers arretent les epines */
    for (int j = 0; j < PILLARS; j++)
      if (PillarSolid(j) &&
          hypotf(b->pos.x - pillars[j].pos.x, b->pos.y - pillars[j].pos.y) <
              PillarR()) {
        blocked = true;
        break;
      }
    if (blocked) {
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

static void Update(float dt) {
  invincible -= dt;
  dashTime -= dt;
  dashCooldown -= dt;
  stunT = fmaxf(0, stunT - dt);
  shake *= expf(-dt * 6);
  flash = fmaxf(0, flash - dt * 1.5f);
  bossFlash = fmaxf(0, bossFlash - dt * 2.5f);
  popupT = fmaxf(0, popupT - dt);
  if (burstT >= 0) {
    burstT += dt;
    if (burstT > BURST_TIME)
      burstT = -1;
  }
  shownHp += ((1.0f - (float)collected / TOTAL_RAMS) - shownHp) * fminf(1, dt * 4);

  /* le boss regagne sa place (immobile pendant la visee et l'etourdissement) */
  if (chargeMode == C_NONE && stunT <= 0) {
    float homeY = arena.y + arena.height * 0.28f;
    float homeX = bossTargetX + sinf(clock * 0.45f) * arena.width * 0.06f;
    bossPos.x += (homeX - bossPos.x) * fminf(1, dt * 2.5f);
    bossPos.y += (homeY - bossPos.y) * fminf(1, dt * 2.5f);
  }

  MovePlayer(dt);
  UpdatePillars(dt);

  if (dying > 0) {                      /* mort du boss : outro */
    dying -= dt;
    shake = fmaxf(shake, 3 * unit);
    if (dying <= 0)
      state = BOSS1_WON;
    return;
  }
  if (introTime < INTRO_TIME) {
    introTime += dt;
    return;
  }

  timeLeft -= dt;

  if (transition > 0) {
    transition -= dt;
  } else {
    if (crownWaves > 0) {
      crownTimer -= dt;
      if (crownTimer <= 0)
        SpawnCrownWave();
    }
    if (chargeMode == C_NONE && stunT <= 0) {
      attackTimer -= dt;
      if (attackTimer <= 0 && crownWaves <= 0)
        LaunchAttack();
    }
  }

  for (int i = 0; i < MAX_HAZARDS; i++)
    if (hazards[i].on)
      UpdateHazard(&hazards[i], dt);
  UpdateBullets(dt);
  UpdateCharge(dt);

  if (timeLeft <= 0 && state == BOSS1_PLAYING)
    state = BOSS1_LOST;
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

static void DrawBoss(void) {
  int phase = Phase();
  float h = arena.height * 0.46f;
  float w = bossTex.id ? h * bossTex.width / bossTex.height : h * 0.9f;
  Color gold = {255, 205, 100, 255};

  float introP = Clamp01(introTime / INTRO_TIME);
  float ease = 1 - powf(1 - introP, 3);
  float alpha = Clamp01(introTime / 1.8f);
  if (dying > 0 && dying < DYING_TIME - 1.2f)
    alpha = Clamp01(dying / (DYING_TIME - 1.2f));
  float rise = (1 - ease) * arena.height * 0.12f;
  float tremble = dying > 0 ? sinf(clock * 80) * 4 * unit : 0;
  tremble += bossFlash * sinf(clock * 90) * 3 * unit;
  if (chargeMode == C_AIM)              /* il se crispe avant de charger */
    tremble += sinf(clock * 60) * 2 * unit * Clamp01(chargeT / chargeWarn);

  Vector2 c = {bossPos.x + tremble,
               bossPos.y + sinf(clock * 1.3f) * (chargeMode == C_RUSH ? 0 : 5) * unit - rise};
  float charge = chargeMode == C_AIM ? Clamp01(chargeT / chargeWarn) : 0;

  BeginBlendMode(BLEND_ADDITIVE);   /* aura sombre cramoisie et doree */
  for (int l = 0; l < 6; l++)
    DrawCircleV((Vector2){c.x, c.y + h * 0.05f}, h * (0.22f + 0.1f * l),
                Fade((Color){190, 60, 30, 255}, (0.03f + 0.02f * phase) * alpha));
  for (int i = 0; i < 18; i++) {    /* poussiere d'or qui monte */
    float ph = fmodf(clock * 0.28f + Rand01(i * 3), 1.0f);
    DrawCircleV((Vector2){c.x + (Rand01(i * 3 + 1) - 0.5f) * w,
                          c.y + h * 0.4f - ph * h * 0.9f},
                (1 + 2 * Rand01(i * 3 + 2)) * unit,
                Fade(gold, (1 - ph) * 0.8f * alpha));
  }
  /* sceau d'or de la main : se charge avant la charge */
  Vector2 sigil = {c.x + w * 0.24f, c.y - h * 0.275f};
  float pulse = 0.5f + 0.5f * sinf(clock * 4);
  DrawCircleV(sigil, h * (0.08f + 0.06f * charge + 0.01f * pulse),
              Fade(gold, (0.2f + 0.5f * charge) * alpha));
  for (int i = 0; i < 8; i++) {
    float a = clock * (0.8f + 0.4f * phase) + i * PI / 4;
    DrawLineEx(Polar(sigil, a, h * 0.09f), Polar(sigil, a, h * (0.14f + 0.03f * charge)),
               2 * unit, Fade(gold, 0.45f * alpha));
  }
  /* halo de la couronne */
  DrawCircleV((Vector2){c.x, c.y - h * 0.38f}, h * 0.14f, Fade(gold, 0.10f * alpha));
  EndBlendMode();

  if (bossTex.id == 0) {
    DrawCircleV(c, h * 0.3f, Fade((Color){110, 30, 30, 255}, alpha));
    return;
  }
  float f = bossFlash;
  Color tint = {255, (unsigned char)(255 - 110 * f), (unsigned char)(255 - 110 * f),
                (unsigned char)(255 * alpha)};
  Rectangle src = {0, 0, (float)bossTex.width, (float)bossTex.height};
  Rectangle dst = {c.x - w / 2, c.y - h / 2, w, h};
  DrawTexturePro(bossTex, src, dst, (Vector2){0, 0}, 0, tint);

  if (bossFlash > 0) {              /* eclat blanc quand il est humilie */
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(bossTex, src, dst, (Vector2){0, 0}, 0,
                   Fade(WHITE, bossFlash * 0.5f * alpha));
    EndBlendMode();
  }

  if (stunT > 0 && dying <= 0) {    /* etoiles d'etourdissement */
    for (int i = 0; i < 4; i++) {
      float a = clock * 5 + i * PI / 2;
      DrawPoly(Polar((Vector2){c.x, c.y - h * 0.5f}, a, 28 * unit), 4, 5 * unit,
               clock * 120, Fade(gold, Clamp01(stunT)));
    }
  }
}

static void DrawPillar(Pillar *p) {
  if (!p->alive)
    return;
  Color gold = {225, 180, 80, 255};
  float r = PillarR();
  float hgt = 46 * unit * p->rise;
  Rectangle body = {p->pos.x - r * 0.8f, p->pos.y - hgt, r * 1.6f, hgt};

  DrawEllipse((int)p->pos.x, (int)(p->pos.y + 4 * unit), r * 1.25f, r * 0.55f,
              Fade(BLACK, 0.4f));
  DrawRectangleRec(body, (Color){38, 28, 32, 255});
  DrawRectangleLinesEx(body, 2 * unit, Fade(gold, 0.8f));
  for (int k = 0; k < 3; k++) {
    float y = body.y + hgt * (0.25f + 0.25f * k);
    DrawLineEx((Vector2){body.x, y}, (Vector2){body.x + body.width, y}, 2 * unit,
               Fade(gold, 0.6f));
  }
  DrawEllipse((int)p->pos.x, (int)(p->pos.y - hgt), r * 0.95f, r * 0.45f,
              (Color){62, 46, 40, 255});
  DrawEllipseLines((int)p->pos.x, (int)(p->pos.y - hgt), r * 0.95f, r * 0.45f, gold);
  DrawLineEx((Vector2){p->pos.x, p->pos.y - hgt},
             (Vector2){p->pos.x, p->pos.y - hgt - 14 * unit * p->rise}, 3 * unit, gold);
}

/* Trajectoire de la charge et reticule de visee. */
static void DrawCharge(void) {
  if (chargeMode == C_NONE)
    return;
  Color gold = {255, 215, 120, 255};
  float R = BossR();

  if (chargeMode == C_AIM) {
    float p = Clamp01(chargeT / chargeWarn);
    Vector2 from = bossPos;
    Vector2 end = {from.x + chargeDir.x * chargeLen, from.y + chargeDir.y * chargeLen};
    Vector2 n = {-chargeDir.y * R, chargeDir.x * R};
    Color edge = chargeLocked ? RED : gold;
    Color fill = chargeLocked
                     ? Fade(RED, 0.22f + 0.12f * sinf(clock * 30))
                     : Fade(gold, 0.08f + 0.12f * p);

    DrawLineEx(from, end, R * 2, fill);
    DrawLineEx((Vector2){from.x + n.x, from.y + n.y}, (Vector2){end.x + n.x, end.y + n.y},
               2 * unit, Fade(edge, 0.7f));
    DrawLineEx((Vector2){from.x - n.x, from.y - n.y}, (Vector2){end.x - n.x, end.y - n.y},
               2 * unit, Fade(edge, 0.7f));

    if (chargeRam >= 0) {             /* le pilier qui va l'arreter */
      float k = 0.5f + 0.5f * sinf(clock * 8);
      DrawRing(pillars[chargeRam].pos, PillarR() * 1.3f, PillarR() * (1.5f + 0.2f * k),
               0, 360, 32, Fade(gold, 0.9f));
    }

    float rr = 24 * unit * (1.7f - 0.7f * p);   /* reticule sur le joueur */
    Color rc = chargeLocked ? WHITE : gold;
    DrawCircleLinesV(player, rr, rc);
    DrawLineEx((Vector2){player.x - rr * 1.4f, player.y},
               (Vector2){player.x + rr * 1.4f, player.y}, 2 * unit, Fade(rc, 0.8f));
    DrawLineEx((Vector2){player.x, player.y - rr * 1.4f},
               (Vector2){player.x, player.y + rr * 1.4f}, 2 * unit, Fade(rc, 0.8f));
  } else {
    BeginBlendMode(BLEND_ADDITIVE);
    DrawLineEx(chargeFrom, bossPos, R * 1.6f, Fade(gold, 0.25f));
    EndBlendMode();
  }
}

static void DrawHazard(Hazard *h) {
  if (h->t < 0)
    return;
  Color gold = {255, 215, 120, 255};
  Color crimson = {200, 40, 40, 255};
  bool firing = h->t >= h->warn;
  float p = Clamp01(h->t / h->warn);
  float u = Clamp01((h->t - h->warn) / h->life), al = 1 - u;

  switch (h->kind) {
  case H_BLAST: {
    if (!firing) {
      DrawCircleV(h->pos, h->size * p, Fade(crimson, 0.4f));
      DrawCircleLinesV(h->pos, h->size, Fade(gold, 0.9f));
    } else {
      float r = h->size * (0.85f + 0.25f * u);
      BeginBlendMode(BLEND_ADDITIVE);
      DrawCircleV(h->pos, r, Fade(gold, 0.6f * al));
      for (int i = 0; i < 10; i++) {   /* eclats de la dalle */
        float a = clock * 1.2f + i * 2 * PI / 10;
        DrawLineEx(Polar(h->pos, a, r * 0.3f), Polar(h->pos, a, r * 1.3f),
                   3 * unit, Fade(WHITE, al));
      }
      DrawCircleV(h->pos, r * 0.45f, Fade(WHITE, 0.8f * al));
      EndBlendMode();
      DrawRing(h->pos, r * 0.95f, r * 1.05f, 0, 360, 48, Fade(crimson, al));
    }
    break;
  }
  case H_COLUMN: {
    Rectangle col = {h->pos.x - h->width / 2, arena.y, h->width, arena.height};
    if (!firing) {
      DrawRectangleRec(col, Fade(gold, 0.06f + 0.18f * p));
      DrawRectangleLinesEx(col, 2 * unit, Fade(gold, 0.6f));
      DrawCircleLinesV((Vector2){h->pos.x, arena.y + 18 * unit},
                       14 * unit * (1.4f - 0.4f * p), Fade(gold, 0.9f));
    } else {
      DrawRectangleRec(col, Fade(WHITE, al));
      BeginBlendMode(BLEND_ADDITIVE);
      DrawRectangleRec((Rectangle){col.x - col.width * 0.4f, col.y,
                                   col.width * 1.8f, col.height},
                       Fade(gold, 0.45f * al));
      EndBlendMode();
    }
    break;
  }
  case H_BAND: {
    Rectangle band = {arena.x, h->pos.y - h->size / 2, arena.width, h->size};
    if (!firing) {
      DrawRectangleRec(band, Fade(crimson, 0.05f + 0.18f * p));
      DrawRectangleLinesEx(band, 2 * unit, Fade(gold, 0.6f));
    } else {
      DrawRectangleRec(band, Fade(WHITE, al));
      BeginBlendMode(BLEND_ADDITIVE);
      DrawRectangleRec((Rectangle){band.x, band.y - band.height * 0.4f, band.width,
                                   band.height * 1.8f},
                       Fade(crimson, 0.5f * al));
      EndBlendMode();
    }
    break;
  }
  }
}

static void DrawBullets(void) {
  const FunctionalSettings *items = FunctionalGet();
  float size = 6 * unit * items->bulletSize;
  Color gold = {255, 215, 120, 255};
  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 0; i < MAX_BULLETS; i++)
    if (bullets[i].on)
      DrawCircleV(bullets[i].pos, size * 2.0f, Fade(gold, 0.25f));
  EndBlendMode();
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].on)
      continue;
    float ang = atan2f(bullets[i].vel.y, bullets[i].vel.x) * RAD2DEG;
    DrawPoly(bullets[i].pos, 3, size * 1.5f, ang, gold);
    DrawPolyLines(bullets[i].pos, 3, size * 1.5f, ang, (Color){200, 40, 40, 255});
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
  for (int i = 0; i < 60; i++) {     /* gravats du pilier */
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
             Fade((Color){220, 60, 40, 255}, fade));
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
                         Fade((Color){90, 25, 15, 255}, 0.2f + 0.1f * phase));

  /* sceau de couronne grave au sol */
  Vector2 sigil = {arena.x + arena.width / 2, arena.y + arena.height * 0.62f};
  float spin = clock * (0.1f + 0.06f * phase);
  BeginBlendMode(BLEND_ADDITIVE);
  DrawCircleLinesV(sigil, 150 * unit, Fade(gold, 0.09f));
  DrawCircleLinesV(sigil, 105 * unit, Fade(gold, 0.07f));
  for (int i = 0; i < 12; i++) {
    float a = spin + i * PI / 6;
    DrawLineEx(Polar(sigil, a, 105 * unit), Polar(sigil, a, (i % 2 ? 150 : 165) * unit),
               2 * unit, Fade(gold, 0.09f));
  }
  EndBlendMode();

  /* cendres et grains d'or */
  for (int i = 0; i < 30; i++) {
    float ph = fmodf(clock * (0.05f + 0.07f * Rand01(i * 4)) + Rand01(i * 4 + 1), 1);
    float x = arena.x + arena.width * Rand01(i * 4 + 2) + sinf(clock * 1.3f + i) * 18 * unit;
    float y = arena.y + arena.height * (i % 2 ? ph : 1 - ph);
    float size = (1.5f + 2.5f * Rand01(i * 4 + 3)) * unit;
    Color col = i % 3 ? (Color){255, 215, 120, 255} : (Color){150, 30, 30, 255};
    DrawEllipse((int)x, (int)y, size * 1.5f, size, Fade(col, 0.4f));
  }

  DrawCharge();
  for (int i = 0; i < PILLARS; i++)
    DrawPillar(&pillars[i]);
  DrawBoss();

  for (int i = 0; i < MAX_HAZARDS; i++)
    if (hazards[i].on)
      DrawHazard(&hazards[i]);
  DrawBullets();

  /* joueur : images remanentes pendant l'esquive */
  if (dashTime > 0)
    for (int g = 3; g >= 1; g--)
      DrawPlayerAt((Vector2){player.x - dir.x * g * 16 * unit,
                             player.y - dir.y * g * 16 * unit},
                   Fade(WHITE, 0.18f), false);
  bool visible = invincible <= 0 || (int)(invincible * 10) % 2 == 0;
  if (visible)
    DrawPlayerAt(player, dashTime > 0 ? Fade(WHITE, 0.4f) : WHITE, true);

  DrawBurst();

  if (popupT > 0) {                  /* texte flottant */
    const char *txt = "HUMILIE !";
    int fs = GameUiPixels(22);
    float rise = (1 - popupT) * 40 * unit;
    DrawText(txt, (int)(popupPos.x - MeasureText(txt, fs) / 2.0f),
             (int)(popupPos.y - 60 * unit - rise), fs,
             Fade((Color){255, 225, 140, 255}, Clamp01(popupT * 1.5f)));
  }
}

static void DrawHud(Rectangle a) {
  float ui = GameUiScale();
  int hudSize = GameUiPixels(20);

  for (int i = 0; i < lives; i++)
    DrawHeart((Vector2){a.x + (16 + i * 28) * ui, a.y + 16 * ui}, 20 * ui);

  /* barre de vie du boss, avec reperes de phase */
  DrawCentered(a, "BEHEMOT, DEMON DE L'ARROGANCE", 16, a.y + GameUiPixels(6),
               (Color){241, 213, 147, 255});
  Rectangle bar = {a.x + a.width * 0.25f, a.y + GameUiPixels(28),
                   a.width * 0.5f, (float)GameUiPixels(10)};
  DrawRectangleRec(bar, (Color){30, 18, 6, 255});
  DrawRectangleRec((Rectangle){bar.x, bar.y, bar.width * Clamp01(shownHp), bar.height},
                   (Color){190, 120, 35, 255});
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
    DrawCentered(a, "BEHEMOT", 56, a.y + a.height * 0.5f - GameUiPixels(44),
                 Fade((Color){241, 213, 147, 255}, al));
    DrawCentered(a, "DEMON DE L'ARROGANCE", 24,
                 a.y + a.height * 0.5f + GameUiPixels(18),
                 Fade((Color){220, 130, 50, 255}, al));
  } else if (collected == 0 && chargeMode == C_AIM) {
    DrawCentered(a, "ATTIREZ SA CHARGE CONTRE UN PILIER", 16,
                 a.y + a.height - GameUiPixels(50), Fade(GOLD, 0.95f));
  }

  if (transition > 0 && dying <= 0) {          /* titre de phase */
    float p = 1 - transition / TRANSITION_TIME;
    float al = Clamp01(fminf(p / 0.2f, (1 - p) / 0.25f));
    int ph = Phase();
    DrawCentered(a, phaseTitles[ph], 40, a.y + a.height * 0.5f - GameUiPixels(40),
                 Fade((Color){241, 213, 147, 255}, al));
    DrawCentered(a, phaseNames[ph], 22, a.y + a.height * 0.5f + GameUiPixels(12),
                 Fade((Color){220, 130, 50, 255}, al));
  }

  if (state != BOSS1_PLAYING) {
    const char *title = state == BOSS1_WON ? "VICTOIRE" : "GAME OVER";
    DrawCentered(a, title, 40, a.y + a.height / 2 - GameUiPixels(20), WHITE);
    DrawCentered(a, "ENTREE pour continuer", 20,
                 a.y + a.height / 2 + GameUiPixels(20), GRAY);
  }
}

static void Draw(Rectangle a) {
  BeginScissorMode((int)a.x, (int)a.y, (int)a.width, (int)a.height);

  /* tremblement d'ecran via une camera 2D decalee */
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

void Boss1Update(Rectangle a) {
  if (state == BOSS1_OFF)
    return;

  float smallSide = a.width < a.height ? a.width : a.height;
  arena = a;
  unit = smallSide / 400;
  clock += GetFrameTime();

  if (!ready) {
    player = (Vector2){a.x + a.width / 2, a.y + a.height * 0.8f};
    bossPos = (Vector2){a.x + a.width / 2, a.y + a.height * 0.28f};
    bossTargetX = bossPos.x;
    for (int i = 0; i < PILLARS; i++) {
      SpawnPillar(i);
      pillars[i].rise = 1;
    }
    ready = true;
  }

  if (state == BOSS1_PLAYING)
    Update(GetFrameTime());
  if (state != BOSS1_PLAYING && state != BOSS1_OFF && IsKeyPressed(KEY_ENTER))
    state = BOSS1_OFF;

  Draw(a);
}

Boss1Status Boss1State(void) {
  return state;
}

int Boss1DamageCount(void) {
  return damageCount;
}

void Boss1Unload(void) {
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
  state = BOSS1_OFF;
}
