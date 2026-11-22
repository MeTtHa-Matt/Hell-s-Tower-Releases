#include "modifiers.h"
#include "balance.h"
#include "../ui_scale.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

static char activeModifierText[192];

int GameZonePickModifiers(int difficulty, int optionCount, bool *selected,
                          const char *const *names) {
  activeModifierText[0] = '\0';
  if (!selected || !names || optionCount < 1 ||
      optionCount > MINIGAME_MODIFIER_OPTIONS) {
    TraceLog(LOG_ERROR, "Liste de modificateurs invalide");
    return 0;
  }

  int order[MINIGAME_MODIFIER_OPTIONS];
  for (int i = 0; i < optionCount; i++) {
    selected[i] = false;
    order[i] = i;
  }

  int count = BalanceMiniGameModifierCount(
      difficulty, GetRandomValue(0, 99), GetRandomValue(0, 99));
  if (count > optionCount)
    count = optionCount;

  for (int i = 0; i < count; i++) {
    int choice = GetRandomValue(i, optionCount - 1);
    int swap = order[i];
    order[i] = order[choice];
    order[choice] = swap;
    selected[order[i]] = true;
  }

  if (count > 0) {
    int prefixLength = snprintf(activeModifierText, sizeof(activeModifierText),
                                "MALUS : ");
    if (prefixLength < 0 || (size_t)prefixLength >= sizeof(activeModifierText))
      return count;
    size_t used = (size_t)prefixLength;
    int displayed = 0;
    for (int i = 0; i < optionCount; i++) {
      if (!selected[i] || used >= sizeof(activeModifierText))
        continue;
      int written = snprintf(activeModifierText + used,
                             sizeof(activeModifierText) - used, "%s%s",
                             displayed == 0 ? "" : " / ", names[i]);
      if (written < 0 || (size_t)written >= sizeof(activeModifierText) - used)
        break;
      used += (size_t)written;
      displayed++;
    }
  }
  return count;
}

void GameZoneDrawActiveModifiers(Rectangle area, int yOffset) {
  if (!activeModifierText[0])
    return;

  int size = GameUiPixels(13);
  int minSize = GameUiPixels(9);
  int padding = GameUiPixels(8);
  int maxWidth = (int)area.width - padding * 2;
  while (size > minSize && MeasureText(activeModifierText, size) > maxWidth)
    size--;

  BeginScissorMode((int)area.x, (int)area.y, (int)area.width,
                   (int)area.height);
  DrawText(activeModifierText, (int)area.x + padding,
           (int)area.y + yOffset, size,
           (Color){255, 170, 125, 255});
  EndScissorMode();
}
