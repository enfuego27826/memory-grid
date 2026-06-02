# Decisions (where the spec/brief was silent or self-contradictory)

Per build brief §9, choices made for genuinely unspecified points. Each also
appears as a `// DECISION:` comment at its site in the code.

## Core architecture / seam
- **Monotonic `atMs` threaded through `apply`/`onTimer`.** The core computes
  speed/volunteer bonuses but stays time-agnostic and deterministic; the
  transport supplies a monotonic millisecond clock. (`protocol.hpp`)
- **Membership & host controls are Commands**, not side-channel methods, keeping
  the seam strictly "commands in": `JoinRoom`, `PlayerLeft`, `ConfigureRoom`,
  `TabSwitched`, `SkipRound`, `EndGameEarly`, `PauseToggle`. Reconnection stays
  transport-only; the core never sees a socket swap. (`protocol.hpp`)

## Path secrecy (brief's headline property)
- **`SafePath` is move-only with no full-sequence accessor except
  `revealForMemorise()`**, and there is deliberately **no `to_json(SafePath)`** —
  so serializing the path is a compile error. The path reaches the wire only via
  the `PatternReveal` event, constructed only in the Pattern phase (asserted).
  (`core/safepath.hpp`, `core/game_room.cpp`)

## Gameplay rules the spec left open
- **Speed-bonus denominator.** The spec's formula divides by an undefined walk
  "time_limit". We derive a *par time* = pathLength × 2000 ms (a tunable in
  `config.hpp`); full bonus at/under par, linear decay to zero at 2× par.
- **Chess coords convention.** `A3` = column A (leftmost), row 3 (1-based). The
  explicit `row N col M` form stays row-first. (`core/hint_parser.cpp`)
- **Adjacent-but-wrong tile = a wrong step**; only *non-adjacent* clicks are
  silently rejected (spec §16 only defines the latter). Clicking an
  already-walked tile is therefore a wrong step (the path is one-way).
- **Volunteer bonus on partial (lives-mode) failure** is still paid (it rewards
  the brave press); it is suppressed only by an elimination-mode failure's zero
  multiplier. A round total is clamped to ≥ 0. (`core/scoring.cpp`)
- **Ignored correct hints** earn the helper half the zone points of the
  earliest on-path tile they named, on failure only (spec §8/§9).
- **Random walker eligibility**: any non-eliminated, non-spectator player,
  including one whose `canVolunteer` was cleared (that flag only blocks the
  *press*). (`core/game_room.cpp`)
- **Lives refill**: when `lifeRefillPerRound` is on, all players' lives reset at
  each round start; the first round always initialises lives.
- **Walker selection = "Host assigns"** is not yet wired to a host-assign
  command; presses are ignored and the random fallback covers it. Full host
  assignment is a later addition. (`core/game_room.cpp`)
- **Pause / skip-round** are implemented minimally: pause holds the between-rounds
  transition; skip discards the current round without scoring. "Last round only"
  sudden-death elimination currently ends like a normal final round.

## Transport + protocol fidelity pass (milestones 2–6)

- **uWebSockets behind `ITransport`** (`transport/itransport.hpp`). uWS appears only
  in `uws_transport.cpp`; `wire`/`rooms`/`dispatch`/`sessions`/`ratelimit` are uWS-free and
  unit-tested against `FakeRoom` + `FakeTransport`. Swapping WS libs is one file.
- **`ws://` only** (`LIBUS_NO_SSL`); crypto/*.c excluded from the uSockets build. TLS
  terminates at a reverse proxy (also the Dockerfile's assumption).
- **uSockets compiled as a self-authored CMake target** (glob its `*.c`), not its Makefile.
  uWebSockets fetched with `GIT_SUBMODULES_RECURSE` (FetchContent doesn't recurse by default).
  Project enables `C` as well as `CXX` so the uSockets `.c` files link.
- **Disconnect = grace window, not immediate `PlayerLeft`.** Non-walker grace 15 s; walker
  grace 1.5 s (a hung walk should free fast so the core's same-path replay fires); last
  player out → 60 s teardown timer instead. Reconnect within grace rebinds the socket to the
  existing `PlayerId` and replays the cached last `RoomUpdate` — the core never sees the swap.
- **`PlayerLeft` is synthesized by the transport**, never parsed from the wire.
- **Transport-only wire additions:** a `joined {playerId, token}` ack on bind (so the client
  learns its id + reconnect token), and `name` fields on `walker_assigned`/`chat_message`/
  `player_eliminated`/`tab_switch_notice` resolved from the transport's cached roster.
- **Core fidelity additions:** `KickPlayer`/`TransferHost` commands (host-only lobby
  controls); `ScoreLine` widened with the Screen-5 breakdown (walker base/speed/volunteer/
  deductions, helper hintsFollowed/hintsIgnored); `WalkerAssigned` carries the start+finish
  endpoints (endpoints only — not the path).
- **HTTP handshake:** `POST /create` → `{code}`; `POST /join` validates code/password; the
  authoritative bind happens over the WS `join_room` message (carrying code + optional
  password/token). A `GET /` smoke page is served for manual browser testing.
- **Per-room serial dispatch** is guaranteed by the single uWS loop thread (timers fire on
  it too); the future one-room-per-worker move localizes to the dispatch entrypoints.

## Build / environment
- **uWebSockets is plan A** (transport milestone, not built this session). It
  will sit behind a thin `ITransport` so the swap is one file.
- **cmake**: pin `3.31.x` when bootstrapping via pip; cmake 4.x rejects the
  pre-3.5 minimum in doctest v2.4.11. As a fallback the top-level CMakeLists sets
  `CMAKE_POLICY_VERSION_MINIMUM=3.5` so cmake 4.x also configures.
