#ifndef CHATBOX_H
#define CHATBOX_H

#include <stdbool.h>

void chatbox(const char *text, const char *position);
bool IsChatboxFinished(const char *text);
bool IsChatboxInputConsumed(void);
void ResetChatbox(void);
void UnloadChatbox(void);

#endif
