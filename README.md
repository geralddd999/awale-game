# Awalé game server

A client/server Awalé (Oware) game written in C. Players connect to a server, challenge each other, play
matches whose moves are checked by the server, chat, spectate, and review finished games.

Authors: BUSETTI Marco, CHAMBI Gerald - 4IF, INSA Lyon

## Build and run

Requirements: `gcc`, `make`, a POSIX system (Linux or macOS). Windows is not supported.

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
| `make play` | local two-player game in one terminal, no network |
| `make clean` | removes `build/` |

## Features

- Full Awalé rules, checked by the server (see [Rules](#rules)).
- Several games at the same time, each in its own **lobby**.
- Online users list, lobby list, public and private lobbies.
- Challenges: invite a user, who accepts or refuses (or lets the invitation expire).
- Spectators: anyone can watch a game and receives every move and board.
- Chat: in the lobby, in the hall, and private messages.
- Profiles: a bio of up to 10 lines.
- Friends list, used to restrict private lobbies.
- Every finished game is saved; list your games and replay them move by move.
- Ranking and win/loss/draw statistics, kept across restarts.

## Client commands

Anything typed in the client is sent to the server. A line starting with `/` is a command. During a game, a bare
`1`–`6` plays that house. Any other text is sent as chat to your current room.

### Getting started

| Command | Description |
|---|---|
| `/list` | online users, and the lobby each one is in |
| `/lobbies` | open lobbies: `<leader> waiting\|playing public\|private <opponent> <spectators>` |
| `/create [private]` | create a lobby and become its leader |
| `/join <leader>` | join a lobby: as player if seat 1 is free and no game is running, otherwise as spectator |
| `/leave` | leave your lobby. A player leaving during a game **forfeits** it. If the leader leaves, the lobby closes |
| `/help` | list the commands |
| `/quit` | disconnect |

### Playing

| Command | Description |
|---|---|
| `/start` | leader only: start a game (needs an opponent in seat 1) |
| `/move <1-6>` or just `1`–`6` | play a house. Houses are numbered 1–6 from your left |
| `/forfeit` | give up the current game |
| `/kick <user>` | leader only: remove someone from the lobby (not a player during a game) |

### Challenges

| Command | Description |
|---|---|
| `/invite <user>` | leader only: invite a user who is online and not in a lobby |
| `/accept [leader]` | accept your pending invitation (also works for a private lobby) |
| `/deny [leader]` | refuse it |

A user has at most one pending invitation. It expires after 30 s (`invite_sec` on the server).

### Chat and profiles

| Command | Description |
|---|---|
| `/say <text>` | talk to your lobby, or to the hall if you are in none (same as typing text without `/`) |
| `/msg <user> <text>` | private message to an online user |
| `/bio [user]` | show your bio, or another user's |
| `/bio set` | write your bio: type up to 10 lines (80 ASCII characters each), end with a line containing only `.` |
| `/bio set <text>` | set a one-line bio. Send `/bio set` then `.` to clear it |

### Friends and private lobbies

| Command | Description |
|---|---|
| `/friend <user>` | send a friend request (or accept a pending request from `<user>`) |
| `/friend` | list your mutual friends (`online`/`offline`) and pending requests (`incoming`/`outgoing`) |
| `/unfriend <user>` | remove a friend or cancel/decline a pending friend request |

A lobby created with `/create private` can only be joined by the leader's mutual friends, or by users the leader invited.
Friendship is mutual: `/friend bob` sends a friend request to Bob, and once Bob runs `/friend alice`, both are friends and can join each other's private lobbies.

### History and ranking

| Command | Description |
|---|---|
| `/history [user]` | last 20 games of a user (yourself by default): id, players, winner, score, number of moves |
| `/replay <id> [step]` | show a saved game after `step` moves (default: the final position). Try `/replay 3 0`, `/replay 3 1`, … |
| `/rank` | top 10 players: 3 points per win, 1 per draw |

### Example

```
alice> /create                 bob> /join alice
alice> /start
== game starts in alice's lobby: alice vs bob
== the server drew bob to begin
bob> 3                         (bob plays house 3)
alice> good luck!              (chat, no slash)
carol> /join alice             (carol spectates)
alice> /forfeit
== bob wins 0-0 (forfeit)
alice> /history
```

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
