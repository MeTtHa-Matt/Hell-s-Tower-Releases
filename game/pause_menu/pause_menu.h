#ifndef HOME_PAUSE_MENU_H
#define HOME_PAUSE_MENU_H

typedef enum HomePauseResult {
  HOME_PAUSE_NONE,
  HOME_PAUSE_RESUME,
  HOME_PAUSE_RETURN_TO_MENU,
  HOME_PAUSE_QUIT
} HomePauseResult;

typedef enum UnsavedDialogResult {
  UNSAVED_DIALOG_NONE,
  UNSAVED_DIALOG_CONTINUE,
  UNSAVED_DIALOG_CANCEL
} UnsavedDialogResult;

HomePauseResult DrawBlackScreenPauseMenu(float *shadeAlpha);
UnsavedDialogResult DrawUnsavedSessionDialog(void);

#endif
