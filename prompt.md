# Build Brief — Memory Grid (C++ backend + React frontend)

You are building **Memory Grid**, a real-time multiplayer web game. The full
gameplay design — rules, modes, numeric values, point system, UI screens,
edge cases, data models, socket events — is in the **attached specification
document**. Treat that document as the authoritative source for *what the game
does*. This brief governs *how it is built* and, where the two conflict, this
brief wins (it deliberately overrides the spec's tech-stack recommendations).

---

## 1. Source of truth

- The attached spec defines all gameplay: round flow, scoring formulas,
  difficulty presets, custom-mode settings, lives/elimination, helper hint
  scoring, lobby/host controls, edge-case rules, and the screen breakdown.
  Implement it faithfully.
- The spec repeatedly notes that **all numeric values are estimates subject to
  playtesting**. Therefore: put every tunable number (grid sizes, timers, point
  values, bonuses, rate limits, lives, deductions, zone splits) in a **single
  config module** as named constants / a settings struct. No magic numbers
  scattered through the logic. The difficulty presets are just named bundles of
  these constants.

## 2. Architecture decisions (these OVERRIDE the spec's stack section)

The spec recommends Node + Socket.io. Do **not** use that. Instead:

1. **Backend is C++ (C++20).** There is no good C++ Socket.io server, so:
2. **Use native WebSocket, not Socket.io.** The frontend uses the browser's
   built-in `WebSocket`. Messages are JSON objects with a `type` field; route
   on `type`. Implement client-side reconnect manually.
3. **Strict "functional core / imperative shell" split**, because the game core
   and the web server are owned by different people:
   - **Game core** = pure game logic (path generation, tile validation,
     scoring, hint parsing, round state machine). Knows nothing about sockets,
     JSON, threads, or networking.
   - **Transport shell** = web server (WebSocket + HTTP, rooms, broadcasting,
     serialization, rate limiting, reconnection, timers).
   - They meet **only** at the `protocol.hpp` seam in section 5 below. The
     transport never reads core state directly; it sends a `Command` and
     receives a `StepResult` (events to send + timers to schedule).
4. **Threading invariant (contract):** a given game-room instance is only ever
   accessed from a single thread; `apply` / `onTimer` are never called
   concurrently for the same instance. The core therefore needs **no internal
   locking**. The transport is responsible for guaranteeing this. Start
   **single-threaded** (one event loop); structure the room registry so a later
   move to one-room-per-worker is a localized change.
5. **WebSocket/HTTP library: uWebSockets (µWS).** This is the *one* easily
   swapped decision — if you prefer Drogon or Boost.Beast, keep all library
   usage behind a thin transport interface so it can be replaced. uWS is chosen
   for built-in HTTP+WS and pub/sub topics that map directly onto rooms.
6. **Path secrecy:** the safe path is sent to clients **only during the
   memorise phase** (clients must render it). It is **never** sent during the
   walk, and is **never** placed in a DOM attribute. During the walk the server
   sends only one-tile-at-a-time `tile_result {correct, lives}`. The server is
   authoritative for every click. This resolves the apparent tension between the
   spec's "path never leaves the server" line and its `pattern_reveal` event.

## 3. Tech stack

**Backend:** C++20 · uWebSockets (uSockets + zlib + OpenSSL for `wss`) ·
nlohmann/json for serialization · CMake build · vcpkg or Conan for dependencies
· in-memory state only (no database). Provide a Dockerfile (multi-stage:
toolchain build stage → slim runtime).

**Frontend:** React + Vite · Tailwind CSS · HTML Canvas for the grid (needed for
the anti-screenshot noise overlay in the spec) · Zustand for state · native
`WebSocket` (no Socket.io) · deployable as static hosting.

## 4. Repository structure

```
/server
  /core            # game logic — implements IGameRoom, ZERO transport deps
    game_room.{hpp,cpp}
    path_gen.{hpp,cpp}
    scoring.{hpp,cpp}
    hint_parser.{hpp,cpp}
    config.hpp     # all tunable constants + difficulty presets
  /transport       # web server — depends on core ONLY via protocol.hpp
    server.cpp     # uWS app: HTTP + WS routes, dispatch loop
    rooms.{hpp,cpp} # room registry + per-room transport metadata
    wire.{hpp,cpp}  # JSON <-> Command / EventBody serialization
    ratelimit.hpp
    sessions.hpp   # reconnect tokens
  protocol.hpp     # THE SEAM (section 5) — shared by core + transport
  /tests           # unit tests for core (no sockets) + FakeRoom transport tests
  CMakeLists.txt
/client            # React + Vite + Tailwind frontend
/docs
  spec.md          # the attached specification
```

## 5. The seam — implement `protocol.hpp` first

Both halves compile against this and nothing else of each other's. Events are
**semantic** (not wire-shaped): the core emits meaning, the transport owns the
JSON wire format end to end.

```cpp
using PlayerId = uint32_t;

// Commands: transport parses inbound JSON into these
struct JoinRoom   { std::string name; };
struct StartGame  {};
struct PressReady {};
struct ClickTile  { uint8_t row, col; };
struct SendChat   { std::string text; };
using Command = std::variant<JoinRoom, StartGame, PressReady, ClickTile, SendChat>;

// Events: core returns these; transport serializes to wire JSON
enum class Reach { Broadcast, One, AllExcept, Spectators };
struct PatternReveal  { std::vector<uint16_t> path; }; // memorise phase only
struct WalkerAssigned { PlayerId id; bool wasRandom; };
struct TileResultEv   { bool correct; uint8_t lives; };
struct WalkerPosition { uint8_t row, col; };
struct ChatBroadcast  { PlayerId from; std::string text; bool spectator; };
struct HintScored     { PlayerId helper; int points; };
struct ScoreUpdate    { /* per-player round + total */ };
struct PlayerEliminated { PlayerId id; };
struct GameEnded      { /* final leaderboard */ };
struct RoomUpdate     { /* players, state, settings */ };
using EventBody = std::variant<PatternReveal, WalkerAssigned, TileResultEv,
                               WalkerPosition, ChatBroadcast, HintScored,
                               ScoreUpdate, PlayerEliminated, GameEnded, RoomUpdate>;
struct Event { Reach reach; PlayerId who; EventBody body; };

// Timer directives: core asks the transport to wake it later.
// Durations are game policy (core); waiting is I/O (transport).
struct StartTimer  { uint32_t id; uint32_t delayMs; };
struct CancelTimer { uint32_t id; };
using TimerOp = std::variant<StartTimer, CancelTimer>;

struct StepResult {
    std::vector<Event>   out;
    std::vector<TimerOp> timers;
};

class IGameRoom {                 // core implements; transport consumes
public:
    virtual ~IGameRoom() = default;
    virtual StepResult apply(PlayerId from, const Command& cmd) = 0;
    virtual StepResult onTimer(uint32_t timerId) = 0;
    // CONTRACT: never called concurrently for the same instance.
};
```

Adjust the event set to cover every server→client event in the spec's socket
list, but keep the command-in / events-out shape exactly.

## 6. Hard constraints (do not violate)

- No game rules in the transport layer; no JSON/socket knowledge in the core.
- The path type has **no serializer that runs during the walk**; outside the
  memorise phase it must be impossible to put the path on the wire.
- Server is authoritative for every tile click, ready-press, and score.
- All tunables live in `config.hpp`; difficulty presets are bundles of those.
- Transport is the trust boundary: it rejects malformed JSON, unknown types,
  oversized payloads, and messages from sockets not bound to a room/player. The
  *core* rejects rule violations (non-adjacent tile, non-walker clicking) by
  returning no events — the transport does not encode adjacency rules.
- Chat rate limiting + the "5 dropped → 10s mute" rule live in the transport,
  enforced **before** a `SendChat` reaches the core.
- Reconnection identity (session token → rebind socket to existing player) is
  transport-only; the core must never observe a socket swap.

## 7. Build order (milestones — implement and verify in this order)

1. `protocol.hpp` + `config.hpp` (constants + the four difficulty presets).
2. uWS skeleton: HTTP create/join returning a 6-char room code; a WS route that
   echoes; native-WebSocket smoke test from the browser.
3. A `FakeRoom : IGameRoom` returning canned responses, plus the full dispatch
   loop and a `deliver()` that serializes `EventBody` and routes by `Reach`
   (publish to room topic / send to one / send to all-but-one). Get one scripted
   round flowing to the browser **without any real game logic**.
4. Room registry, pub/sub broadcasting, socket↔player map via per-connection
   user data, timers (transport schedules `StartTimer`, calls `onTimer`).
5. Rate limiting + mute, reconnection tokens, disconnect detection, the 60s
   empty-room teardown, host transfer.
6. Real game core behind `IGameRoom`: path generation (straight/zigzag/random,
   auto-length ≈60% of tiles), click validation + lives/reset + elimination,
   the full scoring formulas, three-zone helper hint scoring, hint parsing
   (`A3`, `row 2 col 3`, `go right`, `top left` — hand-rolled, not std::regex),
   round state machine. Swap `FakeRoom` for it; the transport should need
   essentially no changes.
7. Frontend: the seven screens from the spec (Home, Lobby, Pattern Reveal +
   Volunteer Race, The Walk, Score Reveal, Elimination, Final Leaderboard),
   Canvas grid with the imperceptible-noise overlay, reconnect logic.

## 8. Testing

- Core: unit tests driven purely through `apply`/`onTimer` with scripted command
  sequences — no sockets. Cover path gen validity, wrong-step lives/reset,
  elimination, every scoring branch (success/partial/fail multipliers, speed
  bonus clamp, volunteer bonus = 0 when random, deductions), and hint
  zone/parse cases.
- Transport: tests against `FakeRoom` covering join, dispatch, rate limit + mute,
  reconnect rebind, malformed input rejection, and timer firing.

## 9. Handling ambiguity

Where the spec is silent or self-contradictory, prefer the resolution this brief
gives (e.g. path secrecy). For anything else genuinely unspecified, make a
reasonable choice **and leave a short `// DECISION:` comment** explaining it
rather than silently guessing. Surface a brief list of such decisions at the end.

## 10. Out of scope

No database/persistence (state is per-session in memory). No accounts/auth
beyond room codes + optional lobby password. Client-side anti-cheat (canvas
noise, tab-switch / dev-tools detection) is best-effort per the spec — the
server simply receives the resulting events and applies penalties.
