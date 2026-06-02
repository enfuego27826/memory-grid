import type { FlashMode, GameSettings } from './settings';

export type Phase = 'lobby' | 'pattern' | 'walk' | 'score' | 'elimination' | 'ended';

export interface PlayerView {
  id: number;
  name: string;
  colour: string;
  score: number;
  lives: number;
  isEliminated: boolean;
  isWalker: boolean;
  isSpectator: boolean;
  isHost: boolean;
  isReady: boolean;
}

export interface ScoreLine {
  playerId: number;
  roundScore: number;
  total: number;
  base: number;
  speed: number;
  volunteer: number;
  deductions: number;
  hintsFollowed: number;
  hintsIgnored: number;
}

export interface LeaderboardEntry {
  playerId: number;
  name: string;
  total: number;
  successfulWalks: number;
  hintsFollowed: number;
}

// ---- Client → Server ------------------------------------------------------
export type ClientMsg =
  | { type: 'join_room'; code: string; name: string; token?: string; password?: string }
  | { type: 'press_ready_walk' }
  | { type: 'click_tile'; row: number; col: number }
  | { type: 'send_chat'; message: string }
  | { type: 'start_game' }
  | { type: 'configure_room'; settings: Partial<GameSettings> & { difficulty: string } }
  | { type: 'skip_round' }
  | { type: 'end_game' }
  | { type: 'pause_toggle' }
  | { type: 'kick_player'; target: number }
  | { type: 'transfer_host'; target: number }
  | { type: 'tab_switched' };

// ---- Server → Client ------------------------------------------------------
export type ServerMsg =
  | { type: 'joined'; playerId: number; token: string }
  | { type: 'room_update'; players: PlayerView[]; state: Phase; settings: GameSettings;
      currentRound: number; totalRounds: number; paused: boolean }
  | { type: 'phase_start'; phase: Phase }
  | { type: 'pattern_reveal'; path: [number, number][]; flashMode: FlashMode }
  | { type: 'walker_assigned'; playerId: number; name: string; wasRandom: boolean;
      startRow: number; startCol: number; finishRow: number; finishCol: number }
  | { type: 'tile_result'; correct: boolean; lives: number }
  | { type: 'walker_position'; row: number; col: number }
  | { type: 'walker_reset'; lives: number }
  | { type: 'chat_message'; from: number; name: string; message: string; isSpectator: boolean }
  | { type: 'hint_scored'; helperId: number; points: number }
  | { type: 'score_update'; scores: ScoreLine[] }
  | { type: 'player_eliminated'; playerId: number; name: string }
  | { type: 'game_ended'; leaderboard: LeaderboardEntry[] }
  | { type: 'tab_switch_notice'; playerId: number; name: string; offence: number }
  | { type: 'system_notice'; text: string };
