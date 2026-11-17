#include "background.h"
#include <raylib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *videoFile;
static Texture2D tex;
static long *frameOffsets;
static uint32_t *frameSizes;
static uint32_t frameCount, currentFrame, videoFps;
static float timer;
static bool playbackFailed;

static bool ReadUint32(uint32_t *value) {
  unsigned char bytes[4];
  if (fread(bytes, 1, sizeof(bytes), videoFile) != sizeof(bytes))
    return false;
  *value = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
  return true;
}

static bool LoadVideoFrame(uint32_t frameIndex, bool createTexture) {
  if (frameIndex >= frameCount ||
      fseek(videoFile, frameOffsets[frameIndex], SEEK_SET) != 0)
    return false;

  uint32_t size = frameSizes[frameIndex];
  unsigned char *data = malloc(size);
  if (!data) {
    TraceLog(LOG_ERROR, "Memoire insuffisante pour lire une image du fond");
    return false;
  }
  bool readSucceeded = fread(data, 1, size, videoFile) == size;
  Image image = {0};
  if (readSucceeded) {
    SetTraceLogLevel(LOG_WARNING);
    image = LoadImageFromMemory(".png", data, (int)size);
    SetTraceLogLevel(LOG_INFO);
  }
  free(data);

  if (!readSucceeded || !image.data) {
    TraceLog(LOG_ERROR, "Impossible de decoder la frame %u du fond", frameIndex);
    return false;
  }
  if (createTexture) {
    tex = LoadTextureFromImage(image);
  } else if (tex.id != 0) {
    UpdateTexture(tex, image.data);
  }
  UnloadImage(image);
  return tex.id != 0;
}

void InitMenuBackground(void) {
  char magic[4];
  uint32_t width, height;
  videoFile = fopen("assets/menu_background.htv", "rb");
  if (videoFile &&
      fread(magic, 1, sizeof(magic), videoFile) == sizeof(magic) &&
      memcmp(magic, "HTV1", sizeof(magic)) == 0 &&
      ReadUint32(&width) && ReadUint32(&height) && ReadUint32(&videoFps) &&
      ReadUint32(&frameCount) && width > 0 && width <= 8192 &&
      height > 0 && height <= 8192 && videoFps > 0 && videoFps <= 60 &&
      frameCount > 0 && frameCount <= 3600) {
    frameOffsets = malloc((size_t)frameCount * sizeof(*frameOffsets));
    frameSizes = malloc((size_t)frameCount * sizeof(*frameSizes));
    if (frameOffsets && frameSizes) {
      bool valid = true;
      if (fseek(videoFile, 0, SEEK_END) != 0)
        valid = false;
      long fileSize = valid ? ftell(videoFile) : -1;
      if (fileSize < 0 || fseek(videoFile, 20, SEEK_SET) != 0)
        valid = false;
      for (uint32_t i = 0; valid && i < frameCount; i++) {
        if (!ReadUint32(&frameSizes[i]) || frameSizes[i] == 0 ||
            frameSizes[i] > 16 * 1024 * 1024) {
          TraceLog(LOG_ERROR, "Taille invalide pour la frame %u du fond", i);
          valid = false;
        }
      }
      long frameOffset = valid ? ftell(videoFile) : -1;
      for (uint32_t i = 0; valid && i < frameCount; i++) {
        frameOffsets[i] = frameOffset;
        if (frameOffset < 0 ||
            frameOffset > fileSize - (long)frameSizes[i]) {
          valid = false;
          break;
        }
        frameOffset += (long)frameSizes[i];
      }
      if (valid && LoadVideoFrame(0, true))
        return;
      if (!valid)
        TraceLog(LOG_ERROR, "Index de frames invalide dans le fond video");
    }
  }

  TraceLog(LOG_ERROR, "Impossible de charger assets/menu_background.htv");
  if (videoFile) {
    fclose(videoFile);
    videoFile = NULL;
  }
  free(frameOffsets);
  free(frameSizes);
  frameOffsets = NULL;
  frameSizes = NULL;
  frameCount = 0;
  tex = LoadTexture("assets/loading_screen.png");
}

void UpdateAndDrawMenuBackground(void) {
  if (tex.id == 0)
    return;

  if (videoFile && !playbackFailed && frameCount > 1) {
    timer += GetFrameTime();
    float frameDuration = 1.0f / videoFps;
    if (timer >= frameDuration) {
      uint32_t framesToAdvance = (uint32_t)(timer / frameDuration);
      timer = fmodf(timer, frameDuration);
      currentFrame = (currentFrame + framesToAdvance) % frameCount;
      if (!LoadVideoFrame(currentFrame, false)) {
        playbackFailed = true;
      }
    }
  }

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float screenAspect = (float)sw / sh;
  float textureAspect = (float)tex.width / tex.height;
  Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
  if (textureAspect > screenAspect) {
    source.width = tex.height * screenAspect;
    source.x = (tex.width - source.width) / 2;
  } else {
    source.height = tex.width / screenAspect;
    source.y = (tex.height - source.height) / 2;
  }
  Rectangle dest = {0, 0, (float)sw, (float)sh};
  DrawTexturePro(tex, source, dest, (Vector2){0, 0}, 0, WHITE);
  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 90});
}

void UnloadMenuBackground(void) {
  if (videoFile)
    fclose(videoFile);
  videoFile = NULL;
  free(frameOffsets);
  free(frameSizes);
  frameOffsets = NULL;
  frameSizes = NULL;
  UnloadTexture(tex);
  tex = (Texture2D){0};
  frameCount = currentFrame = videoFps = 0;
  timer = 0;
  playbackFailed = false;
}
