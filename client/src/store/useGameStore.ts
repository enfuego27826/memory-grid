import { create } from 'zustand';
import type {
  LeaderboardEntry, Phase, PlayerView, ScoreLine, ServerMsg,
} from '../types/wire';
import type { FlashMode, GameSettings } from '../types/settings';

export interface ChatEntry { key: number; from: number; name: string; text: string; spectator: boolean; }
export interface Notice { key: number; text: string; }

type ConnStatus = 'idle' | 'connecting' | 'open' | 'reconnecting' | 'closed';

interface State {
  // connection
  status: ConnStatus;
  token: string | null;
  myId: number | null;
  roomCode: string | null;
  myName: string | null;

  // room snapshot
  phase: Phase;
  players: PlayerView[];
  settings: GameSettings | null;
  currentRound: number;
  totalRounds: number;
  paused: boolean;

  // walker
  walkerId: number | null;
  walkerWasRandom: boolean;
  startTile: { row: number; col: number } | null;
  finishTile: { row: number; col: number } | null;

  // TRANSIENT pattern path — present ONLY during the memorise phase, in the JS
  // heap (never a DOM attribute), dropped on phase change / walker assigned / hide.
  patternPath: [number, number][] | null;
  flashMode: FlashMode;
  patternHidden: boolean;

  // walk runtime
  walkerPos: { row: number; col: number } | null;
  walkedTiles: number[]; // flat indices in walker colour
  myLives: number | null;
  walkStartedAt: number | null;

  // chat + notices
  chat: ChatEntry[];
  notices: Notice[];
  lastChatSentAt: number;

  // score reveal
  roundScores: ScoreLine[];
  eliminatedThisRound: number[];

  // final
  leaderboard: LeaderboardEntry[];

  // deadlines (client-side display only; server is authoritative)
  memoriseDeadline: number | null;
}

interface Actions {
  setStatus(s: ConnStatus): void;
  setIdentity(name: string, code: string): void;
  apply(msg: ServerMsg): void;
  hidePattern(): void;
  markChatSent(): void;
  reset(): void;
}

const flatIdx = (row: number, col: number, cols: number) => row * cols + col;
let keySeq = 1;

export const useGameStore = create<State & Actions>((set, get) => ({
  status: 'idle',
  token: null,
  myId: null,
  roomCode: null,
  myName: null,
  phase: 'lobby',
  players: [],
  settings: null,
  currentRound: 0,
  totalRounds: 0,
  paused: false,
  walkerId: null,
  walkerWasRandom: false,
  startTile: null,
  finishTile: null,
  patternPath: null,
  flashMode: 'solid',
  patternHidden: false,
  walkerPos: null,
  walkedTiles: [],
  myLives: null,
  walkStartedAt: null,
  chat: [],
  notices: [],
  lastChatSentAt: 0,
  roundScores: [],
  eliminatedThisRound: [],
  leaderboard: [],
  memoriseDeadline: null,

  setStatus: (s) => set({ status: s }),
  setIdentity: (name, code) => set({ myName: name, roomCode: code }),
  markChatSent: () => set({ lastChatSentAt: Date.now() }),

  hidePattern: () => set({ patternHidden: true, patternPath: null }),

  apply: (msg) => {
    const s = get();
    const cols = s.settings?.gridSize.cols ?? 6;
    switch (msg.type) {
      case 'joined':
        set({ myId: msg.playerId, token: msg.token });
        break;

      case 'room_update': {
        const leftPattern = s.phase === 'pattern' && msg.state !== 'pattern';
        set({
          players: msg.players,
          settings: msg.settings,
          phase: msg.state,
          currentRound: msg.currentRound,
          totalRounds: msg.totalRounds,
          paused: msg.paused,
          // drop transient path if we are no longer in the memorise phase
          patternPath: msg.state === 'pattern' ? s.patternPath : null,
          patternHidden: msg.state === 'pattern' ? s.patternHidden : false,
        });
        if (leftPattern) set({ patternPath: null });
        break;
      }

      case 'phase_start':
        if (msg.phase === 'pattern') {
          set({
            phase: 'pattern',
            patternHidden: false,
            patternPath: null,
            walkedTiles: [],
            walkerPos: null,
            roundScores: [],
            eliminatedThisRound: [],
            memoriseDeadline: s.settings ? Date.now() + s.settings.memoriseTime * 1000 : null,
          });
        } else if (msg.phase === 'walk') {
          set({ phase: 'walk', patternPath: null, walkStartedAt: Date.now() });
        } else {
          // any other phase (score/elimination/ended/lobby) drops the transient path
          set({ phase: msg.phase, patternPath: null });
        }
        break;

      case 'pattern_reveal':
        // accept ONLY while genuinely in the memorise phase and not hidden
        if (s.phase === 'pattern' && !s.patternHidden)
          set({ patternPath: msg.path, flashMode: msg.flashMode });
        break;

      case 'walker_assigned':
        set({
          walkerId: msg.playerId,
          walkerWasRandom: msg.wasRandom,
          startTile: { row: msg.startRow, col: msg.startCol },
          finishTile: { row: msg.finishRow, col: msg.finishCol },
          walkerPos: { row: msg.startRow, col: msg.startCol },
          walkedTiles: [flatIdx(msg.startRow, msg.startCol, cols)],
          patternPath: null, // pattern vanishes for everyone
        });
        break;

      case 'walker_position':
        set((st) => {
          const idx = flatIdx(msg.row, msg.col, cols);
          const walked = st.walkedTiles.includes(idx) ? st.walkedTiles : [...st.walkedTiles, idx];
          return { walkerPos: { row: msg.row, col: msg.col }, walkedTiles: walked };
        });
        break;

      case 'walker_reset':
        set((st) => ({
          myLives: st.walkerId === st.myId ? msg.lives : st.myLives,
          walkerPos: st.startTile,
          walkedTiles: st.startTile ? [flatIdx(st.startTile.row, st.startTile.col, cols)] : [],
        }));
        break;

      case 'tile_result':
        set({ myLives: msg.lives });
        break;

      case 'chat_message':
        set((st) => ({
          chat: [...st.chat.slice(-200),
            { key: keySeq++, from: msg.from, name: msg.name, text: msg.message, spectator: msg.isSpectator }],
        }));
        break;

      case 'hint_scored':
        // surfaced via the chat/score; a transient notice keeps it visible
        set((st) => ({ notices: [...st.notices.slice(-8),
          { key: keySeq++, text: `Hint followed: +${msg.points}` }] }));
        break;

      case 'score_update':
        set({ roundScores: msg.scores });
        break;

      case 'player_eliminated':
        set((st) => ({ eliminatedThisRound: [...st.eliminatedThisRound, msg.playerId] }));
        break;

      case 'game_ended':
        set({ leaderboard: msg.leaderboard, phase: 'ended' });
        break;

      case 'tab_switch_notice':
        set((st) => ({ notices: [...st.notices.slice(-8),
          { key: keySeq++, text: `${msg.name} switched tabs (offence ${msg.offence})` }] }));
        break;

      case 'system_notice':
        set((st) => ({ notices: [...st.notices.slice(-8), { key: keySeq++, text: msg.text }] }));
        break;
    }
  },

  reset: () => set({
    status: 'idle', token: null, myId: null, roomCode: null, myName: null,
    phase: 'lobby', players: [], settings: null, currentRound: 0, totalRounds: 0, paused: false,
    walkerId: null, startTile: null, finishTile: null, patternPath: null, patternHidden: false,
    walkerPos: null, walkedTiles: [], myLives: null, walkStartedAt: null,
    chat: [], notices: [], roundScores: [], eliminatedThisRound: [], leaderboard: [],
    memoriseDeadline: null,
  }),
}));

// ---- derived selectors ----
// Law of Demeter: components ask these selectors rather than reaching through
// `store.players.find(...)?.field` or `store.settings.gridSize.cols` themselves.
export const myPlayer = (s: State) => s.players.find((p) => p.id === s.myId) ?? null;
export const amHost = (s: State) => myPlayer(s)?.isHost ?? false;
export const amWalker = (s: State) => s.myId != null && s.myId === s.walkerId;
export const playerName = (s: State, id: number) => s.players.find((p) => p.id === id)?.name ?? `#${id}`;
const walker = (s: State) => s.players.find((p) => p.id === s.walkerId) ?? null;
export const gridDims = (s: State) => ({ rows: s.settings?.gridSize.rows ?? 6, cols: s.settings?.gridSize.cols ?? 6 });
export const walkerColour = (s: State) => walker(s)?.colour ?? '#60a5fa';
export const walkerLives = (s: State) =>
  s.myId != null && s.myId === s.walkerId ? s.myLives ?? 0 : walker(s)?.lives ?? 0;
