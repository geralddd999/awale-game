#!/usr/bin/env bash
# End-to-end test: starts a server in a temporary directory and talks the raw protocol to it
# through bash /dev/tcp sockets, one file descriptor per simulated user.
# WRAP runs the server under a tool, e.g. WRAP="valgrind -q --error-exitcode=9" make e2e
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PORT=${PORT:-$((20000 + RANDOM % 20000))}
TMP=$(mktemp -d)

# invitations expire after 2 seconds instead of 30 to keep the test short
(cd "$TMP" && exec ${WRAP:-} "$ROOT/build/server" "$PORT" 2) >"$TMP/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; rm -rf "$TMP"' EXIT
# wait until the server accepts connections
for _ in $(seq 50); do (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break; sleep 0.1; done

passed=0
failed=0
LAST=""

send(){ printf '%s\n' "$2" >&"$1"; }

# reads lines from fd $1 until one matches the glob $2, skipping the others
# (gives up after WAIT seconds without a line, 2 by default)
expect(){
	local fd=$1 pattern=$2 line
	while IFS= read -r -t "${WAIT:-2}" -u "$fd" line; do
		if [[ $line == $pattern ]]; then
			LAST=$line
			passed=$((passed+1))
			return 0
		fi
	done
	echo "FAILED: expected '$pattern' on fd $fd"
	failed=$((failed+1))
	return 1
}

exec {A}<>/dev/tcp/127.0.0.1/$PORT
exec {B}<>/dev/tcp/127.0.0.1/$PORT
exec {C}<>/dev/tcp/127.0.0.1/$PORT
exec {D}<>/dev/tcp/127.0.0.1/$PORT

# --- login
send $A "/list";             expect $A "ERR NOT_LOGGED *"
send $A "/login alice";      expect $A "OK login alice"
send $B "/login bob";        expect $B "OK login bob"
expect $A "EVT USER_JOIN bob"
send $C "/login carol";      expect $C "OK login carol"
send $D "/login alice";      expect $D "ERR NAME_TAKEN *"
send $D "/login bad name!";  expect $D "ERR BAD_NAME *"
send $D "/login draw";       expect $D "ERR BAD_NAME *"

# --- two commands in a single packet are both handled
printf '/list\n/lobbies\n' >&"$A"
expect $A "OK list 3"
expect $A "OK lobbies 0"

# --- lobby
send $A "/move 1";           expect $A "ERR NO_LOBBY *"
send $A "/create";           expect $A "OK create alice public"
expect $B "EVT LOBBY_OPEN alice public"
send $A "/create";           expect $A "ERR ALREADY_IN_LOBBY *"
send $B "/join nobody";      expect $B "ERR NO_LOBBY *"
send $A "/start";            expect $A "ERR SEATS_EMPTY *"
send $B "/join alice";       expect $B "OK join alice player"
expect $A "EVT JOINED alice bob player"
send $C "/join alice";       expect $C "OK join alice spectator"
send $B "/start";            expect $B "ERR NOT_LEADER *"

# --- game
send $A "/start";            expect $A "EVT START alice alice bob 0 *"
first=$(awk '{print $7}' <<<"$LAST")
expect $B "EVT START alice alice bob 1 $first"
expect $C "EVT START alice alice bob spectator $first"
expect $A "BOARD alice * 0 4 4 4 4 4 4 4 4 4 4 4 4 0 0"
turn=$(awk '{print $3}' <<<"$LAST")
# the player drawn by the server is the one whose turn it is
[[ ($turn == 0 && $first == alice) || ($turn == 1 && $first == bob) ]] && passed=$((passed+1)) || { echo "FAILED: first player $first but turn $turn"; failed=$((failed+1)); }
if [[ $turn == 0 ]]; then P=$A; O=$B; PNAME=alice; else P=$B; O=$A; PNAME=bob; fi

send $O "/move 1";           expect $O "ERR NOT_YOUR_TURN *"
send $C "/move 1";           expect $C "ERR NOT_PLAYER *"
send $P "/move 7";           expect $P "ERR INVALID_HOUSE *"
send $P "/move x";           expect $P "ERR BAD_ARGS *"
send $P "/move 4294967297";  expect $P "ERR BAD_ARGS *"   
send $P "/move -2147483648"; expect $P "ERR INVALID_HOUSE *"
send $P "/move 1";           expect $P "OK move 1"
expect $C "EVT MOVE alice $PNAME 1"
expect $C "BOARD alice *"

# --- lobby chat stays in the lobby, hall chat stays in the hall
send $D "/login dave";       expect $D "OK login dave"
send $B "/say good luck";    expect $A "EVT CHAT bob good luck"
expect $C "EVT CHAT bob good luck"
send $D "/say hello hall";   expect $D "OK say"
send $D "/msg carol psst";   expect $C "EVT PM dave psst"

# --- spectator joining mid-game gets the position
send $D "/join alice";       expect $D "OK join alice spectator"
expect $D "EVT START alice alice bob spectator *"
expect $D "BOARD alice *"
send $D "/leave";            expect $D "OK leave"
expect $A "EVT LEFT alice dave left"

# --- forfeit, stats, history, replay
send $B "/forfeit";          expect $B "OK forfeit"
expect $A "EVT END alice alice * * forfeit 1"
expect $C "EVT END alice alice * * forfeit 1"
send $A "/history";          expect $A "OK history 1"
expect $A "GAME 1 alice bob alice *"
send $A "/replay 1 0";       expect $A "OK replay 1 0 1 alice bob"
expect $A "BOARD #1 * 0 4 4 4 4 4 4 4 4 4 4 4 4 0 0"
send $A "/rank";             expect $A "OK rank *"
expect $A "RANK 1 alice 1 0 0"

# --- rematch, then the opponent disconnects: they lose and the spectator takes the seat
send $A "/start";            expect $A "EVT START alice alice bob 0 *"
exec {B}>&-
expect $A "EVT END alice alice * * disconnect 2"
expect $A "EVT LEFT alice bob disconnect"
expect $A "EVT SEAT alice carol"

# --- the leader leaves: the lobby closes
send $A "/leave";            expect $A "OK leave"
expect $C "EVT LOBBY_CLOSED alice leader_left"
send $C "/join alice";       expect $C "ERR NO_LOBBY *"

# --- private lobby: only invited users (or friends of the leader) can get in
send $C "/create private";   expect $C "OK create carol private"
send $A "/join carol";       expect $A "ERR PRIVATE_LOBBY *"
# edges are one-way: alice -> carol does not let alice into carol's lobby
send $A "/friend carol";     expect $A "OK friend carol"
send $A "/join carol";       expect $A "ERR PRIVATE_LOBBY *"

# --- invitations: one pending at a time, answered with /accept or /deny, or expiring
send $C "/invite alice";     expect $C "OK invite alice"
expect $A "EVT INVITE carol 2"
send $C "/invite alice";     expect $C "ERR BUSY *"
send $D "/create";           expect $D "OK create dave public"
send $D "/invite alice";     expect $D "ERR BUSY *"
send $D "/invite carol";     expect $D "ERR BUSY *"
send $D "/leave";            expect $D "OK leave"
send $A "/accept dave";      expect $A "ERR NO_INVITE *"
send $A "/deny";             expect $A "OK deny carol"
expect $C "EVT INVITE_DENIED carol alice"
send $A "/accept";           expect $A "ERR NO_INVITE *"
send $C "/invite alice";     expect $C "OK invite alice"
expect $A "EVT INVITE carol 2"
WAIT=5 expect $A "EVT INVITE_EXPIRED carol alice"
expect $C "EVT INVITE_EXPIRED carol alice"
send $A "/accept";           expect $A "ERR NO_INVITE *"
send $C "/invite alice";     expect $C "OK invite alice"
expect $A "EVT INVITE carol 2"
send $A "/accept carol";     expect $A "OK join carol player"
send $C "/invite alice";     expect $C "ERR BUSY *"
send $C "/friend dave";      expect $C "OK friend dave"
expect $D "EVT FRIEND_REQ carol"
send $D "/friend carol";     expect $D "OK friend carol"
expect $C "EVT FRIEND_ACCEPT dave"
send $C "/friend";           expect $C "OK friends 2"
expect $C "FRIEND dave online"
expect $C "FRIEND alice incoming"
send $C "/friend dave";      expect $C "ERR BAD_ARGS *"
send $C "/friend alice";     expect $C "OK friend alice"
send $C "/unfriend alice";   expect $C "OK unfriend alice"
send $C "/unfriend alice";   expect $C "ERR UNKNOWN_USER *"
send $D "/join carol";       expect $D "OK join carol spectator"
send $C "/kick dave";        expect $C "OK kick dave"
expect $D "EVT KICKED carol"

# --- closing a lobby cancels the invitations it sent
send $C "/invite dave";      expect $C "OK invite dave"
expect $D "EVT INVITE carol 2"
send $C "/leave";            expect $C "OK leave"
expect $D "EVT INVITE_CANCELLED carol dave"


# --- profiles
send $A "/bio set I like seeds"; expect $A "OK bio"
send $C "/bio alice";        expect $C "OK bio alice 1"; expect $C "BIO I like seeds"
send $A '/bio set Hi\n\nI am alice\nfrom Lyon'; expect $A "OK bio"
send $C "/bio alice";        expect $C "OK bio alice 4"; expect $C "BIO Hi"; expect $C "BIO "; expect $C "BIO I am alice"; expect $C "BIO from Lyon"
send $A '/bio set l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10'; expect $A "OK bio"
send $A '/bio set l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\nl11'; expect $A "ERR BAD_ARGS *too many lines*"
send $A "/bio set $(printf 'x%.0s' $(seq 81))"; expect $A "ERR BAD_ARGS *too long*"
send $A "/bio set $(printf 'x%.0s' $(seq 80))"; expect $A "OK bio"
send $A "/bio set caf$(printf '\303\251')"; expect $A "ERR BAD_ARGS *ASCII*"
send $A "/bio set $(printf 'a\033[31mb')"; expect $A "ERR BAD_ARGS *ASCII*"
send $A "/bio set";          expect $A "OK bio"
send $C "/bio alice";        expect $C "OK bio alice 0"
send $A "/bio set I like seeds"; expect $A "OK bio"


# --- a line without newline that fills the buffer gets the client dropped
head -c 2000 /dev/zero | tr '\0' 'x' >&"$D"
expect $D "ERR LINE_TOO_LONG *"

# --- data survives a restart
send $A "/quit";             expect $A "OK quit"
grep -q "^alice 2 0 0 I like seeds$" "$TMP/data/users.txt" && passed=$((passed+1)) || { echo "FAILED: users.txt content"; cat "$TMP/data/users.txt"; failed=$((failed+1)); }
[[ $(cat "$TMP/data/friends.txt") == $'carol dave\ndave carol' ]] && passed=$((passed+1)) || { echo "FAILED: friends.txt content"; cat "$TMP/data/friends.txt"; failed=$((failed+1)); }
[[ -f $TMP/data/games/1.txt && -f $TMP/data/games/2.txt ]] && passed=$((passed+1)) || { echo "FAILED: saved games"; failed=$((failed+1)); }

if [[ -n ${WRAP:-} ]]; then
	kill $SERVER; wait $SERVER 2>/dev/null
	if grep -q "==[0-9]*==" "$TMP/server.log"; then echo "FAILED: $WRAP reported errors"; cat "$TMP/server.log"; failed=$((failed+1)); else passed=$((passed+1)); fi
fi

echo "e2e: $passed passed, $failed failed"
[[ $failed == 0 ]]
