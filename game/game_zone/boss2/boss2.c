#include "boss2.h"
#include "../balance.h"
#include "../../items/functionnal/functionnal.h"
#include "../../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

/* Chemin de l'image du boss (renommez votre fichier sans accents). */
#define BOSS_TEXTURE_PATH "assets/boss/selena.png"

#define MAX_HAZARDS 48
#define MAX_BULLETS 180
#define MAX_MIRRORS 3
#define DURATION 150.0f
#define TOTAL_REFLECTS 9     /* 3 phases x 3 reflets */
#define PHASE_REFLECTS 3
#define ACTIVE 0.35f
#define BEAM_FLASH 0.6f      /* duree visuelle de l'eclat lunaire */
#define DASH_TIME 0.22f
#define DASH_COOLDOWN 1.2f
#define INTRO_TIME 3.4f
#define TRANSITION_TIME 2.6f
#define DYING_TIME 3.8f
#define BURST_TIME 1.6f

enum { H_BLAST, H_ROSE, H_ARC, H_ORB };
enum { A_RAIN, A_ARC, A_ROSES, A_ORB };
enum { FRONT, BACK, LEFT, RIGHT };

typedef struct {
  int kind;
  Vector2 pos;
  float angle, size, width, warn, t, life, speed, gap, reach;
  bool on, hit, fired;
} Hazard;

typedef struct {
  Vector2 pos, vel;
  int kind;                 /* 0 : larme de lune, 1 : petale de rose */
  bool on;
} Bullet;

typedef struct {
  Vector2 pos, vel;
} Mirror;

static const char *dirNames[4] = {"front", "back", "left", "right"};
static const char *footNames[2] = {"left", "right"};
static const char *phaseTitles[3] = {"PHASE I", "PHASE II", "PHASE III"};
static const char *phaseNames[3] = {"LARMES DE LUNE", "LA MAREE NOIRE",
                                    "PLEINE LUNE ECARLATE"};

static Hazard hazards[MAX_HAZARDS];
static Bullet bullets[MAX_BULLETS];
static Mirror mirrors[MAX_MIRRORS];
static Texture2D heartTex, shadowTex, bossTex, sprites[4][2];
static Rectangle arena;
static float unit;
static Vector2 player, dir, bossPos, burstPos, popupPos;
static bool ready, moving, burstBig, beamOn, beamFired;
static Boss2Status state;
static int floorNumber, difficulty, lives, maxLives, damageCount, collected,
    sealWards, facing, lastAttack, regularLeft, mirrorCount;
static float timeLeft, attackTimer, invincible, step, dashTime, dashCooldown,
    introTime, transition, dying, shake, flash, bossFlash, burstT, shownHp,
    clock, rainLeft, rainTimer, popupT, bossTargetX, beamT, beamWarn, mirrorR;

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
  int p = collected / PHASE_REFLECTS;
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

static float AimAngle(Vector2 from, float lead) {
  Vector2 t = player;
  if (moving) {
    t.x += dir.x * lead;
    t.y += dir.y * lead;
  }
  return atan2f(t.y - from.y, t.x - from.x);
}

static int RegularsPerBeam(void) { return Phase() >= 2 ? 2 : 1; }

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
    state = BOSS2_LOST;
}

/* ============================== creation ============================== */

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
    hazards[i].on = true;
    return &hazards[i];
  }
  return NULL;
}

static void AddBullet(Vector2 pos, Vector2 vel, int kind) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].on)
      continue;
    bullets[i] = (Bullet){pos, vel, kind, true};
    return;
  }
}

/* ============================== reflet ============================== */

/* Le boss encaisse la lumiere renvoyee par un miroir. */
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
  shake = fmaxf(shake, 12 * unit);
  rainLeft = 0;
  beamOn = false;
  regularLeft = RegularsPerBeam();
  memset(hazards, 0, sizeof(hazards));
  memset(bullets, 0, sizeof(bullets));

  if (collected >= TOTAL_REFLECTS) {              /* mort du boss */
    dying = DYING_TIME;
    burstBig = true;
    flash = 0.9f;
  } else if (collected % PHASE_REFLECTS == 0) {   /* changement de phase */
    transition = TRANSITION_TIME;
    burstBig = true;
    flash = 0.8f;
    attackTimer = TRANSITION_TIME + 1.0f;
  } else {
    attackTimer = 1.2f * items->sealSpawn;
  }
}

/* Eclat de lune : toute l'arene devient mortelle, sauf les miroirs. */
static void StartBeam(void) {
  static const int counts[3] = {3, 3, 2};
  static const float radii[3] = {40, 34, 30};
  int p = Phase();
  float margin = 40 * unit;
  mirrorCount = counts[p];
  mirrorR = radii[p] * unit;

  for (int i = 0; i < mirrorCount; i++) {
    Vector2 pos = player;
    for (int tries = 0; tries < 20; tries++) {
      if (i == 0) {      /* le premier miroir est toujours a portee du joueur */
        pos = Polar(player, RandRange(0, 6.28f), RandRange(90, 170) * unit);
      } else {
        pos = (Vector2){arena.x + RandRange(0.1f, 0.9f) * arena.width,
                        arena.y + RandRange(0.35f, 0.95f) * arena.height};
      }
      pos.x = Limit(pos.x, arena.x + margin, arena.x + arena.width - margin);
      pos.y = Limit(pos.y, arena.y + arena.height * 0.35f,
                    arena.y + arena.height - margin);
      bool ok = true;
      for (int j = 0; j < i; j++)
        if (hypotf(pos.x - mirrors[j].pos.x, pos.y - mirrors[j].pos.y) <
            110 * unit)
          ok = false;
      if (ok)
        break;
    }
    mirrors[i].pos = pos;
    mirrors[i].vel = (Vector2){0, 0};
    if (p >= 2) {        /* en phase III, les miroirs derivent */
      float a = RandRange(0, 6.28f);
      mirrors[i].vel = (Vector2){cosf(a) * 45 * unit, sinf(a) * 45 * unit};
    }
  }

  beamOn = true;
  beamFired = false;
  beamT = 0;
  beamWarn = fmaxf(1.5f, Warn(2.0f));   /* assez long pour atteindre un miroir */

  /* phases II et III : des explosions gênent le trajet, sans bloquer les miroirs */
  for (int k = 0; k < p; k++) {
    for (int tries = 0; tries < 10; tries++) {
      Vector2 pos = {arena.x + RandRange(0.08f, 0.92f) * arena.width,
                     arena.y + RandRange(0.3f, 0.95f) * arena.height};
      float size = RandRange(40, 55) * unit;
      bool ok = true;
      for (int j = 0; j < mirrorCount; j++)
        if (hypotf(pos.x - mirrors[j].pos.x, pos.y - mirrors[j].pos.y) <
            size + mirrorR + 12 * unit)
          ok = false;
      if (!ok)
        continue;
      AddHazard(H_BLAST, pos, 0, size, 0, Warn(1.3f));
      break;
    }
  }
}

static void UpdateBeam(float dt) {
  if (!beamOn)
    return;
  beamT += dt;
  if (beamFired) {
    if (beamT > beamWarn + BEAM_FLASH)
      beamOn = false;
    return;
  }

  if (Phase() >= 2)                     /* miroirs qui derivent et rebondissent */
    for (int i = 0; i < mirrorCount; i++) {
      Mirror *m = &mirrors[i];
      m->pos.x += m->vel.x * dt;
      m->pos.y += m->vel.y * dt;
      float lx = arena.x + mirrorR, hx = arena.x + arena.width - mirrorR;
      float ly = arena.y + arena.height * 0.35f, hy = arena.y + arena.height - mirrorR;
      if (m->pos.x < lx || m->pos.x > hx) {
        m->vel.x = -m->vel.x;
        m->pos.x = Limit(m->pos.x, lx, hx);
      }
      if (m->pos.y < ly || m->pos.y > hy) {
        m->vel.y = -m->vel.y;
        m->pos.y = Limit(m->pos.y, ly, hy);
      }
    }

  if (beamT < beamWarn)
    return;

  /* l'eclat tombe : on regarde ou se tient le joueur */
  beamFired = true;
  for (int i = 0; i < mirrorCount; i++)
    if (hypotf(player.x - mirrors[i].pos.x, player.y - mirrors[i].pos.y) < mirrorR) {
      Strike(mirrors[i].pos);
      return;
    }
  shake = fmaxf(shake, 10 * unit);
  if (dashTime <= 0 && invincible <= 0)
    Hurt();
}

/* ============================== attaques ============================== */

/* Larmes de lune : pluie de gouttes argentees. */
static void AtkRain(void) {
  rainLeft = 2.4f;
  rainTimer = 0;
}

static void SpawnRainTick(void) {
  const FunctionalSettings *items = FunctionalGet();
  int p = Phase();
  float lo = BalanceRandomSpeedMin(floorNumber, difficulty, 130.0f);
  float hi = BalanceRandomSpeedMax(floorNumber, difficulty, 130.0f);
  int n = 1 + (p >= 1);
  for (int i = 0; i < n; i++) {
    float x = arena.x + RandRange(0.05f, 0.95f) * arena.width;
    float speed = RandRange(lo, hi) * unit * items->bulletSpeed;
    float vx = p >= 2 ? RandRange(-30, 30) * unit : 0;
    AddBullet((Vector2){x, arena.y - 10 * unit}, (Vector2){vx, speed}, 0);
  }
}

/* Croissants : arcs de lumiere qui s'elargissent depuis la deesse. */
static void AtkArc(void) {
  const FunctionalSettings *items = FunctionalGet();
  int n = 1 + Phase();
  float base = AimAngle(bossPos, 50 * unit);
  for (int i = 0; i < n; i++) {
    float off = i == 0 ? 0 : (i % 2 ? 1 : -1) * 0.7f * ((i + 1) / 2);
    Hazard *h = AddHazard(H_ARC, bossPos, base + off, 0, 20 * unit, Warn(0.9f));
    if (!h)
      continue;
    h->speed = 220 * unit * Intensity() * items->bulletSpeed;
    h->gap = 0.45f;                    /* demi-ouverture de l'arc (rad) */
    h->reach = MaxReach();
    h->life = 99;
    h->t = -i * 0.5f;
  }
}

/* Roses : eclosion de roses qui projettent leurs petales. */
static void AtkRoses(void) {
  int n = 3 + Phase();
  for (int i = 0; i < n; i++) {
    Vector2 p;
    if (i == 0)
      p = (Vector2){player.x + RandRange(-60, 60) * unit,
                    player.y + RandRange(-60, 60) * unit};
    else
      p = (Vector2){arena.x + RandRange(0.08f, 0.92f) * arena.width,
                    arena.y + RandRange(0.3f, 0.95f) * arena.height};
    Hazard *h = AddHazard(H_ROSE, p, 0, 38 * unit, 0, Warn(1.4f));
    if (h)
      h->t = -i * 0.12f;
  }
}

/* Orbe noire : lente, elle attire le joueur vers elle. */
static void AtkOrb(void) {
  const FunctionalSettings *items = FunctionalGet();
  int n = Phase() >= 2 ? 2 : 1;
  for (int i = 0; i < n; i++) {
    Hazard *h = AddHazard(H_ORB, bossPos, AimAngle(bossPos, 40 * unit), 24 * unit,
                          0, Warn(1.2f));
    if (!h)
      continue;
    h->speed = 85 * unit * items->bulletSpeed;
    h->reach = 130 * unit;             /* rayon d'attraction */
    h->life = 5.0f;
    h->t = -i * 1.2f;
  }
}

static void SpawnPetals(Hazard *h) {
  const FunctionalSettings *items = FunctionalGet();
  int n = Phase() >= 2 ? 10 : 8;
  float speed = 115 * unit * items->bulletSpeed;
  float off = RandRange(0, 6.28f);
  for (int i = 0; i < n; i++) {
    float a = off + i * 2 * PI / n;
    AddBullet(Polar(h->pos, a, h->size * 0.5f),
              (Vector2){cosf(a) * speed, sinf(a) * speed}, 1);
  }
}

static void LaunchAttack(void) {
  const FunctionalSettings *items = FunctionalGet();
  static const int list0[] = {A_RAIN, A_ARC, A_ROSES, A_RAIN};
  static const int list1[] = {A_RAIN, A_ARC, A_ROSES, A_ORB, A_ARC};
  static const int list2[] = {A_RAIN, A_ARC, A_ROSES, A_ORB, A_ROSES};
  int p = Phase();

  /* apres la (ou les) attaque(s) normale(s), la lune frappe */
  if (regularLeft <= 0) {
    StartBeam();
    regularLeft = RegularsPerBeam();
    attackTimer = beamWarn + 0.9f;
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
  case A_RAIN:  AtkRain(); extra = 1.0f; break;
  case A_ARC:   AtkArc(); extra = (1 + p) * 0.5f + 1.0f; break;
  case A_ROSES: AtkRoses(); extra = 2.0f; break;
  case A_ORB:   AtkOrb(); extra = 3.0f; break;
  }
  attackTimer = ((1.6f - 0.2f * p) / Intensity() + extra) * items->sealSpawn;
}

/* ============================== demarrage ============================== */

void Boss2Start(int floorNumberArg, int d) {
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
  state = BOSS2_PLAYING;
  timeLeft = DURATION + items->duration;
  attackTimer = 0.5f;
  invincible = step = dashTime = dashCooldown = 0;
  introTime = transition = dying = shake = flash = bossFlash = 0;
  rainLeft = rainTimer = 0;
  popupT = 0;
  beamOn = false;
  beamFired = false;
  beamT = 0;
  mirrorCount = 0;
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
}

static bool Touches(Hazard *h) {
  float r = 10 * unit * FunctionalGet()->hitbox;
  float dx = player.x - h->pos.x, dy = player.y - h->pos.y;
  float d = hypotf(dx, dy);

  switch (h->kind) {
  case H_BLAST:
  case H_ROSE:
  case H_ORB:
    return d < h->size + r;
  case H_ARC: {
    bool onBand = fabsf(d - h->size) < h->width / 2 + r;
    float diff = fabsf(remainderf(atan2f(dy, dx) - h->angle, 2 * PI));
    return onBand && diff < h->gap + r / fmaxf(d, r);
  }
  }
  return false;
}

static void UpdateHazard(Hazard *h, float dt) {
  h->t += dt;
  if (h->t < 0)
    return;
  bool firing = h->t >= h->warn;

  if (firing && !h->fired) {
    h->fired = true;
    shake = fmaxf(shake, (h->kind == H_ARC ? 6 : 4) * unit);
    if (h->kind == H_ROSE)
      SpawnPetals(h);
  }

  if (h->kind == H_ARC) {
    if (firing) {
      h->size += h->speed * dt;
      if (h->size > h->reach) {
        h->on = false;
        return;
      }
    }
  } else if (h->t > h->warn + h->life) {
    h->on = false;
    return;
  }

  if (h->kind == H_ORB && firing) {
    h->pos.x += cosf(h->angle) * h->speed * dt;
    h->pos.y += sinf(h->angle) * h->speed * dt;
    float dx = h->pos.x - player.x, dy = h->pos.y - player.y;
    float dist = hypotf(dx, dy);
    if (dist < h->reach && dist > 1 && dashTime <= 0 && transition <= 0 &&
        dying <= 0) {                  /* attraction vers l'orbe */
      float pull = 140 * unit * (1 - dist / h->reach) * dt;
      float margin = 28 * unit;
      player.x = Limit(player.x + dx / dist * pull, arena.x + margin,
                       arena.x + arena.width - margin);
      player.y = Limit(player.y + dy / dist * pull, arena.y + margin,
                       arena.y + arena.height - margin);
    }
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
    if (b->kind == 0)                  /* les larmes ondulent legerement */
      b->pos.x += sinf(clock * 3 + b->pos.y * 0.03f) * 18 * unit * dt;
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

static void Update(float dt) {
  invincible -= dt;
  dashTime -= dt;
  dashCooldown -= dt;
  shake *= expf(-dt * 6);
  flash = fmaxf(0, flash - dt * 1.5f);
  bossFlash = fmaxf(0, bossFlash - dt * 2.5f);
  popupT = fmaxf(0, popupT - dt);
  if (burstT >= 0) {
    burstT += dt;
    if (burstT > BURST_TIME)
      burstT = -1;
  }
  shownHp += ((1.0f - (float)collected / TOTAL_REFLECTS) - shownHp) * fminf(1, dt * 4);

  /* la deesse flotte, avec un lent balancement */
  bossPos.y = arena.y + arena.height * 0.28f;
  float sway = sinf(clock * 0.45f) * arena.width * 0.08f;
  bossPos.x += (bossTargetX + sway - bossPos.x) * fminf(1, dt * 2);

  MovePlayer(dt);

  if (dying > 0) {                      /* mort du boss : outro */
    dying -= dt;
    shake = fmaxf(shake, 3 * unit);
    if (dying <= 0)
      state = BOSS2_WON;
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
    if (rainLeft > 0) {
      rainLeft -= dt;
      rainTimer -= dt;
      if (rainTimer <= 0) {
        SpawnRainTick();
        rainTimer = 0.1f / Intensity();
      }
    }
    attackTimer -= dt;
    if (attackTimer <= 0 && rainLeft <= 0)
      LaunchAttack();
  }

  for (int i = 0; i < MAX_HAZARDS; i++)
    if (hazards[i].on)
      UpdateHazard(&hazards[i], dt);
  UpdateBullets(dt);
  UpdateBeam(dt);

  if (timeLeft <= 0 && state == BOSS2_PLAYING)
    state = BOSS2_LOST;
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
  Color lun = {200, 180, 255, 255};
  Color gold = {255, 200, 110, 255};

  float introP = Clamp01(introTime / INTRO_TIME);
  float ease = 1 - powf(1 - introP, 3);
  float alpha = Clamp01(introTime / 1.8f);
  if (dying > 0 && dying < DYING_TIME - 1.2f)
    alpha = Clamp01(dying / (DYING_TIME - 1.2f));
  float rise = (1 - ease) * arena.height * 0.12f;
  float tremble = dying > 0 ? sinf(clock * 80) * 4 * unit : 0;
  tremble += bossFlash * sinf(clock * 90) * 3 * unit;

  Vector2 c = {bossPos.x + tremble,
               bossPos.y + sinf(clock * 1.4f) * 5 * unit - rise};
  /* l'eclat de lune se charge : les orbes brillent de plus en plus */
  float charge = (beamOn && !beamFired) ? Clamp01(beamT / beamWarn) : 0;

  BeginBlendMode(BLEND_ADDITIVE);   /* aura lunaire, de plus en plus ecarlate */
  for (int l = 0; l < 6; l++)
    DrawCircleV((Vector2){c.x, c.y + h * 0.05f}, h * (0.22f + 0.1f * l),
                Fade(phase >= 2 ? (Color){200, 50, 90, 255} : (Color){130, 100, 200, 255},
                     (0.03f + 0.015f * phase) * alpha));
  for (int i = 0; i < 20; i++) {    /* poussiere de lune qui monte */
    float ph = fmodf(clock * 0.28f + Rand01(i * 3), 1.0f);
    DrawCircleV((Vector2){c.x + (Rand01(i * 3 + 1) - 0.5f) * w,
                          c.y + h * 0.4f - ph * h * 0.9f},
                (1 + 2 * Rand01(i * 3 + 2)) * unit,
                Fade(lun, (1 - ph) * 0.8f * alpha));
  }
  /* halo de la deesse */
  Vector2 halo = {c.x, c.y - h * 0.36f};
  DrawCircleV(halo, h * 0.16f, Fade(lun, (0.08f + 0.12f * charge) * alpha));
  /* orbe d'or du sceptre (gauche) et orbe noire (droite) */
  Vector2 staff = {c.x - w * 0.31f, c.y - h * 0.35f};
  Vector2 dark = {c.x + w * 0.275f, c.y - h * 0.27f};
  float pulse = 0.5f + 0.5f * sinf(clock * 4);
  DrawCircleV(staff, h * (0.07f + 0.08f * charge + 0.01f * pulse),
              Fade(gold, (0.25f + 0.5f * charge) * alpha));
  DrawCircleV(dark, h * (0.08f + 0.02f * phase + 0.015f * pulse),
              Fade((Color){170, 60, 200, 255}, (0.15f + 0.1f * phase) * alpha));
  for (int i = 0; i < 6; i++) {
    float a = clock * (0.8f + 0.3f * phase) + i * PI / 3;
    DrawLineEx(Polar(dark, a, h * 0.08f), Polar(dark, a, h * 0.14f), 2 * unit,
               Fade((Color){200, 90, 220, 255}, 0.4f * alpha));
  }
  EndBlendMode();

  if (bossTex.id == 0) {
    DrawCircleV(c, h * 0.3f, Fade((Color){110, 40, 90, 255}, alpha));
    return;
  }
  float f = bossFlash;
  Color tint = {255, (unsigned char)(255 - 110 * f), (unsigned char)(255 - 110 * f),
                (unsigned char)(255 * alpha)};
  Rectangle src = {0, 0, (float)bossTex.width, (float)bossTex.height};
  Rectangle dst = {c.x - w / 2, c.y - h / 2, w, h};
  DrawTexturePro(bossTex, src, dst, (Vector2){0, 0}, 0, tint);

  if (bossFlash > 0) {              /* eclat blanc quand elle encaisse */
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(bossTex, src, dst, (Vector2){0, 0}, 0,
                   Fade(WHITE, bossFlash * 0.5f * alpha));
    EndBlendMode();
  }
}

static void DrawMirror(Vector2 c, float r, float glow) {
  Color lun = {210, 195, 255, 255};
  BeginBlendMode(BLEND_ADDITIVE);
  DrawCircleV(c, r * 1.5f, Fade(lun, 0.08f + 0.12f * glow));
  DrawCircleV(c, r, Fade(lun, 0.14f + 0.22f * glow));
  for (int i = 0; i < 4; i++) {      /* reperes qui tournent */
    float a = clock * 1.2f + i * PI / 2;
    DrawLineEx(Polar(c, a, r * 1.05f), Polar(c, a, r * 1.3f), 2 * unit,
               Fade(WHITE, 0.4f + 0.4f * glow));
  }
  EndBlendMode();
  DrawRing(c, r - 3 * unit, r, 0, 360, 48, Fade(WHITE, 0.5f + 0.5f * glow));
  DrawCircleSector(c, r * 0.42f, 90, 270, 24, Fade(WHITE, 0.6f + 0.3f * glow));
}

static void DrawBeam(void) {
  if (!beamOn)
    return;
  Color lun = {200, 180, 255, 255};
  float p = Clamp01(beamT / beamWarn);

  if (!beamFired) {
    float pulse = 0.5f + 0.5f * sinf(clock * (8 + 14 * p));
    DrawRectangleRec(arena, Fade(lun, 0.03f + 0.14f * p * (0.5f + 0.5f * pulse)));
    for (int i = 0; i < mirrorCount; i++) {
      DrawMirror(mirrors[i].pos, mirrorR, p);
      DrawCircleLinesV(mirrors[i].pos, mirrorR * (1 + (1 - p) * 1.2f),
                       Fade(WHITE, 0.5f));
    }
  } else {                              /* la lumiere mortelle inonde l'arene */
    float u = Clamp01((beamT - beamWarn) / BEAM_FLASH);
    DrawRectangleRec(arena, Fade(lun, 0.6f * (1 - u)));
    for (int i = 0; i < mirrorCount; i++)
      DrawMirror(mirrors[i].pos, mirrorR, 1);
  }
}

static void DrawHazard(Hazard *h) {
  if (h->t < 0)
    return;
  Color lun = {200, 180, 255, 255};
  Color crimson = {200, 30, 60, 255};
  Color violet = {170, 60, 200, 255};
  bool firing = h->t >= h->warn;
  float p = Clamp01(h->t / h->warn);
  float u = Clamp01((h->t - h->warn) / h->life), al = 1 - u;

  switch (h->kind) {
  case H_BLAST: {
    if (!firing) {
      DrawCircleV(h->pos, h->size * p, Fade(lun, 0.3f));
      DrawCircleLinesV(h->pos, h->size, Fade(lun, 0.9f));
    } else {
      float r = h->size * (0.85f + 0.25f * u);
      BeginBlendMode(BLEND_ADDITIVE);
      DrawCircleV(h->pos, r, Fade(lun, 0.6f * al));
      DrawCircleV(h->pos, r * 0.45f, Fade(WHITE, 0.8f * al));
      EndBlendMode();
      DrawRing(h->pos, r * 0.95f, r * 1.05f, 0, 360, 48, Fade(WHITE, al));
    }
    break;
  }
  case H_ROSE: {
    if (!firing) {
      DrawCircleV(h->pos, h->size * p, Fade(crimson, 0.4f));
      DrawCircleLinesV(h->pos, h->size, Fade(RED, 0.9f));
      DrawPolyLines(h->pos, 8, h->size * 0.6f * p, clock * 40, Fade(lun, 0.7f));
    } else {
      float r = h->size * (0.85f + 0.25f * u);
      BeginBlendMode(BLEND_ADDITIVE);
      DrawCircleV(h->pos, r, Fade(crimson, 0.7f * al));
      for (int i = 0; i < 8; i++) {   /* petales de la rose */
        float a = clock * 1.2f + i * PI / 4;
        DrawLineEx(Polar(h->pos, a, r * 0.2f), Polar(h->pos, a, r * 1.3f),
                   4 * unit, Fade((Color){255, 90, 110, 255}, al));
      }
      DrawCircleV(h->pos, r * 0.45f, Fade(WHITE, 0.8f * al));
      EndBlendMode();
      DrawRing(h->pos, r * 0.95f, r * 1.05f, 0, 360, 48, Fade(RED, al));
    }
    break;
  }
  case H_ARC: {
    float deg = h->angle * RAD2DEG, half = h->gap * RAD2DEG;
    if (!firing) {                      /* couloir de passage du croissant */
      DrawLineEx(h->pos, Polar(h->pos, h->angle - h->gap, 400 * unit), 2 * unit,
                 Fade(lun, 0.4f * p));
      DrawLineEx(h->pos, Polar(h->pos, h->angle + h->gap, 400 * unit), 2 * unit,
                 Fade(lun, 0.4f * p));
      DrawRing(h->pos, 30 * unit, 36 * unit, deg - half, deg + half, 24,
               Fade(lun, 0.8f * p));
    } else {
      BeginBlendMode(BLEND_ADDITIVE);
      DrawRing(h->pos, h->size - h->width, h->size + h->width, deg - half,
               deg + half, 48, Fade(lun, 0.5f));
      EndBlendMode();
      DrawRing(h->pos, h->size - h->width / 2, h->size + h->width / 2, deg - half,
               deg + half, 48, WHITE);
    }
    break;
  }
  case H_ORB: {
    if (!firing) {
      DrawLineEx(h->pos, Polar(h->pos, h->angle, 400 * unit), 2 * unit,
                 Fade(violet, 0.5f * p));
      DrawCircleV(h->pos, h->size * p, Fade(violet, 0.5f));
    } else {
      float fadeOut = Clamp01((h->life - (h->t - h->warn)) / 0.4f);
      BeginBlendMode(BLEND_ADDITIVE);
      DrawCircleLinesV(h->pos, h->reach, Fade(violet, 0.25f * fadeOut));
      DrawCircleV(h->pos, h->size * 1.8f, Fade(violet, 0.3f * fadeOut));
      for (int i = 0; i < 4; i++) {
        float a = clock * 3 + i * PI / 2;
        DrawLineEx(Polar(h->pos, a, h->size), Polar(h->pos, a, h->size * 1.9f),
                   2 * unit, Fade(lun, 0.7f * fadeOut));
      }
      EndBlendMode();
      DrawCircleV(h->pos, h->size, Fade((Color){15, 0, 25, 255}, fadeOut));
      DrawRing(h->pos, h->size - 2 * unit, h->size, 0, 360, 32,
               Fade(violet, fadeOut));
    }
    break;
  }
  }
}

static void DrawBullets(void) {
  const FunctionalSettings *items = FunctionalGet();
  float size = 6 * unit * items->bulletSize;
  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (!b->on)
      continue;
    float sp = fmaxf(hypotf(b->vel.x, b->vel.y), 1);
    Vector2 tail = {b->pos.x - b->vel.x / sp * size * 4,
                    b->pos.y - b->vel.y / sp * size * 4};
    Color c = b->kind ? (Color){230, 50, 80, 255} : (Color){200, 190, 255, 255};
    DrawLineEx(tail, b->pos, size * 0.9f, Fade(c, 0.45f));
    DrawCircleV(b->pos, size * 1.8f, Fade(c, 0.3f));
  }
  EndBlendMode();
  for (int i = 0; i < MAX_BULLETS; i++)
    if (bullets[i].on)
      DrawCircleV(bullets[i].pos, size,
                  bullets[i].kind ? (Color){240, 60, 90, 255}
                                  : (Color){235, 230, 255, 255});
}

static void DrawBurst(void) {
  if (burstT < 0)
    return;
  Color lun = {215, 200, 255, 255};
  float u = burstT / BURST_TIME, fade = 1 - u;
  BeginBlendMode(BLEND_ADDITIVE);
  if (burstT < 0.3f)                 /* la lumiere renvoyee jusqu'a la deesse */
    DrawLineEx(burstPos, bossPos, (1 - burstT / 0.3f) * 12 * unit,
               Fade(WHITE, 1 - burstT / 0.3f));
  float rr = unit * (20 + u * 260);
  DrawRing(burstPos, rr - 5 * unit, rr + 5 * unit, 0, 360, 64, Fade(lun, fade));
  for (int i = 0; i < 60; i++) {
    float a = Rand01(i * 2) * 2 * PI;
    float sp = (120 + 380 * Rand01(i * 2 + 1)) * unit;
    Vector2 p = Polar(burstPos, a, sp * burstT * (1.1f - 0.4f * burstT));
    p.y += 200 * unit * burstT * burstT;
    DrawCircleV(p, (1.5f + 2.5f * Rand01(i)) * unit * fade,
                Fade(i % 3 == 0 ? WHITE : lun, fade));
  }
  if (burstBig) {
    float r2 = unit * (30 + u * 520);
    DrawRing(bossPos, r2 - 8 * unit, r2 + 8 * unit, 0, 360, 96,
             Fade((Color){200, 50, 100, 255}, fade));
    for (int i = 0; i < 90; i++) {
      float a = Rand01(i * 5 + 7) * 2 * PI;
      float sp = (150 + 450 * Rand01(i * 5 + 8)) * unit;
      Vector2 p = Polar(bossPos, a, sp * burstT * (1.1f - 0.4f * burstT));
      p.y += 160 * unit * burstT * burstT;
      DrawCircleV(p, (2 + 3 * Rand01(i + 9)) * unit * fade,
                  Fade(i % 2 ? (Color){240, 80, 110, 255} : lun, fade));
    }
  }
  EndBlendMode();
}

static void DrawArena(void) {
  int phase = Phase();
  Color lun = {200, 180, 255, 255};

  DrawRectangleGradientV((int)arena.x, (int)arena.y, (int)arena.width,
                         (int)arena.height, Fade(BLACK, 0),
                         Fade((Color){55, 15, 80, 255}, 0.22f + 0.1f * phase));

  /* sceau lunaire grave au sol : huit lunes qui tournent doucement */
  Vector2 sigil = {arena.x + arena.width / 2, arena.y + arena.height * 0.62f};
  float spin = clock * (0.1f + 0.06f * phase);
  BeginBlendMode(BLEND_ADDITIVE);
  DrawCircleLinesV(sigil, 150 * unit, Fade(lun, 0.09f));
  DrawCircleLinesV(sigil, 105 * unit, Fade(lun, 0.07f));
  for (int i = 0; i < 8; i++) {
    float a = spin + i * PI / 4;
    Vector2 m = Polar(sigil, a, 128 * unit);
    DrawCircleV(m, (6 + 2 * sinf(i + clock)) * unit, Fade(lun, 0.10f));
    DrawLineEx(Polar(sigil, a, 105 * unit), Polar(sigil, a, 150 * unit), 2 * unit,
               Fade(lun, 0.08f));
  }
  EndBlendMode();

  /* poussieres argentees et petales ecarlates */
  for (int i = 0; i < 30; i++) {
    float ph = fmodf(clock * (0.05f + 0.07f * Rand01(i * 4)) + Rand01(i * 4 + 1), 1);
    float x = arena.x + arena.width * Rand01(i * 4 + 2) + sinf(clock * 1.3f + i) * 18 * unit;
    float y = arena.y + arena.height * ph;
    float size = (1.5f + 2.5f * Rand01(i * 4 + 3)) * unit;
    Color col = i % 3 ? (Color){200, 190, 240, 255} : (Color){170, 25, 55, 255};
    DrawEllipse((int)x, (int)y, size * 1.5f, size, Fade(col, 0.4f));
  }

  DrawBoss();

  for (int i = 0; i < MAX_HAZARDS; i++)
    if (hazards[i].on)
      DrawHazard(&hazards[i]);
  DrawBullets();
  DrawBeam();

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

  if (popupT > 0) {                  /* texte flottant du reflet */
    const char *txt = "REFLET !";
    int fs = GameUiPixels(22);
    float rise = (1 - popupT) * 40 * unit;
    DrawText(txt, (int)(popupPos.x - MeasureText(txt, fs) / 2.0f),
             (int)(popupPos.y - 50 * unit - rise), fs,
             Fade((Color){225, 215, 255, 255}, Clamp01(popupT * 1.5f)));
  }
}

static void DrawHud(Rectangle a) {
  float ui = GameUiScale();
  int hudSize = GameUiPixels(20);

  for (int i = 0; i < lives; i++)
    DrawHeart((Vector2){a.x + (16 + i * 28) * ui, a.y + 16 * ui}, 20 * ui);

  /* barre de vie du boss, avec reperes de phase */
  DrawCentered(a, "SELENA, DEESSE DECHUE", 16, a.y + GameUiPixels(6),
               (Color){241, 213, 147, 255});
  Rectangle bar = {a.x + a.width * 0.25f, a.y + GameUiPixels(28),
                   a.width * 0.5f, (float)GameUiPixels(10)};
  DrawRectangleRec(bar, (Color){25, 10, 35, 255});
  DrawRectangleRec((Rectangle){bar.x, bar.y, bar.width * Clamp01(shownHp), bar.height},
                   (Color){150, 110, 225, 255});
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
    DrawCentered(a, "SELENA", 56, a.y + a.height * 0.5f - GameUiPixels(44),
                 Fade((Color){241, 213, 147, 255}, al));
    DrawCentered(a, "DEESSE DECHUE", 24, a.y + a.height * 0.5f + GameUiPixels(18),
                 Fade((Color){190, 150, 240, 255}, al));
  } else if (collected == 0 && beamOn && !beamFired) {
    DrawCentered(a, "LA LUNE VA FRAPPER : TENEZ-VOUS DANS UN MIROIR", 16,
                 a.y + a.height - GameUiPixels(50), Fade(GOLD, 0.95f));
  }

  if (transition > 0 && dying <= 0) {          /* titre de phase */
    float p = 1 - transition / TRANSITION_TIME;
    float al = Clamp01(fminf(p / 0.2f, (1 - p) / 0.25f));
    int ph = Phase();
    DrawCentered(a, phaseTitles[ph], 40, a.y + a.height * 0.5f - GameUiPixels(40),
                 Fade((Color){241, 213, 147, 255}, al));
    DrawCentered(a, phaseNames[ph], 22, a.y + a.height * 0.5f + GameUiPixels(12),
                 Fade((Color){190, 150, 240, 255}, al));
  }

  if (state != BOSS2_PLAYING) {
    const char *title = state == BOSS2_WON ? "VICTOIRE" : "GAME OVER";
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

void Boss2Update(Rectangle a) {
  if (state == BOSS2_OFF)
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

  if (state == BOSS2_PLAYING)
    Update(GetFrameTime());
  if (state != BOSS2_PLAYING && state != BOSS2_OFF && IsKeyPressed(KEY_ENTER))
    state = BOSS2_OFF;

  Draw(a);
}

Boss2Status Boss2State(void) {
  return state;
}

int Boss2DamageCount(void) {
  return damageCount;
}

void Boss2Unload(void) {
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
  state = BOSS2_OFF;
}
