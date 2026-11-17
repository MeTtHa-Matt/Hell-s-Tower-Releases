#include "button_sounds.h"
#include "raylib.h"

#define SOUND_VOICES 8

static Sound moveSounds[SOUND_VOICES], clickSounds[SOUND_VOICES];
static int moveSoundCount, clickSoundCount;
static int nextMoveSound, nextClickSound;
static bool buttonHovered[BUTTON_SOUND_COUNT];
static bool buttonSeenThisFrame[BUTTON_SOUND_COUNT];
static bool buttonHoveredThisFrame[BUTTON_SOUND_COUNT];

static int LoadSoundVoices(const char *path, Sound *sounds) {
  int count = 0;
  for (; count < SOUND_VOICES; count++) {
    sounds[count] = LoadSound(path);
    if (sounds[count].frameCount == 0)
      break;
    SetSoundVolume(sounds[count], 0.6f);
  }
  if (count == 0)
    TraceLog(LOG_WARNING, "Impossible de charger %s", path);
  return count;
}

static void PlaySoundVoice(Sound *sounds, int count, int *nextSound) {
  if (count == 0)
    return;

  for (int i = 0; i < count; i++) {
    int index = (*nextSound + i) % count;
    if (!IsSoundPlaying(sounds[index])) {
      PlaySound(sounds[index]);
      *nextSound = (index + 1) % count;
      return;
    }
  }

  StopSound(sounds[*nextSound]);
  PlaySound(sounds[*nextSound]);
  *nextSound = (*nextSound + 1) % count;
}

void InitButtonSounds(void) {
  if (!IsAudioDeviceReady())
    return;

  moveSoundCount = LoadSoundVoices("assets/button_move.mp3", moveSounds);
  clickSoundCount = LoadSoundVoices("assets/button_click.mp3", clickSounds);
}

void BeginButtonSoundFrame(void) {
  for (int i = 0; i < BUTTON_SOUND_COUNT; i++) {
    buttonSeenThisFrame[i] = false;
    buttonHoveredThisFrame[i] = false;
  }
}

void UpdateButtonHoverSound(ButtonSoundId id, bool hovered) {
  int index = (int)id;
  if (index < 0 || index >= BUTTON_SOUND_COUNT)
    return;

  buttonSeenThisFrame[index] = true;
  buttonHoveredThisFrame[index] = hovered;
  if (hovered && !buttonHovered[index])
    PlaySoundVoice(moveSounds, moveSoundCount, &nextMoveSound);
}

void EndButtonSoundFrame(void) {
  for (int i = 0; i < BUTTON_SOUND_COUNT; i++)
    buttonHovered[i] = buttonSeenThisFrame[i] && buttonHoveredThisFrame[i];
}

void PlayButtonClickSound(void) {
  PlaySoundVoice(clickSounds, clickSoundCount, &nextClickSound);
}

void UnloadButtonSounds(void) {
  for (int i = 0; i < moveSoundCount; i++)
    UnloadSound(moveSounds[i]);
  for (int i = 0; i < clickSoundCount; i++)
    UnloadSound(clickSounds[i]);
  moveSoundCount = clickSoundCount = 0;
  nextMoveSound = nextClickSound = 0;
  for (int i = 0; i < BUTTON_SOUND_COUNT; i++)
    buttonHovered[i] = false;
}
