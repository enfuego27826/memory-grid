# Memory Grid

### A real-time multiplayer pattern-memory game. One player walks a hidden path across a grid while others guide them via chat. Built for the web, no audio or video required.

> **Note:** All specific numeric values in this document (point amounts, time limits, grid sizes, etc.) are starting estimates and are subject to change based on playtesting and experimentation.
> 

---

## 1. Game Overview

**Memory Grid** is a multiplayer web game where:

- A secret safe path is shown across a grid for a limited time.
- All players must memorise it before it disappears.
- One volunteer (the **walker**) then navigates the grid tile by tile from start to finish.
- All other players (the **helpers**) watch and type hints freely in a live chat.
- If the walker reaches the end successfully, the walker and helpers all earn points.
- If the walker fails (steps on a wrong tile), they lose a life and reset to the start — same path, same round.
- Rounds continue until a winner is declared or all rounds are exhausted.

**Players:** 2–20
**Session length:** 10–30 minutes depending on rounds and difficulty
**Platform:** Web browser (desktop + mobile)
**Communication:** Text chat only — no audio, no video

---

## 2. How It Works — Quick Summary

Each round has four steps:

1. **Memorise.** A safe path lights up across the grid. Everyone studies it. A countdown runs (e.g. 20 seconds). The first player to press **"I'm Ready — Walk It!"** becomes the walker and the pattern disappears for everyone immediately. If nobody presses in time, a walker is randomly assigned.
2. **Walk.** The walker clicks tiles one at a time trying to follow the now-hidden path from start to finish. Everyone else watches and types hints in chat freely.
3. **Wrong step.** If the walker steps on a wrong tile, they lose a life and are sent back to the start. The same path is still active — helpers can use what they've learned to guide better on the next attempt. If the walker runs out of lives (or is in elimination mode), the walk fails.
4. **Score & repeat.** Points are awarded, the leaderboard updates, and the next round begins with a new path.

---

## 3. Points System — Quick Summary

Points reward both bravery (volunteering early) and skill (walking fast, giving good hints).

**Walker earns:**

- A base amount for successfully reaching the end.
- A speed bonus for finishing quickly.
- A volunteer bonus for pressing the button early — the earlier you pressed relative to the timer, the bigger the bonus. Randomly assigned walkers get no volunteer bonus.
- Deductions for each wrong step taken.

**Helpers earn:**

- Points for each hint the walker follows. Hints given earlier in the walk (when the path is less clear) are worth more than hints given near the finish.
- No penalty for wrong hints — helpers should feel free to shout out ideas.

**On failure:**

- Walker scores zero (elimination mode) or partial points (lives mode, if at least half the path was completed before running out of lives).
- Helpers who gave correct hints that the walker ignored still keep half their hint points.

> All specific point values are starting estimates and will be adjusted through playtesting.
> 

---

## 4. Difficulty Modes

Four preset modes are available in the lobby. Settings in presets can be previewed but not edited — use Custom Mode for that.

### Easy

| Setting | Value |
| --- | --- |
| Grid size | 4 × 4 |
| Memorise time | 30 seconds |
| Path length | 6–8 tiles |
| Lives | 3 |
| Elimination | Off |
| Chat rate limit | 1 message / 1s |
| Speed bonus | Yes |
| Volunteer bonus | Yes |

### Medium

| Setting | Value |
| --- | --- |
| Grid size | 6 × 6 |
| Memorise time | 20 seconds |
| Path length | 10–14 tiles |
| Lives | 2 |
| Elimination | Optional (host toggle) |
| Chat rate limit | 1 message / 1.5s |
| Speed bonus | Yes |
| Volunteer bonus | Yes |

### Hard

| Setting | Value |
| --- | --- |
| Grid size | 8 × 8 |
| Memorise time | 15 seconds |
| Path length | 16–22 tiles |
| Lives | 1 |
| Elimination | On (mandatory) |
| Chat rate limit | 1 message / 2s |
| Speed bonus | Yes |
| Volunteer bonus | Yes |

### Custom

See Section 5 for full details.

---

## 5. Custom Mode

Custom mode lets the host configure every parameter of the game. All settings below are exposed in the lobby settings panel.

### Grid Settings

| Setting | Type | Range / Options | Default |
| --- | --- | --- | --- |
| Grid size | Integer × Integer | 3×3 to 12×12 | 6×6 |
| Path length | Integer or Auto | 3 to (grid size² - 4), or Auto | Auto |
| Path style | Select | Straight, Zigzag, Random | Random |

> **Auto path length** generates a path that is approximately 60% of the total tiles in the grid.
> 

### Memorise Phase Settings

| Setting | Type | Range | Default |
| --- | --- | --- | --- |
| Memorise time limit | Integer (seconds) | 5s – 60s | 20s |
| Pattern flash mode | Select | Solid, Flashing (0.5s blink), Fading | Solid |

> The timer is the hard deadline. The first player to press "I'm Ready — Walk It!" ends the phase early for everyone. The first press always wins.
> 

### Walker Settings

| Setting | Type | Options | Default |
| --- | --- | --- | --- |
| Walker selection | Select | First to press (race), Host assigns | First to press |
| Walker can chat | Boolean | Yes / No | No |

> If the timer expires with no press, a walker is randomly assigned. Random assignment always applies as a fallback regardless of the walker selection setting.
> 

### Lives & Penalty Settings

| Setting | Type | Options | Default |
| --- | --- | --- | --- |
| Lives per walker | Integer | 1 – 5 | 2 |
| Wrong step penalty | Select | Lose a life + reset to start, Instant elimination | Lose a life + reset to start |
| Wrong step score deduction | Integer (points) | 0 – 200 | 50 |
| Life refill per round | Boolean | Yes / No | Yes |

### Elimination Settings

| Setting | Type | Options | Default |
| --- | --- | --- | --- |
| Elimination mode | Select | Off, On, Last round only | Off |
| Players eliminated per round | Integer | 1 – 3 | 1 |
| Eliminated players | Select | Spectate only, Stay but no walk | Spectate only |
| Minimum players to end game | Integer | 1 – 4 | 2 |

### Scoring Settings

| Setting | Type | Range | Default |
| --- | --- | --- | --- |
| Base points for success | Integer | 100 – 1000 | 500 |
| Speed bonus (max) | Integer | 0 – 500 | 300 |
| Volunteer bonus (max) | Integer | 0 – 200 | 50 |
| Helper hint reward — early walk | Integer | 10 – 100 | 40 |
| Helper hint reward — mid walk | Integer | 10 – 60 | 25 |
| Helper hint reward — late walk | Integer | 5 – 30 | 15 |

> **Volunteer bonus** scales with how early the button was pressed. Pressing with 20s left on a 20s timer earns the full bonus; pressing with 2s left earns very little. Randomly assigned walkers earn no volunteer bonus.
> 

### Round Settings

| Setting | Type | Range | Default |
| --- | --- | --- | --- |
| Number of rounds | Integer | 1 – 20 | 5 |
| Difficulty escalation | Boolean | Yes / No | No |
| Escalation step | Select | +1 grid row/col per round, −5s memorise time per round, Both | +1 grid row/col |

---

## 6. Round Flow

Each round follows exactly this sequence:

```
[Pattern Reveal + Volunteer Race] → [The Walk] → [Score Reveal] → [Elimination Check] → next round
```

### Phase 1 — Pattern Reveal + Volunteer Race

1. Server generates a random valid path from the start edge to the finish edge.
2. Path is rendered on the grid (lit tiles) and shown to all players.
3. Countdown timer begins (duration = memorise time setting).
4. A single **"I'm Ready — Walk It!"** button is shown to every non-eliminated player.
5. The first player to press the button becomes the walker. The press simultaneously means "I've memorised it" and "I'll walk."
6. As soon as any player presses:
    - Pattern disappears immediately for all players.
    - That player is locked in as the walker.
    - The walk phase begins.
7. If the timer reaches 0 and nobody has pressed:
    - Pattern disappears.
    - A walker is randomly assigned from all non-eliminated players.
    - Walk phase begins.
8. Walker is announced to all players. Chat opens for helpers.
9. Walker's screen shows the dark grid with start and end tiles marked. Helpers see the same view.

> **Design note:** The race to press happens while the pattern is still visible. Pressing early is a deliberate trade-off — you get the volunteer bonus and control, but you may have had less time to memorise. This creates natural tension each round.
> 

### Phase 2 — The Walk

1. Walker clicks tiles one at a time, moving from the start tile toward the finish.
2. Movement rules:
    - Walker may only move to adjacent tiles (up, down, left, right — no diagonals).
    - Walker may not skip tiles.
3. On each tile click:
    - **Correct tile:** Tile lights up in the walker's colour. Walker proceeds.
    - **Wrong tile (lives mode):** Life deducted. Walker snaps back to the start tile. All correctly walked tiles reset. Same path stays active — walk restarts from the beginning with one fewer life.
    - **Wrong tile (elimination mode):** Walker is eliminated from the round immediately.
4. Helpers type freely in chat. Chat is rate-limited per the difficulty setting.
5. Walker cannot type in chat during the walk.
6. A live timer counts up from 0 for speed bonus calculation.
7. Phase ends when:
    - Walker reaches the finish tile (success), OR
    - Walker runs out of lives (failure), OR
    - Walker is eliminated.

### Phase 3 — Score Reveal

1. Animated score breakdown shown to all players:
    - Walker: base score + speed bonus + volunteer bonus − deductions.
    - Each helper: hints followed, hints ignored, total points earned.
2. Running leaderboard updates.
3. 5-second display before moving on.

### Phase 4 — Elimination Check

1. If elimination mode is on, bottom N players are moved to spectator view.
2. If minimum player count is reached, game ends and final leaderboard is shown.
3. Otherwise, next round begins from Phase 1 with a new path.

---

## 7. Walker Mechanics

### Movement

- Walker can only click tiles adjacent to their current position (4-directional, no diagonals).
- Walker cannot go backwards to a tile already correctly walked — the path is one-way.
- Start tile is on the left edge of the grid; finish tile is on the right edge (host can switch to top/bottom).

### Wrong Step Behaviour

| Mode | Behaviour |
| --- | --- |
| Lives (default) | Life deducted. Walker is sent back to the start tile. All walked tiles reset. Same path stays active — walk restarts from the beginning with one fewer life. |
| Instant elimination | Walker is out of the round immediately. No restart. |

> The path never changes between attempts within the same round. Helpers and the walker can learn from each failed attempt and do better next time.
> 

### Tab Switch Penalty

- If the walker switches browser tabs during the walk phase:
    - First offence: warning shown to the lobby + 2-second tile-click freeze.
    - Second offence: warning + 4-second freeze.
    - Third offence: treated as a wrong step.

---

## 8. Helper & Chat Mechanics

### Chat Rules

- All non-walker players can type freely in chat during the walk phase.
- Chat is rate-limited server-side (varies by difficulty).
- Chat is read-only for the walker during their walk.
- Spectators (eliminated players) can chat but their messages appear in a visually muted style.

### Hint Scoring Logic

The server tracks which hints were "followed" by the walker:

- A hint is **followed** if the walker clicks the tile referenced in the message within 3 seconds of the message being sent.
- Coordinate formats recognised: `A3`, `B4`, `row 2 col 3`, `go right`, `go down`, `top left`, etc. (parsed loosely server-side).
- Correct hints not followed: no points, no penalty.
- Wrong hints (tile not on safe path) followed by walker: no penalty to helper — helpers are never penalised.

### Helper Point Zones

The path is divided into three zones for scoring:

| Zone | Tiles | Points per followed hint |
| --- | --- | --- |
| Early (first third) | Tiles 1 – ⌊path/3⌋ | 40 pts (default) |
| Mid (middle third) | Tiles ⌊path/3⌋+1 – ⌊2×path/3⌋ | 25 pts (default) |
| Late (final third) | Remaining tiles | 15 pts (default) |

---

## 9. Scoring System

### Walker Score

```
Walker Score = (Base Points × success_multiplier) + Speed Bonus + Volunteer Bonus − Wrong Step Deductions
```

- `success_multiplier` = 1.0 if success; 0.0 if failure in elimination mode; 0.5 if failure in lives mode and at least 50% of path was completed.
- Speed Bonus = `max_speed_bonus × (1 − time_taken / time_limit)` clamped to [0, max_speed_bonus].
- Volunteer Bonus = `max_volunteer_bonus × (time_remaining_when_pressed / total_memorise_time)` clamped to [0, max_volunteer_bonus]. Zero if randomly assigned.
- Wrong Step Deductions = `wrong_steps × deduction_per_step`.

### Helper Score

```
Helper Score = Σ(hint_points for each followed hint)
```

Applied per round and added to the running total. No deductions of any kind for helpers.

On failure: helpers who gave correct hints the walker ignored still keep half their hint points for that round.

### Leaderboard

- Cumulative score across all rounds, sorted descending.
- Tiebreaker: most successful walks → most correct hints followed → earliest join time.

---

## 10. Elimination & Lives System

### Lives Mode

- Each walker starts a round with N lives.
- Each wrong step costs 1 life and resets the walker to start.
- If lives reach 0, the walk fails. Score is partial if more than 50% of the path was completed, otherwise 0.
- Lives refill to full at the start of each new round (if Life refill is on).
- Players are never permanently eliminated in lives-only mode.

### Elimination Mode

- After each round, the bottom N players by cumulative score are eliminated.
- Eliminated players move to spectator mode: they can see the grid and chat but cannot volunteer.
- Spectator messages appear with a visually distinct style.
- Game ends when only 1 player remains OR the set number of rounds is completed.

### Combined Mode (lives + elimination)

- Lives apply during the walk (wrong steps cost lives within a round).
- Elimination applies after each round based on cumulative score.
- This is the default for Medium difficulty.

### Last Round Only Elimination

- No elimination during regular rounds — everyone plays all rounds.
- After the final round, the bottom half of players are eliminated for a single sudden-death bonus round.
- Winner of the sudden-death round wins the game regardless of cumulative score.

---

## 11. Lobby & Host Controls

### Creating a Lobby

1. Player enters a display name.
2. Clicks "Create Lobby."
3. Receives a 6-character room code (e.g. `XK92BT`).
4. Optionally sets a lobby password.
5. Shares the code with friends.

### Joining a Lobby

1. Player enters display name + room code (+ password if set).
2. Lands in the waiting room.
3. Can see other players' names and ready status.

### Host Controls (in lobby)

| Control | Description |
| --- | --- |
| Kick player | Remove a player from the lobby |
| Transfer host | Assign host role to another player |
| Start game | Available when ≥2 players are present |
| Difficulty | Select Easy / Medium / Hard / Custom |
| Game mode | Lives / Elimination / Combined / Last Round Only |
| Number of rounds | Set total rounds |
| Spectator slots | Allow extra spectator-only joins |

### Mid-Game Host Controls

| Control | Description |
| --- | --- |
| Pause game | Pause between rounds |
| Skip round | Skip the current round |
| End game early | Triggers the final leaderboard immediately |

---

## 12. Anti-Cheat & Anti-Screenshot

### Screenshot Disruption

The pattern phase renders on an HTML Canvas element with the following protections:

- A pixel-level noise overlay is applied on the canvas that is imperceptible at normal screen brightness but appears as visible noise in a screenshot or screen recording.
- The canvas uses a fullscreen overlay with `user-select: none` and `pointer-events: none` on all DOM children except the ready button.
- No path data is stored in any accessible DOM attribute — all path state lives server-side only.

### Tab Switch Detection

- `document.addEventListener('visibilitychange')` fires when the walker switches tabs.
- Penalty applied as described in Section 7.
- A public notice is shown to the entire lobby: "[Player] switched tabs."

### Dev Tools Detection

- The window `resize` event monitors for the viewport shrinking by more than 100px (the typical signature of dev tools opening).
- If detected during the pattern reveal phase, the pattern immediately hides and a warning is broadcast to the lobby.

### Chat Spam Prevention

- Rate limiting is enforced server-side per player per room.
- Messages over the rate limit are silently dropped.
- Players with more than 5 dropped messages in a row are temporarily muted for 10 seconds.

### Path Data Security

- The safe path is never sent to any client in plaintext.
- Only the walker receives one tile confirmation at a time — the server validates each click server-side.
- Helpers never receive path data; they only see the walker's position on the grid.
- All game state is authoritative on the server. The client sends "I clicked tile X" and the server responds "correct" or "wrong."

---

## 13. UI & Screen Breakdown

### Screen 1 — Home

- Game title and tagline.
- "Create Lobby" button.
- "Join Lobby" input (room code + name).
- Collapsible "How to play" section.

### Screen 2 — Lobby / Waiting Room

- Room code displayed prominently with a copy button.
- Player list (name, ready status, avatar colour).
- Host controls panel on the right side.
- Difficulty selector with a preview of all settings.
- Pre-game chat (no rate limit).
- Start Game button (host only, disabled until ≥2 players).

### Screen 3 — Pattern Reveal + Volunteer Race

- Full grid displayed with the safe path lit.
- Countdown timer (large, top centre).
- "I'm Ready — Walk It!" button (large, prominent) — pressing it volunteers you as walker and ends the pattern phase for everyone.
- Live indicator showing how many players haven't pressed yet.
- No chat during this phase.
- If timer hits 0 with no press: "Randomly assigning walker…" shown for 1 second before walk begins.

### Screen 4 — The Walk

- Grid (left/centre, takes majority of screen):
    - Walker's current position highlighted.
    - Correctly walked tiles lit in the walker's colour.
    - Start and finish tiles always marked.
- Live chat (right side):
    - Rate-limited input for helpers.
    - Walker's name shown differently — cannot type.
    - Spectator messages visually muted.
- Walker info bar (top): lives remaining, live timer, walker's name.
- Collapsible leaderboard sidebar (collapses on mobile).

### Screen 5 — Score Reveal

- Animated score cards for each player.
- Walker breakdown: base points + speed bonus + volunteer bonus − deductions.
- Helper breakdown: hints followed, hints ignored, total points.
- "Next Round" countdown (5 seconds).

### Screen 6 — Elimination (if applicable)

- Eliminated player names shown with an "Eliminated" badge.
- 3-second display before the next round.
- Eliminated players' screens transition to spectator view.

### Screen 7 — Final Leaderboard

- Full ranked list of all players with winner highlighted.
- End-of-game stats: total correct hints, times walked, fastest walk, most helpful player.
- "Play Again" (same lobby) and "Leave" buttons.

---

## 14. Tech Stack Recommendations

### Frontend

| Layer | Choice | Reason |
| --- | --- | --- |
| Framework | React (Vite) | Component model suits game phases well |
| Styling | Tailwind CSS | Fast, consistent utility classes |
| Grid rendering | HTML Canvas | Required for anti-screenshot noise overlay |
| Real-time | Socket.io client | Pairs with Socket.io server |
| State | Zustand | Lightweight, works well with socket events |

### Backend

| Layer | Choice | Reason |
| --- | --- | --- |
| Runtime | Node.js | Non-blocking, pairs naturally with Socket.io |
| Framework | Express | Minimal, handles lobby REST endpoints |
| Real-time | Socket.io | Rooms, events, and broadcasting built-in |
| Game state | In-memory (Map) | No database needed for active sessions |
| Path generation | Server-side only | Path never leaves the server |

### Deployment

| Service | Use |
| --- | --- |
| Vercel / Netlify | Frontend static hosting |
| Railway / Render | Node backend + Socket.io server |
| No database needed | All state is ephemeral per session |

---

## 15. Data Models

### Room

```tsx
type Room = {
  code: string;              // 6-char room code
  hostId: string;
  players: Player[];
  spectators: Player[];
  settings: GameSettings;
  state: 'lobby' | 'pattern' | 'walk' | 'score' | 'ended';
  currentRound: number;
  totalRounds: number;
  currentPath: number[][];   // Server-side only — never sent to clients
  currentWalkerId: string | null;
}
```

### Player

```tsx
type Player = {
  id: string;
  name: string;
  colour: string;            // Assigned on join
  score: number;             // Cumulative across rounds
  lives: number;             // Current lives this round
  isEliminated: boolean;
  isReady: boolean;
  isWalker: boolean;
  tabSwitchCount: number;
  hintsFollowed: number;
}
```

### GameSettings

```tsx
type GameSettings = {
  difficulty: 'easy' | 'medium' | 'hard' | 'custom';
  gridSize: { rows: number; cols: number };
  memoriseTime: number;            // seconds
  patternFlashMode: 'solid' | 'flashing' | 'fading';
  walkerSelection: 'first_press' | 'host'; // random fallback always applies on timer expiry
  walkerCanChat: boolean;
  livesPerWalker: number;
  wrongStepPenalty: 'life_and_reset' | 'eliminate';
  wrongStepDeduction: number;      // points deducted per wrong step
  eliminationMode: 'off' | 'on' | 'last_round';
  eliminationsPerRound: number;
  basePoints: number;
  maxSpeedBonus: number;
  maxVolunteerBonus: number;
  hintPointsEarly: number;
  hintPointsMid: number;
  hintPointsLate: number;
  chatRateLimit: number;           // ms between allowed messages
}
```

### Socket Events

```tsx
// Client → Server
'join_room'         { code, name, password? }
'press_ready_walk'  {}                              // volunteer + ready combined; first press wins
'click_tile'        { row: number, col: number }
'send_chat'         { message: string }
'start_game'        {}                              // host only

// Server → Client
'room_update'       { players, state, settings }
'phase_start'       { phase: 'pattern' | 'walk' | 'score' }
'pattern_reveal'    { path: number[][] }            // sent during pattern phase only, then cleared
'walker_assigned'   { playerId, name, wasRandom: boolean }
'tile_result'       { correct: boolean, lives: number }
'walker_position'   { row: number, col: number }
'chat_message'      { playerId, name, message, isSpectator }
'hint_scored'       { helperId, points }
'score_update'      { scores: { playerId, roundScore, total }[] }
'player_eliminated' { playerId, name }
'game_ended'        { leaderboard: Player[] }
```

---

## 16. Edge Cases & Rules

## 16. Edge Cases & Rules

| Situation | Rule |
| --- | --- |
| Walker disconnects mid-walk | Walk is cancelled. Round replays with the same path so other players' memorisation isn't wasted. Walker is not score-penalised but cannot press the volunteer button next round. |
| All players disconnect | Room is destroyed after 60 seconds of inactivity. |
| Nobody presses before timer expires | Walker is randomly assigned from all non-eliminated players. Volunteer bonus is 0 for randomly assigned walkers. |
| Only 1 player remains (elimination mode) | That player is declared the winner immediately. |
| Tie on leaderboard | Tiebreaker order: most successful walks → most correct hints → earliest join time. |
| Walker clicks a non-adjacent tile | Click is silently rejected. No penalty. |
| Helper hint matches multiple tiles | Server matches the closest valid tile to the walker's current position. |
| Host leaves mid-game | Host role is automatically transferred to the next player in join order. |
| Player joins mid-game | They join as spectator only. They can play from the next game. |
| Grid size changed in custom with Auto path length | Auto recalculates to 60% of total tiles, rounded to the nearest integer. |
| Pattern flash mode set to Fading | Pattern fades out over the last 5 seconds of memorise time rather than disappearing instantly. |
