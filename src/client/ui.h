#ifndef UI_H
#define UI_H

#include <ncurses.h>

enum {
  CP_DEFAULT = 1,
  CP_TITLE,
  CP_SELECTED,
  CP_BORDER,
  CP_BORDER_ACTIVE,
  CP_CHAT_NAME,
  CP_CHAT_PM,
  CP_CHAT_SYS,
  CP_ERROR,
  CP_SUCCESS,
  CP_PIT,
  CP_PIT_ACTIVE,
  CP_STORE
};

void ui_init(void);
void ui_shutdown(void);
void ui_tick(void);
void ui_render(void);
void ui_handle_key(int ch);

#endif
