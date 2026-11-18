#ifndef UPDATE_SCREEN_H
#define UPDATE_SCREEN_H

typedef enum {
  UPDATE_SCREEN_NONE,
  UPDATE_SCREEN_QUIT
} UpdateScreenAction;

UpdateScreenAction UpdateRequiredScreenDraw(const char *latestVersion);

#endif
