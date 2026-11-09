#ifndef NET_CLIENT_H
#define NET_CLIENT_H

void net_client_handle_line(const char *line);
void net_client_toggle_lobby_privacy(void);
void net_client_notify_chat_cmd(const char *cmd);

#endif
