#include "finalboss.h"
#include "../balance.h"
#include "../modifiers.h"
#include "../../items/functionnal/functionnal.h"
#include "../../ui_scale.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

/* Chemin de l'image du boss (renommez votre fichier sans accents). */
#define BOSS_TEXTURE_PATH "assets/boss/malenia.png"

#define MAX_HAZARDS 48
#define MAX_PETALS 220
#define DURATION 150.0f
#define TOTAL_SEALS 9      /* 3 phases x 3 sceaux */
#define PHASE_SEALS 3
#define SEAL_TIME 2.0f
#define ACTIVE 0.35f
#define DASH_TIME 0.22f
#define DASH_COOLDOWN 1.2f
#define INTRO_TIME 3.2f
#define TRANSITION_TIME 2.6f
#define DYING_TIME 3.6f
#define BURST_TIME 1.6f
#define PLAYER_SPEED_MALUS 0.75f
#define PLAYER_DASH_COOLDOWN_MALUS 1.5f
#define PLAYER_HITBOX_MALUS 1.4f

enum { H_SLASH, H_BEAM, H_COLUMN, H_BLAST, H_RING };
enum { A_SLASH, A_BEAMS, A_DANCE, A_PETALS, A_BLOOM, A_RING, A_COLUMNS };
enum { FRONT, BACK, LEFT, RIGHT };
enum { MALUS_SLOW, MALUS_DASH, MALUS_LIFE, MALUS_HITBOX };

typedef struct {
  int kind;
  Vector2 pos;
  float angle, size, width, warn, t, speed, gap, maxSize;
  bool on, hit, fired;
} Hazard;

typedef struct {
  Vector2 pos, vel;
  float curve;
  bool on;
} Petal;

static const char *dirNames[4] = {"front", "back", "left", "right"};
static const char *footNames[2] = {"left", "right"};
static const char *phaseTitles[3] = {"PHASE I", "PHASE II", "PHASE III"};
static const char *phaseNames[3] = {"LA LAME DE MIQUELLA", "LA DANSE DES EAUX",
                                    "FLORAISON ECARLATE"};
static const char *playerMalusNames[MINIGAME_MODIFIER_OPTIONS] = {
    "DEPLACEMENT REDUIT", "RECUPERATION ESQUIVE LONGUE", "VIE EN MOINS",
    "HITBOX AUGMENTEE"};

static Hazard hazards[MAX_HAZARDS];
static Petal petals[MAX_PETALS];
static bool playerMaluses[MINIGAME_MODIFIER_OPTIONS];
static Texture2D heartTex, sealTex, shadowTex, bossTex, sprites[4][2];
static Rectangle arena;
static float unit;
static Vector2 player, rune, dir, bossPos, burstPos;
static float bossTargetX;
static bool ready, moving, burstBig;
static FinalBossStatus state;
static int floorNumber, difficulty, lives, maxLives, damageCount, sealed,
    sealWards, facing, lastAttack, danceLeft, petalWaves, petalCount;
static float timeLeft, attackTimer, invincible, charge, step, dashTime,
    dashCooldown, introTime, transition, dying, shake, flash, bossFlash,
    burstT, shownHp, danceTimer, waveTimer, clock;

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
  int p = sealed / PHASE_SEALS;
  return p > 2 ? 2 : p;
}

static float Intensity(void) {
  return BalanceBossIntensity(floorNumber, difficulty);
}

static float PlayerHitboxScale(void) {
  return playerMaluses[MALUS_HITBOX] ? PLAYER_HITBOX_MALUS : 1.0f;
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
    state = FINALBOSS_LOST;
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
    hazards[i].on = true;
    return &hazards[i];
  }
  return NULL;
}

static void AddPetal(Vector2 pos, Vector2 vel, float curve) {
  for (int i = 0; i < MAX_PETALS; i++) {
    if (petals[i].on)
      continue;
    petals[i] = (Petal){pos, vel, curve, true};
    return;
  }
}

static void MoveBossTo(float x) {
  bossTargetX = Limit(x, arena.x + arena.width * 0.18f,
                      arena.x + arena.width * 0.82f);
}

static float AimAngle(Vector2 from, float lead) {
  Vector2 t = player;
  if (moving) {
    t.x += dir.x * lead;
    t.y += dir.y * lead;
  }
  return atan2f(t.y - from.y, t.x - from.x);
}

static float MaxReach(void) {
  return hypotf(arena.width, arena.height) * 1.1f;
}

static void PlaceRune(void) {
  float margin = 60 * unit;
  for (int i = 0; i < 20; i++) {
    rune.x = arena.x + margin + RandRange(0, 1) * (arena.width - 2 * margin);
    rune.y = arena.y + arena.height * 0.38f +
             RandRange(0, 1) * (arena.height * 0.62f - margin);
    if (hypotf(rune.x - player.x, rune.y - player.y) > 150 * unit)
      return;
  }
}

/* ============================== attaques ============================== */

/* Coup de faux : secteur en éventail, depuis la position du boss. */
static void AtkSlash(void) {
  MoveBossTo(player.x + RandRange(-80, 80) * unit);
  Vector2 o = {bossTargetX, bossPos.y};
  AddHazard(H_SLASH, o, AimAngle(o, 70 * unit), arena.height * 0.92f,
            0.36f + 0.02f * Phase(), Warn(1.0f));
  if (Phase() >= 1) {   /* second coup croisé, depuis l'autre côté */
    Vector2 o2 = {arena.x + arena.width - (o.x - arena.x), bossPos.y};
    Hazard *h = AddHazard(H_SLASH, o2, AimAngle(o2, 70 * unit),
                          arena.height * 0.92f, 0.34f, Warn(1.0f));
    if (h)
      h->t = -0.5f;
  }
}

/* Lames : éventail de rayons fins vers le joueur. */
static void AtkBeams(void) {
  int n = 3 + (Phase() >= 2) + (difficulty >= 8);
  float base = AimAngle(bossPos, 50 * unit);
  for (int i = 0; i < n; i++) {
    float a = base + (i - (n - 1) / 2.0f) * 0.34f;
    AddHazard(H_BEAM, bossPos, a, MaxReach(), 16 * unit, Warn(1.0f));
  }
}

/* Danse : le boss bondit d'un point à l'autre en lançant des coups. */
static void AtkDance(void) {
  danceLeft = Phase() >= 2 ? 6 : 4;
  danceTimer = 0;
}

static void DanceStep(void) {
  float x = bossTargetX;
  for (int i = 0; i < 8 && fabsf(x - bossTargetX) < arena.width * 0.25f; i++)
    x = arena.x + arena.width * RandRange(0.18f, 0.82f);
  MoveBossTo(x);
  Vector2 o = {bossTargetX, bossPos.y};
  AddHazard(H_SLASH, o, AimAngle(o, 80 * unit), arena.height * 0.95f, 0.34f,
            Warn(0.7f));
  shake = fmaxf(shake, 3 * unit);
  danceLeft--;
  danceTimer = fmaxf(0.38f, 0.62f / Intensity());
}

/* Pétales : vagues radiales qui tourbillonnent. */
static void AtkPetals(void) {
  petalWaves = Phase() >= 2 ? 3 : 2;
  petalCount = Phase() >= 2 ? 20 : 14;
  waveTimer = 0;
}

static void SpawnPetalWave(void) {
  const FunctionalSettings *items = FunctionalGet();
  float offset = RandRange(0, 6.28f);
  float lo = BalanceRandomSpeedMin(floorNumber, difficulty, 120.0f);
  float hi = BalanceRandomSpeedMax(floorNumber, difficulty, 120.0f);
  for (int i = 0; i < petalCount; i++) {
    float a = offset + i * 2 * PI / petalCount;
    float speed = RandRange(lo, hi) * unit * items->bulletSpeed;
    float curve = (i % 2 ? 0.35f : -0.35f) * (Phase() >= 2 ? 1.0f : 0.5f);
    AddPetal(Polar(bossPos, a, 24 * unit),
             (Vector2){cosf(a) * speed, sinf(a) * speed}, curve);
  }
  shake = fmaxf(shake, 3 * unit);
  petalWaves--;
  waveTimer = 0.55f;
}

/* Floraison : cercles de pourriture écarlate. */
static void AtkBloom(void) {
  int n = 5 + (Phase() >= 2);
  for (int i = 0; i < n; i++) {
    Vector2 p;
    if (i == 0) {
      p = (Vector2){player.x + RandRange(-70, 70) * unit,
                    player.y + RandRange(-70, 70) * unit};
    } else {
      p = (Vector2){arena.x + RandRange(0.08f, 0.92f) * arena.width,
                    arena.y + RandRange(0.3f, 0.95f) * arena.height};
    }
    AddHazard(H_BLAST, p, 0, RandRange(42, 70) * unit, 0, Warn(1.35f));
  }
}

/* Onde écarlate : anneau avec une brèche (ou esquive). */
static void AtkRing(void) {
  const FunctionalSettings *items = FunctionalGet();
  int n = Phase() >= 2 ? 2 : 1;
  for (int i = 0; i < n; i++) {
    Hazard *h = AddHazard(H_RING, bossPos, 0, 0, 16 * unit, Warn(0.9f));
    if (!h)
      continue;
    h->speed = 210 * unit * Intensity() * items->bulletSpeed;
    h->gap = RandRange(0, 6.28f);
    h->angle = 0.5f;               /* demi-largeur de la brèche (rad) */
    h->maxSize = MaxReach();
    h->t = -i * 1.1f;
  }
}

/* Pluie de lumière de Miquella : colonnes dorées en cascade. */
static void AtkColumns(void) {
  int n = 6;
  bool reverse = GetRandomValue(0, 1) == 0;
  for (int i = 0; i < n; i++) {
    float x = arena.x + arena.width * (i + 0.5f) / n;
    Hazard *h = AddHazard(H_COLUMN, (Vector2){x, arena.y}, 0, 0,
                          arena.width * 0.085f, Warn(1.0f));
    if (h)
      h->t = -(reverse ? n - 1 - i : i) * 0.2f;
  }
}

static void LaunchAttack(void) {
  const FunctionalSettings *items = FunctionalGet();
  static const int list0[] = {A_SLASH, A_BEAMS, A_SLASH, A_BEAMS};
  static const int list1[] = {A_SLASH, A_BEAMS, A_DANCE, A_PETALS, A_DANCE};
  static const int list2[] = {A_BEAMS, A_DANCE, A_PETALS, A_BLOOM,
                              A_RING,  A_COLUMNS, A_DANCE};
  int p = Phase();
  const int *list = p == 0 ? list0 : (p == 1 ? list1 : list2);
  int n = p == 0 ? 4 : (p == 1 ? 5 : 7);

  int attack = list[GetRandomValue(0, n - 1)];
  for (int tries = 0; tries < 3 && attack == lastAttack; tries++)
    attack = list[GetRandomValue(0, n - 1)];
  lastAttack = attack;

  float extra = 0;
  switch (attack) {
  case A_SLASH:   AtkSlash(); break;
  case A_BEAMS:   AtkBeams(); break;
  case A_DANCE:   AtkDance(); extra = (Phase() >= 2 ? 6 : 4) * 0.6f; break;
  case A_PETALS:  AtkPetals(); extra = (Phase() >= 2 ? 3 : 2) * 0.55f; break;
  case A_BLOOM:   AtkBloom(); extra = 0.6f; break;
  case A_RING:    AtkRing(); extra = Phase() >= 2 ? 1.6f : 0.6f; break;
  case A_COLUMNS: AtkColumns(); extra = 1.2f; break;
  }
  /* le sceau est parfois pris pour cible quand on le charge */
  if (charge > 0 && GetRandomValue(0, 2) == 0)
    AddHazard(H_BLAST, rune, 0, 70 * unit, 0, Warn(1.0f));

  attackTimer = ((2.5f - 0.35f * p) / Intensity() + extra) * items->sealSpawn;
}

/* ============================== démarrage ============================== */

void FinalBossStart(int floorNumberArg, int d) {
  if (heartTex.id == 0)
    heartTex = LoadTexture("assets/heart.png");
  if (sealTex.id == 0)
    sealTex = LoadTexture("assets/seal.png");
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
  difficulty = Limit((float)d, 1, 10);
  GameZonePickModifiers(difficulty, MINIGAME_MODIFIER_OPTIONS, playerMaluses,
                        playerMalusNames);

  const FunctionalSettings *items = FunctionalGet();
  lives = maxLives = 3 + items->extraLives -
                      (playerMaluses[MALUS_LIFE] ? 1 : 0);
  if (lives < 1)
    lives = maxLives = 1;
  damageCount = 0;
  sealed = 0;
  sealWards = 0;
  state = FINALBOSS_PLAYING;
  timeLeft = DURATION + items->duration;
  attackTimer = 0.5f;
  invincible = charge = step = dashTime = dashCooldown = 0;
  introTime = transition = dying = shake = flash = bossFlash = 0;
  danceLeft = petalWaves = 0;
  danceTimer = waveTimer = 0;
  burstT = -1;
  burstBig = false;
  shownHp = 1;
  lastAttack = -1;
  moving = false;
  dir = (Vector2){0, 1};
  facing = FRONT;
  ready = false;
  memset(hazards, 0, sizeof(hazards));
  memset(petals, 0, sizeof(petals));
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
    dashCooldown = DASH_COOLDOWN * items->sealDashCooldown *
                   (playerMaluses[MALUS_DASH]
                        ? PLAYER_DASH_COOLDOWN_MALUS
                        : 1.0f);
  }

  float speed = 200 * unit * items->playerSpeed *
                (playerMaluses[MALUS_SLOW] ? PLAYER_SPEED_MALUS : 1.0f);
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
  float r = 10 * unit * FunctionalGet()->hitbox * PlayerHitboxScale();
  float dx = player.x - h->pos.x, dy = player.y - h->pos.y;
  float d = hypotf(dx, dy);

  switch (h->kind) {
  case H_SLASH: {
    if (d > h->size + r)
      return false;
    float diff = remainderf(atan2f(dy, dx) - h->angle, 2 * PI);
    return fabsf(diff) < h->width + r / fmaxf(d, r);
  }
  case H_BEAM: {
    Vector2 end = Polar(h->pos, h->angle, h->size);
    return NearSegment(player, h->pos, end, h->width / 2 + r);
  }
  case H_COLUMN:
    return fabsf(player.x - h->pos.x) < h->width / 2 + r;
  case H_BLAST:
    return d < h->size + r;
  case H_RING: {
    bool onBand = fabsf(d - h->size) < h->width / 2 + r;
    bool outsideGap = fabsf(remainderf(atan2f(dy, dx) - h->gap, 2 * PI)) > h->angle;
    return onBand && outsideGap;
  }
  }
  return false;
}

static void UpdateHazard(Hazard *h, float dt) {
  h->t += dt;
  if (h->t < 0)
    return;

  if (h->kind == H_RING) {
    if (h->t >= h->warn) {
      if (!h->fired) {
        h->fired = true;
        shake = fmaxf(shake, 6 * unit);
      }
      h->size += h->speed * dt;
      if (h->size > h->maxSize) {
        h->on = false;
        return;
      }
    }
  } else if (h->t > h->warn + ACTIVE) {
    h->on = false;
    return;
  } else if (h->t >= h->warn && !h->fired) {
    h->fired = true;
    shake = fmaxf(shake, (h->kind == H_SLASH ? 7 : 4) * unit);
  }

  if (h->t < h->warn || h->hit || invincible > 0 || dashTime > 0 ||
      transition > 0 || dying > 0 || !Touches(h))
    return;
  h->hit = true;
  Hurt();
}

static void UpdatePetals(float dt) {
  const FunctionalSettings *items = FunctionalGet();
  float radius = 7 * unit * items->bulletSize +
                 9 * unit * items->hitbox * PlayerHitboxScale();
  for (int i = 0; i < MAX_PETALS; i++) {
    Petal *p = &petals[i];
    if (!p->on)
      continue;
    float c = cosf(p->curve * dt), s = sinf(p->curve * dt);
    p->vel = (Vector2){p->vel.x * c - p->vel.y * s, p->vel.x * s + p->vel.y * c};
    p->pos.x += p->vel.x * dt;
    p->pos.y += p->vel.y * dt;
    float out = 40 * unit;
    if (p->pos.x < arena.x - out || p->pos.x > arena.x + arena.width + out ||
        p->pos.y < arena.y - out || p->pos.y > arena.y + arena.height + out) {
      p->on = false;
      continue;
    }
    if (invincible > 0 || dashTime > 0 || transition > 0 || dying > 0)
      continue;
    if (hypotf(player.x - p->pos.x, player.y - p->pos.y) < radius) {
      p->on = false;
      Hurt();
    }
  }
}

static void UpdateRune(float dt) {
  const FunctionalSettings *items = FunctionalGet();
  if (hypotf(player.x - rune.x, player.y - rune.y) < 45 * unit)
    charge += dt * items->sealCharge;
  else
    charge -= dt * 0.35f * items->sealChargeDecay;
  charge = Limit(charge, 0, SEAL_TIME);
  if (charge < SEAL_TIME)
    return;

  /* sceau accompli : le boss est touché */
  burstPos = rune;
  burstT = 0;
  burstBig = false;
  sealed++;
  if (items->sealWard)
    sealWards++;
  charge = 0;
  bossFlash = 1;
  flash = 0.35f;
  shake = fmaxf(shake, 12 * unit);
  danceLeft = petalWaves = 0;
  memset(hazards, 0, sizeof(hazards));
  memset(petals, 0, sizeof(petals));

  if (sealed >= TOTAL_SEALS) {          /* mort du boss */
    dying = DYING_TIME;
    burstBig = true;
    flash = 0.9f;
  } else if (sealed % PHASE_SEALS == 0) { /* changement de phase */
    transition = TRANSITION_TIME;
    burstBig = true;
    flash = 0.8f;
    attackTimer = TRANSITION_TIME + 1.0f;
    PlaceRune();
  } else {
    attackTimer = 1.3f * items->sealSpawn;
    PlaceRune();
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
  shownHp += ((1.0f - (float)sealed / TOTAL_SEALS) - shownHp) * fminf(1, dt * 4);

  /* le boss glisse vers sa position cible (plus vite pendant la danse) */
  bossPos.y = arena.y + arena.height * 0.28f;
  bossPos.x += (bossTargetX - bossPos.x) * fminf(1, dt * (danceLeft > 0 ? 14 : 3));

  MovePlayer(dt);

  if (dying > 0) {                       /* mort du boss : on laisse jouer l'outro */
    dying -= dt;
    shake = fmaxf(shake, 3 * unit);
    if (dying <= 0)
      state = FINALBOSS_WON;
    return;
  }

  if (introTime < INTRO_TIME) {
    introTime += dt;
    return;
  }

  timeLeft -= dt;
  UpdateRune(dt);

  if (dying <= 0 && transition > 0) {
    transition -= dt;
  } else if (dying <= 0) {
    if (danceLeft > 0) {
      danceTimer -= dt;
      if (danceTimer <= 0)
        DanceStep();
    }
    if (petalWaves > 0) {
      waveTimer -= dt;
      if (waveTimer <= 0)
        SpawnPetalWave();
    }
    attackTimer -= dt;
    if (attackTimer <= 0 && danceLeft <= 0 && petalWaves <= 0)
      LaunchAttack();
  }

  for (int i = 0; i < MAX_HAZARDS; i++)
    if (hazards[i].on)
      UpdateHazard(&hazards[i], dt);
  UpdatePetals(dt);

  if (timeLeft <= 0 && state == FINALBOSS_PLAYING)
    state = FINALBOSS_LOST;
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

  float introP = Clamp01(introTime / INTRO_TIME);
  float ease = 1 - powf(1 - introP, 3);
  float alpha = Clamp01(introTime / 1.8f);
  if (dying > 0 && dying < DYING_TIME - 1.2f)
    alpha = Clamp01(dying / (DYING_TIME - 1.2f));
  float rise = (1 - ease) * arena.height * 0.12f;
  float tremble = dying > 0 ? sinf(clock * 80) * 4 * unit : 0;
  tremble += bossFlash * sinf(clock * 90) * 3 * unit;

  Vector2 c = {bossPos.x + tremble,
               bossPos.y + sinf(clock * 1.6f) * 5 * unit - rise};

  BeginBlendMode(BLEND_ADDITIVE);   /* aura : rouge, puis dorée en phase 3 */
  for (int l = 0; l < 6; l++)
    DrawCircleV((Vector2){c.x, c.y + h * 0.05f}, h * (0.22f + 0.1f * l),
                Fade((Color){200, 30, 40, 255},
                     (0.03f + 0.02f * phase) * alpha));
  if (phase >= 2 || dying > 0)
    for (int i = 0; i < 24; i++) {      /* poussière d'or qui monte */
      float ph = fmodf(clock * 0.3f + Rand01(i * 3), 1.0f);
      DrawCircleV((Vector2){c.x + (Rand01(i * 3 + 1) - 0.5f) * w,
                            c.y + h * 0.4f - ph * h * 0.9f},
                  (1 + 2 * Rand01(i * 3 + 2)) * unit,
                  Fade((Color){255, 215, 120, 255}, (1 - ph) * 0.8f * alpha));
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

  if (bossFlash > 0) {              /* éclat blanc quand il est touché */
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(bossTex, src, dst, (Vector2){0, 0}, 0,
                   Fade(WHITE, bossFlash * 0.5f * alpha));
    EndBlendMode();
  }
}

static void DrawRune(void) {
  Color gold = GOLD;
  float p = charge / SEAL_TIME;
  if (sealTex.id != 0) {
    Rectangle src = {0, 0, (float)sealTex.width, (float)sealTex.height};
    Rectangle dst = {rune.x - 50 * unit, rune.y - 50 * unit, 100 * unit, 100 * unit};
    DrawTexturePro(sealTex, src, dst, (Vector2){0, 0}, 0, WHITE);
  }
  BeginBlendMode(BLEND_ADDITIVE);
  DrawCircleV(rune, 48 * unit * (0.4f + 0.6f * p), Fade(gold, 0.10f + 0.25f * p));
  for (int i = 0; i < 8; i++) {      /* rayons de lumière */
    float a = clock * 0.6f + i * PI / 4;
    DrawLineEx(Polar(rune, a, 40 * unit), Polar(rune, a, (60 + 40 * p) * unit),
               2 * unit, Fade(gold, 0.25f + 0.4f * p));
  }
  if (p > 0)
    for (int i = 0; i < 14; i++) {   /* étincelles qui montent */
      float ph = fmodf(clock * 1.1f + Rand01(i), 1.0f);
      DrawCircleV((Vector2){rune.x + (Rand01(i + 50) - 0.5f) * 70 * unit,
                            rune.y + 20 * unit - ph * 90 * unit},
                  2.2f * unit, Fade(RAYWHITE, (1 - ph) * p));
    }
  EndBlendMode();
  DrawRing(rune, 52 * unit, 58 * unit, 0, 360 * p, 0, gold);
}

static void DrawHazard(Hazard *h) {
  if (h->t < 0)
    return;
  Color scarlet = {200, 25, 40, 255};
  Color gold = {255, 215, 120, 255};
  bool firing = h->t >= h->warn;
  float p = Clamp01(h->t / h->warn);
  float u = Clamp01((h->t - h->warn) / ACTIVE), al = 1 - u;

  switch (h->kind) {
  case H_SLASH: {
    float deg = h->angle * RAD2DEG, half = h->width * RAD2DEG;
    if (!firing) {
      DrawCircleSector(h->pos, h->size * p, deg - half, deg + half, 32,
                       Fade(scarlet, 0.18f + 0.25f * p));
      DrawCircleSectorLines(h->pos, h->size, deg - half, deg + half, 32,
                            Fade(RED, 0.7f));
    } else {
      DrawCircleSector(h->pos, h->size, deg - half, deg + half, 32, Fade(WHITE, al));
      BeginBlendMode(BLEND_ADDITIVE);
      DrawCircleSector(h->pos, h->size * 1.04f, deg - half - 4, deg + half + 4, 32,
                       Fade(scarlet, 0.6f * al));
      DrawRing(h->pos, h->size * 0.9f, h->size * 1.02f, deg - half, deg + half, 32,
               Fade(gold, al));
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
      DrawLineEx(h->pos, end, h->width * 2.2f, Fade(scarlet, 0.45f * al));
      EndBlendMode();
      DrawLineEx(h->pos, end, h->width, Fade(WHITE, al));
    }
    break;
  }
  case H_COLUMN: {
    Rectangle col = {h->pos.x - h->width / 2, arena.y, h->width, arena.height};
    if (!firing) {
      DrawRectangleRec(col, Fade(gold, 0.08f + 0.22f * p));
      DrawRectangleLinesEx(col, 2 * unit, Fade(gold, 0.6f));
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
  case H_BLAST: {
    if (!firing) {
      DrawCircleV(h->pos, h->size * p, Fade(scarlet, 0.45f));
      DrawCircleLinesV(h->pos, h->size, RED);
    } else {
      float r = h->size * (0.85f + 0.25f * u);
      BeginBlendMode(BLEND_ADDITIVE);
      DrawCircleV(h->pos, r, Fade(scarlet, 0.7f * al));
      for (int i = 0; i < 10; i++) {   /* pétales de pourriture */
        float a = clock * 1.5f + i * 2 * PI / 10;
        DrawLineEx(Polar(h->pos, a, r * 0.2f), Polar(h->pos, a, r * 1.2f),
                   4 * unit, Fade((Color){255, 90, 70, 255}, al));
      }
      DrawCircleV(h->pos, r * 0.45f, Fade(WHITE, 0.8f * al));
      EndBlendMode();
      DrawRing(h->pos, r * 0.95f, r * 1.05f, 0, 360, 48, Fade(RED, al));
    }
    break;
  }
  case H_RING: {
    float gapDeg = h->gap * RAD2DEG, halfDeg = h->angle * RAD2DEG;
    if (!firing) {                      /* on montre la brèche à l'avance */
      Vector2 a1 = Polar(h->pos, h->gap - h->angle, 170 * unit);
      Vector2 a2 = Polar(h->pos, h->gap + h->angle, 170 * unit);
      DrawCircleLinesV(h->pos, 45 * unit * (0.4f + 0.6f * p), Fade(RED, 0.8f));
      DrawLineEx(h->pos, a1, 2 * unit, Fade(gold, 0.6f * p));
      DrawLineEx(h->pos, a2, 2 * unit, Fade(gold, 0.6f * p));
    } else {
      float start = gapDeg + halfDeg;
      float end = start + 360 - 2 * halfDeg;
      BeginBlendMode(BLEND_ADDITIVE);
      DrawRing(h->pos, h->size - h->width, h->size + h->width, start, end, 96,
               Fade(scarlet, 0.5f));
      EndBlendMode();
      DrawRing(h->pos, h->size - h->width / 2, h->size + h->width / 2, start, end,
               96, WHITE);
    }
    break;
  }
  }
}

static void DrawPetals(void) {
  const FunctionalSettings *items = FunctionalGet();
  float size = 7 * unit * items->bulletSize;
  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 0; i < MAX_PETALS; i++) {
    Petal *p = &petals[i];
    if (!p->on)
      continue;
    float sp = hypotf(p->vel.x, p->vel.y);
    Vector2 tail = {p->pos.x - p->vel.x / fmaxf(sp, 1) * size * 3.5f,
                    p->pos.y - p->vel.y / fmaxf(sp, 1) * size * 3.5f};
    DrawLineEx(tail, p->pos, size * 0.9f, Fade((Color){220, 40, 60, 255}, 0.5f));
    DrawCircleV(p->pos, size * 1.6f, Fade((Color){255, 60, 80, 255}, 0.35f));
  }
  EndBlendMode();
  for (int i = 0; i < MAX_PETALS; i++)
    if (petals[i].on)
      DrawCircleV(petals[i].pos, size, (Color){235, 50, 70, 255});
}

static void DrawBurst(void) {
  if (burstT < 0)
    return;
  float u = burstT / BURST_TIME, fade = 1 - u;
  BeginBlendMode(BLEND_ADDITIVE);
  float rr = unit * (20 + u * 260);
  DrawRing(burstPos, rr - 5 * unit, rr + 5 * unit, 0, 360, 64, Fade(GOLD, fade));
  for (int i = 0; i < 60; i++) {
    float a = Rand01(i * 2) * 2 * PI;
    float sp = (120 + 380 * Rand01(i * 2 + 1)) * unit;
    float d = sp * burstT * (1.1f - 0.4f * burstT);
    Vector2 p = Polar(burstPos, a, d);
    p.y += 200 * unit * burstT * burstT;
    DrawCircleV(p, (1.5f + 2.5f * Rand01(i)) * unit * fade,
                Fade(i % 3 == 0 ? WHITE : GOLD, fade));
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
                  Fade(i % 2 ? (Color){255, 80, 70, 255} : GOLD, fade));
    }
  }
  EndBlendMode();
}

static void DrawArena(void) {
  int phase = Phase();

  /* teinte rouge de plus en plus lourde */
  DrawRectangleGradientV((int)arena.x, (int)arena.y, (int)arena.width,
                         (int)arena.height, Fade(BLACK, 0),
                         Fade((Color){90, 0, 10, 255}, 0.18f + 0.1f * phase));

  /* pétales ambiants (rouges, parfois dorés) */
  for (int i = 0; i < 36; i++) {
    float ph = fmodf(clock * (0.06f + 0.08f * Rand01(i * 4)) + Rand01(i * 4 + 1), 1);
    float x = arena.x + arena.width * Rand01(i * 4 + 2) + sinf(clock * 1.3f + i) * 20 * unit;
    float y = arena.y + arena.height * ph;
    float size = (2 + 3 * Rand01(i * 4 + 3)) * unit;
    Color col = i % 4 == 0 ? (Color){240, 200, 110, 255} : (Color){190, 30, 45, 255};
    DrawEllipse((int)x, (int)y, size * 1.6f, size, Fade(col, 0.45f));
  }

  DrawBoss();
  DrawRune();

  for (int i = 0; i < MAX_HAZARDS; i++)
    if (hazards[i].on)
      DrawHazard(&hazards[i]);
  DrawPetals();

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
  DrawCentered(a, "MALENIA, LAME CONSORT", 16, a.y + GameUiPixels(6),
               (Color){241, 213, 147, 255});
  Rectangle bar = {a.x + a.width * 0.25f, a.y + GameUiPixels(28),
                   a.width * 0.5f, (float)GameUiPixels(10)};
  DrawRectangleRec(bar, (Color){30, 8, 12, 255});
  DrawRectangleRec((Rectangle){bar.x, bar.y, bar.width * Clamp01(shownHp), bar.height},
                   (Color){190, 25, 40, 255});
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

  /* titre d'intro */
  if (introTime < INTRO_TIME) {
    float p = introTime / INTRO_TIME;
    float al = Clamp01(fminf(p / 0.25f, (1 - p) / 0.25f));
    DrawCentered(a, "MALENIA", 56, a.y + a.height * 0.5f - GameUiPixels(44),
                 Fade((Color){241, 213, 147, 255}, al));
    DrawCentered(a, "LAME CONSORT", 24, a.y + a.height * 0.5f + GameUiPixels(18),
                 Fade((Color){200, 60, 70, 255}, al));
  } else if (sealed == 0 && introTime < INTRO_TIME + 6) {
    DrawCentered(a, "RESTEZ DANS LE SCEAU POUR LA BLESSER", 16,
                 a.y + a.height - GameUiPixels(50), Fade(GOLD, 0.9f));
  }

  /* titre de phase */
  if (transition > 0 && dying <= 0) {
    float p = 1 - transition / TRANSITION_TIME;
    float al = Clamp01(fminf(p / 0.2f, (1 - p) / 0.25f));
    int ph = Phase();
    DrawCentered(a, phaseTitles[ph], 40, a.y + a.height * 0.5f - GameUiPixels(40),
                 Fade((Color){241, 213, 147, 255}, al));
    DrawCentered(a, phaseNames[ph], 22, a.y + a.height * 0.5f + GameUiPixels(12),
                 Fade((Color){220, 70, 80, 255}, al));
  }

  if (state != FINALBOSS_PLAYING) {
    const char *title = state == FINALBOSS_WON ? "VICTOIRE" : "GAME OVER";
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

void FinalBossUpdate(Rectangle a) {
  if (state == FINALBOSS_OFF)
    return;

  float smallSide = a.width < a.height ? a.width : a.height;
  arena = a;
  unit = smallSide / 400;
  clock += GetFrameTime();

  if (!ready) {
    player = (Vector2){a.x + a.width / 2, a.y + a.height * 0.8f};
    bossPos = (Vector2){a.x + a.width / 2, a.y + a.height * 0.28f};
    bossTargetX = bossPos.x;
    PlaceRune();
    ready = true;
  }

  if (state == FINALBOSS_PLAYING)
    Update(GetFrameTime());
  if (state != FINALBOSS_PLAYING && state != FINALBOSS_OFF && IsKeyPressed(KEY_ENTER))
    state = FINALBOSS_OFF;

  Draw(a);
}

FinalBossStatus FinalBossState(void) {
  return state;
}

int FinalBossDamageCount(void) {
  return damageCount;
}

void FinalBossUnload(void) {
  UnloadTexture(heartTex);
  heartTex = (Texture2D){0};
  UnloadTexture(sealTex);
  sealTex = (Texture2D){0};
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
  state = FINALBOSS_OFF;
}
