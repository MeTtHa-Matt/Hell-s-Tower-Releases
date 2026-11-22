#ifndef SPIKES_H
#define SPIKES_H

#include "raylib.h"

typedef enum {
  SPIKES_OFF,
  SPIKES_PLAYING,
  SPIKES_WON,
  SPIKES_LOST
} SpikesStatus;

void SpikesStart(int floorNumber, int difficulty);
void SpikesUpdate(Rectangle area);
SpikesStatus SpikesState(void);
int SpikesDamageCount(void);
void SpikesUnload(void);

#endif
