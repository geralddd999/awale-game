# Awalé protocol

Text protocol over TCP (default port 1977).

## Framing
- One message = one line terminated by `\n` (a `\r` before it is ignored).
- Lines are at most 1023 bytes. A client that sends a longer line receives `ERR LINE_TOO_LONG` and is disconnected.
- A bio has up to 10 lines of up to 80 printable ASCII characters. On the wire it is a single line: its lines are separated by the two characters `\n` (backslash, then `n`). The client's `/bio set` editor builds that line for the user.
- Usernames: `[A-Za-z0-9_]{1,16}`, except `draw`, which is reserved: it stands for a drawn game in the winner field of `EVT END` and `GAME`. A username can be connected only once at a time.

Each request gets exactly one `OK` or `ERR` reply. `OK` replies to list commands are followed by one line per item, and the count in the `OK` line says how many. `EVT` and `BOARD` lines can arrive at any time.

## Client states
```
           /login               /create or /join
 UNAUTH ──────────▶ IDLE (hall) ─────────────────▶ IN_LOBBY
                         ▲                            │
                         └─── /leave, /kick, ─────────┘
                              lobby closed
```

## Lobbies
- A lobby is named after its **leader**, the user who created it. A client is in at most one lobby.
- Seat 0 is the leader (`PLAYER_0`). Seat 1 is taken by the first user who joins. Everyone else is a spectator.
- `/start` (leader only, both seats filled) starts a game. **The server draws at random who begins each game** and announces it in `EVT START`. When the game ends, the lobby goes back to waiting, and `/start` again gives a rematch.
- When a player leaves or disconnects during a game, they lose it (`EVT END … left|disconnect`).
- If seat 1 frees up between games, the oldest spectator takes it (`EVT SEAT`).
- When the leader leaves or disconnects, the lobby closes (`EVT LOBBY_CLOSED`) and every member goes back to the hall.
- A private lobby (`/create private`) can be entered by `/join` only by users the leader added as friends, and by anyone through an accepted invitation.

## Invitations
- Only the leader invites, and only users who are logged in and **not in a lobby** (`ERR BUSY` otherwise).
- A user has **at most one pending invitation**. While it is pending, further invitations to them are refused with `ERR BUSY`.
- The invited user answers with `/accept` or `/deny`. The invitation **expires after 30 seconds** without an answer (`EVT INVITE_EXPIRED`), and then the user can be invited again.
- An invitation is also cancelled (`EVT INVITE_CANCELLED`) when the inviting lobby closes, when the invited user enters another lobby, or when they disconnect.
- Accepting = joining the lobby, even if it is private. `/join <leader>` on the inviting lobby counts as accepting too.
  Friendship is one-way: `/friend bob` lets bob into *your* private lobbies, not you into his.
- `/say` talks to your current room: your lobby, or the hall if you are not in one.

## Client → server
| Command | Allowed in | Reply |
|---|---|---|
| `/login <name>` | UNAUTH | `OK login <name>` |
| `/list` | logged | `OK list <n>` + n × `USER <name> idle` or `USER <name> lobby <leader>` |
| `/lobbies` | logged | `OK lobbies <n>` + n × `LOBBY <leader> waiting\|playing public\|private <opponent\|-> <spectators>` |
| `/create [private]` | IDLE | `OK create <leader> public\|private` |
| `/join <leader>` | IDLE | `OK join <leader> player\|spectator` |
| `/leave` | IN_LOBBY | `OK leave` |
| `/invite <name>` | IN_LOBBY, leader | `OK invite <name>` |
| `/accept [leader]` | IDLE, invited | same as `/join`: `OK join <leader> player\|spectator` |
| `/deny [leader]` | IDLE, invited | `OK deny <leader>` |
| `/kick <name>` | IN_LOBBY, leader | `OK kick <name>` |
| `/start` | IN_LOBBY, leader | `OK start` |
| `/move <1-6>` | IN_LOBBY, seated | `OK move <house>` |
| `/forfeit` | IN_LOBBY, seated | `OK forfeit` |
| `/say <text>` | logged | `OK say` |
| `/msg <name> <text>` | logged | `OK msg <name>` |
| `/bio set <text>` | logged | `OK bio`, or `ERR BAD_ARGS` if the bio has more than 10 lines, a line longer than 80 characters, or a non-printable or non-ASCII character. An empty text clears the bio |
| `/bio [name]` | logged | `OK bio <name> <n>` + n × `BIO <line>` (`n` is 0 for an empty bio). Your own bio if no name is given |
| `/friend [name]` | logged | `OK friend <name>`, or with no name `OK friends <n>` + n × `FRIEND <name> online\|offline` |
| `/unfriend <name>` | logged | `OK unfriend <name>` |
| `/history [name]` | logged | `OK history <n>` + n × `GAME <id> <p0> <p1> <winner\|draw> <cap0> <cap1> <moves>` |
| `/replay <id> [step]` | logged | `OK replay <id> <step> <total> <p0> <p1>` + `BOARD #<id> …` |
| `/rank` | logged | `OK rank <n>` + n × `RANK <pos> <name> <wins> <losses> <draws>` |
| `/help` | any | `OK help <command list>` |
| `/quit` | any | `OK quit`, then the server closes the connection |

Houses are numbered 1 to 6 from the player's own left. Sowing goes left to right on your own row.

## Server → client events
```
EVT USER_JOIN <name>                       (hall)
EVT USER_LEFT <name>                       (hall)
EVT LOBBY_OPEN <leader> public|private     (hall)
EVT LOBBY_CLOSED <leader> leader_left|leader_disconnected
EVT JOINED <leader> <name> player|spectator
EVT LEFT <leader> <name> left|disconnect|kicked
EVT SEAT <leader> <name>                   spectator moved to the free seat
EVT KICKED <leader>
EVT INVITE <leader> <seconds>              answer with /accept or /deny before <seconds>
EVT INVITE_DENIED <leader> <target>        (to the leader)
EVT INVITE_EXPIRED <leader> <target>       (to both)
EVT INVITE_CANCELLED <leader> <target>
EVT START <leader> <p0> <p1> 0|1|spectator <first player>
EVT MOVE <leader> <player> <house>
EVT END <leader> <winner|draw> <cap0> <cap1> normal|forfeit|left|disconnect <game_id>
EVT CHAT <from> <text>
EVT PM <from> <text>
```

## Board
```
BOARD <leader|#id> <turn> <winner> <h0> … <h11> <cap0> <cap1>
```
- `turn`: 0 or 1.
- `winner`: 0 means the game is ongoing, 1 means p0 won, 2 means p1 won, 3 means a draw. These are the values of the `Winner` enum in `game.h`.
- `h0..h5` is p0's row and `h6..h11` is p1's row, the same as `Board.houses[]`.

The client rebuilds a `Board` from this line and renders it with `render_board()`.

## Errors
`ERR <CODE> <human readable text>`

| Code | Meaning |
|---|---|
| `NOT_LOGGED` | command sent before `/login` |
| `BAD_NAME`, `NAME_TAKEN` | `/login` refused (invalid or reserved name, or already connected) |
| `UNKNOWN_CMD`, `BAD_ARGS` | unknown command / wrong arguments |
| `BAD_STATE` | command not allowed now (e.g. kicking a player mid-game) |
| `NO_LOBBY` | lobby does not exist / you are not in one |
| `ALREADY_IN_LOBBY` | `/leave` first |
| `NOT_LEADER` | leader-only command |
| `NOT_PLAYER` | spectators cannot play |
| `SEATS_EMPTY` | `/start` without an opponent |
| `LOBBY_FULL`, `PRIVATE_LOBBY` | `/join` refused |
| `UNKNOWN_USER` | no such user, or not online |
| `NOT_YOUR_TURN`, `INVALID_HOUSE`, `EMPTY_HOUSE`, `MUST_FEED` | illegal move (from `PlayResult`) |
| `NO_GAME` | `/replay` of an unknown game |
| `BUSY` | `/invite` of a user already in a lobby or with a pending invitation |
| `NO_INVITE` | `/accept` or `/deny` without a (matching) pending invitation |
| `SERVER_FULL` | a limit was reached |
| `LINE_TOO_LONG` | line over 1023 bytes, connection closed |

## Example
```
A> /login alice              A< OK login alice
B> /login bob                B< OK login bob
                             A< EVT USER_JOIN bob
A> /create                   A< OK create alice public
                             B< EVT LOBBY_OPEN alice public
B> /join alice               B< OK join alice player
                             A< EVT JOINED alice bob player
A> /start                    A< OK start
                             A< EVT START alice alice bob 0 bob
                             B< EVT START alice alice bob 1 bob
                             A,B< BOARD alice 1 0 4 4 4 4 4 4 4 4 4 4 4 4 0 0
B> /move 3                   B< OK move 3
                             A,B< EVT MOVE alice bob 3
                             A,B< BOARD alice 0 0 5 4 4 4 4 4 4 4 0 5 5 5 0 0
B> /say gl                   A< EVT CHAT bob gl
A> /forfeit                  A< OK forfeit
                             A,B< EVT END alice bob 0 0 forfeit 1
```

## Persistence (server side, `data/`)
- `users.txt`: one user per line: `name wins losses draws bio…`
- `friends.txt`: the friend graph, one directed edge per line: `from to`
- `games/<id>.txt`: first player and the list of moves (`save_game` format)
- `games/index.txt`: one game per line: `id p0 p1 winner cap0 cap1 moves`
