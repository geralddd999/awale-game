#ifndef DISPLAY_H
#define DISPLAY_H

void display_init(const char *me);
/* prints one line received from the server in a human friendly way */
void display_line(const char *line);
/* 1 when the user is seated in a running game, so a bare digit can be sent as
 * /move */
int display_in_game(void);

#endif
