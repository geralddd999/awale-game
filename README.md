# awale-game
The goal of this project is to implement an Awalé game server. The aim is to have a client/server application that allows clients to play matches, verify that the rules are correctly applied, communicate, and keep traces (scores, games played, etc.).

## Build and run
```sh
make                                # build/server and build/client
make test                           # rules + line framing unit tests
make e2e                            # end-to-end protocol test against a real server

./build/server [port]               # default port 1977, data stored in ./data
./build/client <host> <name> [port]
```
In the client, type commands such as `/create`, `/join alice`, `/start`, `/help`.
During a game, a bare `1`-`6` plays that house, and any text that does not start with `/` is sent as chat.

The wire protocol is described in [PROTOCOL.md](PROTOCOL.md).
