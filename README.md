# Awalé game server

A client/server Awalé (Oware) game written in C. Players connect to a server, challenge each other, play
matches whose moves are checked by the server, chat, spectate, and review finished games.

Authors: BUSETTI Marco, CHAMBI Gerald - 4IF, INSA Lyon

## Build and run

Requirements: `gcc`, `make`, `ncurses` (make sure to have the `libncurses-dev` package installed on Debian/Ubuntu), a POSIX system (Linux or macOS). Windows is not supported.

```sh
make                                  # builds build/server and build/client
./build/server [port] [invite_sec]    # default port 1977, invitations expire after 30 s
./build/client <host> <username> [port]
```

Example, everything on one machine (one terminal each):

```sh
./build/server
./build/client localhost alice
./build/client localhost bob
```

The server keeps its data in `./data` (created on first run, relative to the directory the server is started in).

| Target | What it does |
|---|---|
| `make test` | unit tests: game rules and line framing |
| `make e2e` | end-to-end test: starts a real server and talks the protocol to it |
| `make play` | local two-player CLI game in one terminal, no network |
| `make clean` | removes `build/` |

## Features

- Full Awalé rules, checked by the server (see [Rules](#rules)).
- Interactive `ncurses` TUI client with dedicated screens for menus, lobbies, games, profiles, friends, rankings, and replays.
- Several games at the same time, each in its own **lobby** (public or private), plus local 2-player play directly in the TUI.
- Challenges: invite an online player to your lobby, who can accept or refuse from the Invites panel.
- Spectators: anyone can watch a game live.
- Wrapped chat panel in every screen (public chat, lobby chat, private messages, and `/` commands).
- Profiles with an editable bio of up to 10 lines.
- Mutual friends list with pending friend requests (send, accept, decline, remove), used to restrict private lobbies.
- Saved games and auto-playing or step-by-step replay viewer.
- Ranking and win/loss/draw statistics, kept across restarts.

## TUI controls

The client runs a full-screen `ncurses` interface (recommended terminal size: at least `90x28`).

| Key | Action |
|---|---|
| `↑` `↓` `←` `→` | Navigate menus, lists, and side panels |
| `Enter` | Select highlighted button or list action |
| `Tab` | Switch focus to/from the Chat input box |
| `Esc` / `b` | Go back / close modal / leave Chat input |
| `i` | Focus the Invites panel (`y` accept, `n` decline) |
| `1`–`6` | Play house `1`–`6` during a local or online match |
| `Space` / `←` `→` | Play/pause or step through moves in the Replay screen |

### TUI screens

- **Main Menu**: `Play`, `Ranking`, `Replays`, `Quit`, and side panel (`Profile`, `Friends`, `Online players`).
- **Play**: `Play Local` (2 players on the same keyboard), `Create Lobby` (toggle `[x] Private?`, invite players, start game), or `Join Lobby` (join open lobbies as player or spectator).
- **Profile**: View your bio and edit it (`Tab` or `F2` to save, `Esc` to cancel).
- **Friends**: View friends and online status, remove a friend, send a friend request (`Add friend?`), or open `Pending invites` to accept (`y` / `Enter`) or reject (`n` / `x` / `d`) incoming requests.
- **Online players & Ranking**: Browse connected players and top rankings; press `Enter` on a player to send or accept a friend request.
- **Replays**: Select a finished game to watch it auto-play or step through it move by move.

## Chat & slash commands

Press `Tab` to focus the Chat box. Plain text is sent as chat (`/say`). You can also run any protocol command directly from the Chat box:

| Command | Description |
|---|---|
| `/msg <user> <text>` | private message to an online user |
| `/bio [user]` | display your bio or another user's bio in chat |
| `/friend <user>` | send a friend request (or accept a pending request from `<user>`) |
| `/unfriend <user>` | remove a friend or cancel/decline a pending request |
| `/invite <user>` | invite an online user to your lobby |
| `/accept [leader]` / `/deny [leader]` | accept or refuse a lobby invitation |
| `/kick <user>` | leader only: remove someone from the lobby |
| `/forfeit` | give up the current game |
| `/help` | list available commands in chat |
| `/quit` | disconnect |

## Rules

- 2 rows of 6 houses, 4 seeds each. A move picks one of your non-empty houses and sows its seeds one by one into
  the following houses: left to right along your own row, then on along the opponent's row. On a lap the origin
  house is skipped.
- If the last seed lands in an **opponent** house that now holds 2 or 3 seeds, you capture it, and keep capturing
  backwards on the opponent's row while houses hold 2 or 3.
- A move that would capture **all** the opponent's seeds is played, but captures nothing.
- If the opponent has no seeds, you must play a move that gives them some.
- The first player to capture **25 seeds** wins.
- If a player cannot move (or cannot feed an empty opponent), the game ends and each player keeps the seeds on
  their own side.
- After 100 consecutive moves without a capture, the game ends the same way.
- Leaving or disconnecting during a game counts as a loss.

## Data on the server

All in `data/`, plain text:

| File | Content |
|---|---|
| `users.txt` | one user per line: name, wins, losses, draws, bio |
| `friends.txt` | friendships (`from to`) and pending friend requests (`REQ from to`) |
| `games/<id>.txt` | the moves of a finished game |
| `games/index.txt` | one line per finished game: id, players, winner, score, number of moves |

Delete `data/` to reset everything.
