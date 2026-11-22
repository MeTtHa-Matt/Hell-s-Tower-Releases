#include "boss3.h"
#include "../balance.h"
#include "../../items/functionnal/functionnal.h"
#include "../../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define BOSS_TEXTURE_PATH "assets/bosses/Agarès, Démon de la Vengeance.png"

#define MAX_HAZARDS 64
#define MAX_BULLETS 120
#define TRAIL 48
#define TRAIL_STEP 0.12f
#define DURATION 150.0f
#define TOTAL_RIPOSTES 9     /* 3 phases x 3 ripostes */
#define PHASE_RIPOSTES 3
#define PARRY_HAZARD 0.5f    /* jauge gagnee en esquivant une attaque */
#define PARRY_BULLET 0.125f  /* jauge gagnee en frolant une meche */
#define PARRY_SLACK 1.7f     /* tolerance de la zone de riposte */
#define ACTIVE 0.35f
#define DASH_TIME 0.22f
#define DASH_COOLDOWN 1.2f
#define INTRO_TIME 3.4f
#define TRANSITION_TIME 2.6f
#define DYING_TIME 3.8f
#define BURST_TIME 1.6f

enum { H_SWEEP, H_RING, H_BLAST, H_BEAM };
enum { A_SWEEP, A_HALO, A_TENDRILS, A_ECHO, A_CHAINS };
enum { FRONT, BACK, LEFT, RIGHT };

typedef struct {
  int kind;
  Vector2 pos;
  float angle, size, width, warn, t, life, speed, gap, reach;
  bool on, hit, fired;
} Hazard;

typedef struct {
  Vector2 pos, vel;
  float life, turn;
  bool on;
} Bullet;

static const char *dirNames[4] = {"front", "back", "left", "right"};
static const char *footNames[2] = {"left", "right"};
static const char *phaseTitles[3] = {"PHASE I", "PHASE II", "PHASE III"};
static const char *phaseNames[3] = {"LA LAME EPINEUSE",
                                    "LES MECHES DE RANCUNE",
                                    "SOLEIL DE VENGEANCE"};

static Hazard hazards[MAX_HAZARDS];
static Bullet bullets[MAX_BULLETS];
static Vector2 trail[TRAIL];
static Texture2D heartTex, shadowTex, bossTex, sprites[4][2];
static Rectangle arena;
static float unit;
static Vector2 player, dir, bossPos, burstPos, popupPos;
static bool ready, moving, burstBig;
static Boss3Status state;
static int floorNumber, difficulty, lives, maxLives, damageCount, collected,
    sealWards, facing, lastAttack, hairWaves, trailHead;
static float timeLeft, attackTimer, invincible, step, dashTime, dashCooldown,
    introTime, transition, dying, shake, flash, bossFlash, burstT, shownHp,
    clock, hairTimer, trailTimer, vengeance, popupT, bossBaseX, bossTargetX;

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
  int p = collected / PHASE_RIPOSTES;
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

static float AimAngle(Vector2 from, float lead) {
  Vector2 t = player;
  if (moving) {
    t.x += dir.x * lead;
    t.y += dir.y * lead;
  }
  return atan2f(t.y - from.y, t.x - from.x);
}

/* Position du joueur il y a `age` secondes (pour les echos). */
static Vector2 TrailAt(float age) {
  int idx = (int)(age / TRAIL_STEP);
  if (idx >= TRAIL)
    idx = TRAIL - 1;
  return trail[(trailHead - idx + TRAIL) % TRAIL];
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
    state = BOSS3_LOST;
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

static void AddBullet(Vector2 pos, Vector2 vel, float turn) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].on)
      continue;
    bullets[i] = (Bullet){pos, vel, 4.0f, turn, true};
    return;
  }
}

static void MoveBossTo(float x) {
  bossBaseX = Limit(x, arena.x + arena.width * 0.2f,
                    arena.x + arena.width * 0.8f);
}

/* ============================== attaques ============================== */

/* Lame epineuse : la grande epee balaie un arc, il reste des zones sures. */
static void AtkSweep(void) {
  int p = Phase();
  MoveBossTo(player.x + RandRange(-60, 60) * unit);
  Vector2 o = {bossBaseX, bossPos.y};
  float life = 0.8f - 0.05f * p;
  float span = 0.85f + 0.12f * p;
  float sign = GetRandomValue(0, 1) ? 1.0f : -1.0f;

  Hazard *h = AddHazard(H_SWEEP, o, PI / 2 - sign * span, arena.height * 0.92f,
                        0.17f, Warn(1.1f));
  if (h) {
    h->life = life;
    h->speed = sign * 2 * span / life;
    h->gap = h->angle;               /* angle de depart (pour l'avertissement) */
  }
  if (p >= 1) {                      /* retour de lame, sens inverse */
    Hazard *b = AddHazard(H_SWEEP, o, PI / 2 + sign * span,
                          arena.height * 0.92f, 0.17f, Warn(1.1f));
    if (b) {
      b->life = life;
      b->speed = -sign * 2 * span / life;
      b->gap = b->angle;
      b->t = -(life + 0.2f);
    }
  }
}

/* Anneaux solaires : ondes issues de l'orbe, avec une breche (ou sans). */
static void AtkHalo(void) {
  const FunctionalSettings *items = FunctionalGet();
  int p = Phase();
  int n = p >= 2 ? 2 : 1;
  for (int i = 0; i < n; i++) {
    Hazard *h = AddHazard(H_RING, bossPos, 0, 0, 16 * unit, Warn(0.9f));
    if (!h)
      continue;
    h->speed = 190 * unit * Intensity() * items->bulletSpeed;
    h->gap = RandRange(0, 6.28f);
    /* demi-largeur de la breche ; >= PI : anneau complet (esquive obligatoire) */
    h->angle = (p >= 2 && i == 1) ? 4.0f : (p == 1 ? 0.6f : 0.5f);
    h->reach = MaxReach();
    h->life = 99;
    h->t = -i * 1.0f;
  }
}

/* Meches de rancune : cheveux a tete chercheuse. */
static void AtkTendrils(void) {
  hairWaves = 2 + (Phase() >= 2);
  hairTimer = 0;
}

static void SpawnHairWave(void) {
  const FunctionalSettings *items = FunctionalGet();
  int p = Phase();
  int n = 4 + p;
  float lo = BalanceRandomSpeedMin(floorNumber, difficulty, 100.0f);
  float hi = BalanceRandomSpeedMax(floorNumber, difficulty, 100.0f);
  float speed = RandRange(lo, hi) * unit * items->bulletSpeed;
  float base = AimAngle(bossPos, 40 * unit);
  for (int i = 0; i < n; i++) {
    float a = base + (i - (n - 1) / 2.0f) * 0.3f;
    AddBullet(Polar(bossPos, a, 26 * unit),
              (Vector2){cosf(a) * speed, sinf(a) * speed}, 0.9f + 0.3f * p);
  }
  shake = fmaxf(shake, 3 * unit);
  hairWaves--;
  hairTimer = 0.75f;
}

/* Echos : la vengeance frappe la ou le joueur etait. */
static void AtkEcho(void) {
  int n = 4 + Phase();
  for (int i = 0; i < n; i++) {
    Vector2 p = TrailAt(0.4f + i * 0.35f);
    Hazard *h = AddHazard(H_BLAST, p, 0, 46 * unit, 0, Warn(1.2f));
    if (h)
      h->t = -i * 0.15f;
  }
  if (Phase() >= 2) {                /* une frappe sur la trajectoire */
    Vector2 p = {player.x + dir.x * 90 * unit, player.y + dir.y * 90 * unit};
    AddHazard(H_BLAST, p, 0, 52 * unit, 0, Warn(1.2f));
  }
}

/* Chaines : rayons en croix (puis diagonaux) centres sur le joueur. */
static void AtkChains(void) {
  int p = Phase();
  float reach = MaxReach();
  int count = p >= 1 ? 4 : 2;
  for (int i = 0; i < count; i++) {
    float a = PI / 2 * (i % 2) + (i >= 2 ? PI / 4 : 0);
    Hazard *h = AddHazard(H_BEAM, Polar(player, a + PI, reach / 2), a, reach,
                          16 * unit, Warn(1.1f));
    if (h)
      h->t = -i * 0.25f;
  }
}

static void LaunchAttack(void) {
  const FunctionalSettings *items = FunctionalGet();
  static const int list0[] = {A_SWEEP, A_TENDRILS, A_ECHO, A_SWEEP};
  static const int list1[] = {A_SWEEP, A_HALO, A_TENDRILS, A_ECHO, A_CHAINS};
  static const int list2[] = {A_SWEEP, A_HALO, A_TENDRILS,
                              A_ECHO,  A_CHAINS, A_HALO};
  int p = Phase();
  const int *list = p == 0 ? list0 : (p == 1 ? list1 : list2);
  int n = p == 0 ? 4 : (p == 1 ? 5 : 6);

  int attack = list[GetRandomValue(0, n - 1)];
  for (int tries = 0; tries < 3 && attack == lastAttack; tries++)
    attack = list[GetRandomValue(0, n - 1)];
  lastAttack = attack;

  bossBaseX = arena.x + arena.width / 2 + RandRange(-0.18f, 0.18f) * arena.width;

  float extra = 0;
  switch (attack) {
  case A_SWEEP:    AtkSweep(); extra = 1.0f + (p >= 1 ? 1.0f : 0); break;
  case A_HALO:     AtkHalo(); extra = p >= 2 ? 1.8f : 0.8f; break;
  case A_TENDRILS: AtkTendrils(); extra = 1.2f; break;
  case A_ECHO:     AtkEcho(); extra = 1.0f + (4 + p) * 0.15f; break;
  case A_CHAINS:   AtkChains(); extra = 1.0f; break;
  }

  attackTimer = ((2.2f - 0.3f * p) / Intensity() + extra) * items->sealSpawn;
}

/* ============================== riposte ============================== */

/* Le boss encaisse un coup : la vengeance est servie. */
static void Strike(void) {
  const FunctionalSettings *items = FunctionalGet();
  burstPos = player;
  burstT = 0;
  burstBig = false;
  collected++;
  vengeance = 0;
  popupT = 1.0f;
  popupPos = player;
  if (items->sealWard && collected % 2 == 0)
    sealWards++;
  bossFlash = 1;
  flash = 0.3f;
  shake = fmaxf(shake, 12 * unit);
  hairWaves = 0;
  memset(hazards, 0, sizeof(hazards));
  memset(bullets, 0, sizeof(bullets));

  if (collected >= TOTAL_RIPOSTES) {              /* mort du boss */
    dying = DYING_TIME;
    burstBig = true;
    flash = 0.9f;
  } else if (collected % PHASE_RIPOSTES == 0) {   /* changement de phase */
    transition = TRANSITION_TIME;
    burstBig = true;
    flash = 0.8f;
    attackTimer = TRANSITION_TIME + 1.0f;
  } else {
    attackTimer = 1.2f * items->sealSpawn;
  }
}

static void Riposte(float amount) {
  vengeance += amount;
  dashCooldown = fminf(dashCooldown, 0.5f);   /* enchainer les ripostes */
  if (vengeance >= 1.0f)
    Strike();
}

/* ============================== demarrage ============================== */

void Boss3Start(int floorNumberArg, int d) {
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
  state = BOSS3_PLAYING;
  timeLeft = DURATION + items->duration;
  attackTimer = 0.5f;
  invincible = step = dashTime = dashCooldown = 0;
  introTime = transition = dying = shake = flash = bossFlash = 0;
  hairWaves = 0;
  hairTimer = 0;
  vengeance = 0;
  popupT = 0;
  trailTimer = 0;
  trailHead = 0;
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

/* scale > 1 : zone elargie, utilisee pour detecter les ripostes */
static bool Touches(Hazard *h, float scale) {
  float r = 10 * unit * FunctionalGet()->hitbox * scale;
  float dx = player.x - h->pos.x, dy = player.y - h->pos.y;
  float d = hypotf(dx, dy);

  switch (h->kind) {
  case H_SWEEP: {
    if (d > h->size + r)
      return false;
    float diff = remainderf(atan2f(dy, dx) - h->angle, 2 * PI);
    return fabsf(diff) < h->width + r / fmaxf(d, r);
  }
  case H_BEAM: {
    Vector2 end = Polar(h->pos, h->angle, h->size);
    return NearSegment(player, h->pos, end, h->width / 2 + r);
  }
  case H_BLAST:
    return d < h->size + r;
  case H_RING: {
    bool onBand = fabsf(d - h->size) < h->width / 2 + r;
    bool outsideGap =
        fabsf(remainderf(atan2f(dy, dx) - h->gap, 2 * PI)) > h->angle;
    return onBand && outsideGap;
  }
  }
  return false;
}

static void UpdateHazard(Hazard *h, float dt) {
  h->t += dt;
  if (h->t < 0)
    return;
  bool firing = h->t >= h->warn;

  if (h->kind == H_RING) {
    if (firing) {
      if (!h->fired) {
        h->fired = true;
        shake = fmaxf(shake, 6 * unit);
      }
      h->size += h->speed * dt;
      if (h->size > h->reach) {
        h->on = false;
        return;
      }
    }
  } else {
    if (h->t > h->warn + h->life) {
      h->on = false;
      return;
    }
    if (firing && !h->fired) {
      h->fired = true;
      shake = fmaxf(shake, (h->kind == H_SWEEP ? 7 : 4) * unit);
    }
    if (firing && h->kind == H_SWEEP)
      h->angle += h->speed * dt;
  }

  if (!firing || h->hit || transition > 0 || dying > 0)
    return;

  /* esquive au contact de l'attaque : riposte ! */
  if (dashTime > 0) {
    if (Touches(h, PARRY_SLACK)) {
      h->hit = true;
      Riposte(PARRY_HAZARD);
    }
    return;
  }
  if (invincible > 0 || !Touches(h, 1.0f))
    return;
  h->hit = true;
  Hurt();
}

static void UpdateBullets(float dt) {
  const FunctionalSettings *items = FunctionalGet();
  float radius = 7 * unit * items->bulletSize + 9 * unit * items->hitbox;
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (!b->on)
      continue;

    b->life -= dt;
    if (b->life <= 0) {
      b->on = false;
      continue;
    }
    if (b->life > 2.2f) {              /* tete chercheuse, puis ligne droite */
      float cur = atan2f(b->vel.y, b->vel.x);
      float want = atan2f(player.y - b->pos.y, player.x - b->pos.x);
      float diff = remainderf(want - cur, 2 * PI);
      float turn = Limit(diff, -b->turn * dt, b->turn * dt);
      float sp = hypotf(b->vel.x, b->vel.y);
      b->vel = (Vector2){cosf(cur + turn) * sp, sinf(cur + turn) * sp};
    }
    b->pos.x += b->vel.x * dt;
    b->pos.y += b->vel.y * dt;

    float out = 40 * unit;
    if (b->pos.x < arena.x - out || b->pos.x > arena.x + arena.width + out ||
        b->pos.y < arena.y - out || b->pos.y > arena.y + arena.height + out) {
      b->on = false;
      continue;
    }
    if (transition > 0 || dying > 0)
      continue;
    float dist = hypotf(player.x - b->pos.x, player.y - b->pos.y);
    if (dashTime > 0) {                /* meche frolee en esquive */
      if (dist < radius * PARRY_SLACK) {
        b->on = false;
        Riposte(PARRY_BULLET);
      }
      continue;
    }
    if (invincible > 0)
      continue;
    if (dist < radius) {
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
  shownHp += ((1.0f - (float)collected / TOTAL_RIPOSTES) - shownHp) * fminf(1, dt * 4);

  /* le boss glisse lentement, avec un leger balancement */
  bossPos.y = arena.y + arena.height * 0.28f;
  bossTargetX = bossBaseX + sinf(clock * 0.6f) * arena.width * 0.04f;
  bossPos.x += (bossTargetX - bossPos.x) * fminf(1, dt * 2.5f);

  MovePlayer(dt);

  /* trace des positions du joueur (pour les echos) */
  trailTimer -= dt;
  if (trailTimer <= 0) {
    trailTimer = TRAIL_STEP;
    trailHead = (trailHead + 1) % TRAIL;
    trail[trailHead] = player;
  }

  if (dying > 0) {                      /* mort du boss : outro */
    dying -= dt;
    shake = fmaxf(shake, 3 * unit);
    if (dying <= 0)
      state = BOSS3_WON;
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
    if (hairWaves > 0) {
      hairTimer -= dt;
      if (hairTimer <= 0)
        SpawnHairWave();
    }
    attackTimer -= dt;
    if (attackTimer <= 0 && hairWaves <= 0)
      LaunchAttack();
  }

  for (int i = 0; i < MAX_HAZARDS; i++)
    if (hazards[i].on)
      UpdateHazard(&hazards[i], dt);
  UpdateBullets(dt);

  if (timeLeft <= 0 && state == BOSS3_PLAYING)
    state = BOSS3_LOST;
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
  float h = arena.height * 0.5f;
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

  Vector2 c = {bossPos.x + tremble,
               bossPos.y + sinf(clock * 1.5f) * 5 * unit - rise};

  BeginBlendMode(BLEND_ADDITIVE);   /* aura cramoisie, de plus en plus ardente */
  for (int l = 0; l < 6; l++)
    DrawCircleV((Vector2){c.x, c.y + h * 0.05f}, h * (0.22f + 0.1f * l),
                Fade((Color){190, 25, 45, 255}, (0.03f + 0.02f * phase) * alpha));
  for (int i = 0; i < 18; i++) {    /* braises de rancune qui montent */
    float ph = fmodf(clock * 0.3f + Rand01(i * 3), 1.0f);
    DrawCircleV((Vector2){c.x + (Rand01(i * 3 + 1) - 0.5f) * w,
                          c.y + h * 0.4f - ph * h * 0.9f},
                (1 + 2 * Rand01(i * 3 + 2)) * unit,
                Fade((Color){255, 120, 80, 255}, (1 - ph) * 0.8f * alpha));
  }
  /* orbe solaire du baton : pulse plus fort a chaque phase */
  Vector2 orb = {c.x + w * 0.33f, c.y - h * 0.30f};
  float pulse = 0.5f + 0.5f * sinf(clock * 4);
  DrawCircleV(orb, h * (0.10f + 0.03f * phase + 0.015f * pulse),
              Fade(gold, (0.15f + 0.08f * phase) * alpha));
  for (int i = 0; i < 8; i++) {
    float a = clock * (0.8f + 0.4f * phase) + i * PI / 4;
    DrawLineEx(Polar(orb, a, h * 0.1f), Polar(orb, a, h * (0.16f + 0.02f * phase)),
               2 * unit, Fade(gold, 0.45f * alpha));
  }
  EndBlendMode();

  if (bossTex.id == 0) {
    DrawCircleV(c, h * 0.3f, Fade((Color){120, 20, 40, 255}, alpha));
    return;
  }
  float f = bossFlash;
  Color tint = {255, (unsigned char)(255 - 110 * f), (unsigned char)(255 - 110 * f),
                (unsigned char)(255 * alpha)};
  Rectangle src = {0, 0, (float)bossTex.width, (float)bossTex.height};
  Rectangle dst = {c.x - w / 2, c.y - h / 2, w, h};
  DrawTexturePro(bossTex, src, dst, (Vector2){0, 0}, 0, tint);

  if (bossFlash > 0) {              /* eclat blanc quand il encaisse */
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(bossTex, src, dst, (Vector2){0, 0}, 0,
                   Fade(WHITE, bossFlash * 0.5f * alpha));
    EndBlendMode();
  }
}

static void DrawHazard(Hazard *h) {
  if (h->t < 0)
    return;
  Color crimson = {200, 25, 45, 255};
  Color gold = {255, 215, 120, 255};
  bool firing = h->t >= h->warn;
  float p = Clamp01(h->t / h->warn);
  float since = h->t - h->warn;
  float u = Clamp01(since / h->life), al = 1 - u;

  switch (h->kind) {
  case H_SWEEP: {
    float half = h->width * RAD2DEG;
    if (!firing) {                    /* on montre tout l'arc a l'avance */
      float a0 = h->gap, a1 = h->gap + h->speed * h->life;
      float lo = fminf(a0, a1) * RAD2DEG - half;
      float hi = fmaxf(a0, a1) * RAD2DEG + half;
      DrawCircleSector(h->pos, h->size, lo, hi, 40, Fade(crimson, 0.06f + 0.12f * p));
      DrawCircleSectorLines(h->pos, h->size, lo, hi, 40, Fade(RED, 0.5f));
      DrawLineEx(h->pos, Polar(h->pos, a0, h->size), (2 + 4 * p) * unit,
                 Fade(gold, 0.4f + 0.5f * p));
    } else {
      float fade = 1 - 0.5f * u;
      for (int k = 4; k >= 0; k--) {  /* traînee de la lame */
        float a = (h->angle - h->speed * 0.025f * k) * RAD2DEG;
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleSector(h->pos, h->size * 1.03f, a - half * 1.5f, a + half * 1.5f,
                         16, Fade(crimson, 0.5f * fade * (1 - k * 0.2f)));
        EndBlendMode();
        DrawCircleSector(h->pos, h->size, a - half, a + half, 16,
                         Fade(WHITE, fade * (1 - k * 0.2f)));
      }
      float a = h->angle * RAD2DEG;
      BeginBlendMode(BLEND_ADDITIVE);
      DrawRing(h->pos, h->size * 0.9f, h->size * 1.02f, a - half, a + half, 16,
               Fade(gold, fade));
      EndBlendMode();
    }
    break;
  }
  case H_BEAM: {
    Vector2 end = Polar(h->pos, h->angle, h->size);
    if (!firing) {
      DrawLineEx(h->pos, end, (2 + (h->width / unit - 2) * p * 0.5f) * unit,
                 Fade(RED, 0.4f + 0.4f * p));
    } else {
      BeginBlendMode(BLEND_ADDITIVE);
      DrawLineEx(h->pos, end, h->width * 2.2f, Fade(crimson, 0.45f * al));
      EndBlendMode();
      DrawLineEx(h->pos, end, h->width, Fade(WHITE, al));
    }
    break;
  }
  case H_BLAST: {
    if (!firing) {
      DrawCircleV(h->pos, h->size * p, Fade(crimson, 0.4f));
      DrawCircleLinesV(h->pos, h->size, Fade(gold, 0.9f));
      DrawCircleLinesV(h->pos, h->size * 0.5f, Fade(RED, 0.5f));
    } else {
      float r = h->size * (0.85f + 0.25f * u);
      BeginBlendMode(BLEND_ADDITIVE);
      DrawCircleV(h->pos, r, Fade(crimson, 0.7f * al));
      for (int i = 0; i < 10; i++) {   /* epines de vengeance */
        float a = clock * 1.4f + i * 2 * PI / 10;
        DrawLineEx(Polar(h->pos, a, r * 0.2f), Polar(h->pos, a, r * 1.25f),
                   3 * unit, Fade(gold, al));
      }
      DrawCircleV(h->pos, r * 0.45f, Fade(WHITE, 0.8f * al));
      EndBlendMode();
      DrawRing(h->pos, r * 0.95f, r * 1.05f, 0, 360, 48, Fade(RED, al));
    }
    break;
  }
  case H_RING: {
    bool full = h->angle >= PI;
    float gapDeg = h->gap * RAD2DEG, halfDeg = h->angle * RAD2DEG;
    if (!firing) {
      DrawCircleLinesV(h->pos, 45 * unit * (0.4f + 0.6f * p), Fade(RED, 0.8f));
      if (full) {                      /* anneau complet : il faut esquiver */
        DrawCircleLinesV(h->pos, 60 * unit * (0.4f + 0.6f * p), Fade(gold, 0.7f));
      } else {
        Vector2 a1 = Polar(h->pos, h->gap - h->angle, 170 * unit);
        Vector2 a2 = Polar(h->pos, h->gap + h->angle, 170 * unit);
        DrawLineEx(h->pos, a1, 2 * unit, Fade(gold, 0.6f * p));
        DrawLineEx(h->pos, a2, 2 * unit, Fade(gold, 0.6f * p));
      }
    } else {
      float start = full ? 0 : gapDeg + halfDeg;
      float end = full ? 360 : start + 360 - 2 * halfDeg;
      BeginBlendMode(BLEND_ADDITIVE);
      DrawRing(h->pos, h->size - h->width, h->size + h->width, start, end, 96,
               Fade(full ? gold : crimson, 0.5f));
      EndBlendMode();
      DrawRing(h->pos, h->size - h->width / 2, h->size + h->width / 2, start, end,
               96, WHITE);
    }
    break;
  }
  }
}

static void DrawBullets(void) {
  const FunctionalSettings *items = FunctionalGet();
  float size = 7 * unit * items->bulletSize;
  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (!b->on)
      continue;
    float sp = fmaxf(hypotf(b->vel.x, b->vel.y), 1);
    Vector2 tail = {b->pos.x - b->vel.x / sp * size * 5,
                    b->pos.y - b->vel.y / sp * size * 5};
    DrawLineEx(tail, b->pos, size * 1.1f, Fade((Color){190, 170, 220, 255}, 0.45f));
    DrawCircleV(b->pos, size * 1.8f, Fade((Color){200, 40, 70, 255}, 0.3f));
  }
  EndBlendMode();
  for (int i = 0; i < MAX_BULLETS; i++)
    if (bullets[i].on)
      DrawCircleV(bullets[i].pos, size, (Color){225, 215, 235, 255});
}

static void DrawBurst(void) {
  if (burstT < 0)
    return;
  Color gold = {255, 215, 120, 255};
  float u = burstT / BURST_TIME, fade = 1 - u;
  BeginBlendMode(BLEND_ADDITIVE);
  if (burstT < 0.3f)                 /* trait de la riposte jusqu'au boss */
    DrawLineEx(burstPos, bossPos, (1 - burstT / 0.3f) * 10 * unit,
               Fade(WHITE, 1 - burstT / 0.3f));
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
                  Fade(i % 2 ? (Color){255, 80, 70, 255} : gold, fade));
    }
  }
  EndBlendMode();
}

static void DrawArena(void) {
  int phase = Phase();
  Color gold = {255, 205, 100, 255};

  DrawRectangleGradientV((int)arena.x, (int)arena.y, (int)arena.width,
                         (int)arena.height, Fade(BLACK, 0),
                         Fade((Color){80, 10, 30, 255}, 0.2f + 0.1f * phase));

  /* sceau solaire grave au sol, a huit pointes */
  Vector2 sigil = {arena.x + arena.width / 2, arena.y + arena.height * 0.62f};
  float spin = clock * (0.12f + 0.08f * phase);
  BeginBlendMode(BLEND_ADDITIVE);
  DrawCircleLinesV(sigil, 140 * unit, Fade(gold, 0.09f));
  DrawCircleLinesV(sigil, 100 * unit, Fade(gold, 0.07f));
  for (int i = 0; i < 8; i++) {
    float a = spin + i * PI / 4;
    DrawLineEx(Polar(sigil, a, 100 * unit), Polar(sigil, a, 175 * unit),
               2 * unit, Fade(gold, 0.10f));
  }
  EndBlendMode();

  /* cendres et petales ecarlates */
  for (int i = 0; i < 30; i++) {
    float ph = fmodf(clock * (0.05f + 0.07f * Rand01(i * 4)) + Rand01(i * 4 + 1), 1);
    float x = arena.x + arena.width * Rand01(i * 4 + 2) + sinf(clock * 1.3f + i) * 18 * unit;
    float y = arena.y + arena.height * ph;
    float size = (1.5f + 2.5f * Rand01(i * 4 + 3)) * unit;
    Color col = i % 3 ? (Color){170, 25, 45, 255} : (Color){190, 175, 210, 255};
    DrawEllipse((int)x, (int)y, size * 1.6f, size, Fade(col, 0.4f));
  }

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

  /* jauge de vengeance autour du joueur */
  if (vengeance > 0 && dying <= 0) {
    BeginBlendMode(BLEND_ADDITIVE);
    DrawRing(player, 26 * unit, 30 * unit, -90, -90 + 360 * Clamp01(vengeance), 32,
             Fade(gold, 0.9f));
    EndBlendMode();
  }

  DrawBurst();

  if (popupT > 0) {                  /* texte flottant de la riposte */
    const char *txt = "RIPOSTE !";
    int fs = GameUiPixels(22);
    float rise = (1 - popupT) * 40 * unit;
    DrawText(txt, (int)(popupPos.x - MeasureText(txt, fs) / 2.0f),
             (int)(popupPos.y - 50 * unit - rise), fs,
             Fade((Color){255, 225, 140, 255}, Clamp01(popupT * 1.5f)));
  }
}

static void DrawHud(Rectangle a) {
  float ui = GameUiScale();
  int hudSize = GameUiPixels(20);

  for (int i = 0; i < lives; i++)
    DrawHeart((Vector2){a.x + (16 + i * 28) * ui, a.y + 16 * ui}, 20 * ui);

  /* barre de vie du boss, avec reperes de phase */
  DrawCentered(a, "AGARES, DEMON DE LA VENGEANCE", 16, a.y + GameUiPixels(6),
               (Color){241, 213, 147, 255});
  Rectangle bar = {a.x + a.width * 0.25f, a.y + GameUiPixels(28),
                   a.width * 0.5f, (float)GameUiPixels(10)};
  DrawRectangleRec(bar, (Color){30, 8, 18, 255});
  DrawRectangleRec((Rectangle){bar.x, bar.y, bar.width * Clamp01(shownHp), bar.height},
                   (Color){175, 30, 60, 255});
  for (int i = 1; i < 3; i++)
    DrawLineEx((Vector2){bar.x + bar.width * i / 3, bar.y},
               (Vector2){bar.x + bar.width * i / 3, bar.y + bar.height}, 2, GOLD);
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
    DrawCentered(a, "AGARES", 56, a.y + a.height * 0.5f - GameUiPixels(44),
                 Fade((Color){241, 213, 147, 255}, al));
    DrawCentered(a, "DEMON DE LA VENGEANCE", 24,
                 a.y + a.height * 0.5f + GameUiPixels(18),
                 Fade((Color){200, 50, 80, 255}, al));
  } else if (collected == 0 && introTime < INTRO_TIME + 7) {
    DrawCentered(a, "ESQUIVEZ AU CONTACT DES ATTAQUES (ESPACE) POUR RIPOSTER", 16,
                 a.y + a.height - GameUiPixels(50), Fade(GOLD, 0.9f));
  }

  if (transition > 0 && dying <= 0) {          /* titre de phase */
    float p = 1 - transition / TRANSITION_TIME;
    float al = Clamp01(fminf(p / 0.2f, (1 - p) / 0.25f));
    int ph = Phase();
    DrawCentered(a, phaseTitles[ph], 40, a.y + a.height * 0.5f - GameUiPixels(40),
                 Fade((Color){241, 213, 147, 255}, al));
    DrawCentered(a, phaseNames[ph], 22, a.y + a.height * 0.5f + GameUiPixels(12),
                 Fade((Color){220, 70, 90, 255}, al));
  }

  if (state != BOSS3_PLAYING) {
    const char *title = state == BOSS3_WON ? "VICTOIRE" : "GAME OVER";
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

void Boss3Update(Rectangle a) {
  if (state == BOSS3_OFF)
    return;

  float smallSide = a.width < a.height ? a.width : a.height;
  arena = a;
  unit = smallSide / 400;
  clock += GetFrameTime();

  if (!ready) {
    player = (Vector2){a.x + a.width / 2, a.y + a.height * 0.8f};
    bossPos = (Vector2){a.x + a.width / 2, a.y + a.height * 0.28f};
    bossBaseX = bossTargetX = bossPos.x;
    for (int i = 0; i < TRAIL; i++)
      trail[i] = player;
    ready = true;
  }

  if (state == BOSS3_PLAYING)
    Update(GetFrameTime());
  if (state != BOSS3_PLAYING && state != BOSS3_OFF && IsKeyPressed(KEY_ENTER))
    state = BOSS3_OFF;

  Draw(a);
}

Boss3Status Boss3State(void) {
  return state;
}

int Boss3DamageCount(void) {
  return damageCount;
}

void Boss3Unload(void) {
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
  state = BOSS3_OFF;
}
