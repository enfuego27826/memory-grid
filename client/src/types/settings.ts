// Mirror of the backend GameSettings wire shape (server is authoritative).
export type Difficulty = 'easy' | 'medium' | 'hard' | 'custom';
export type PathStyle = 'straight' | 'zigzag' | 'random';
export type FlashMode = 'solid' | 'flashing' | 'fading';
export type WalkerSelection = 'first_press' | 'host';
export type WrongStepPenalty = 'life_and_reset' | 'eliminate';
export type EliminationMode = 'off' | 'on' | 'last_round';

export interface GameSettings {
  difficulty: Difficulty;
  gridSize: { rows: number; cols: number };
  pathLength: number;
  pathLengthAuto: boolean;
  pathStyle: PathStyle;
  startEdge: 'left' | 'top';
  memoriseTime: number;
  patternFlashMode: FlashMode;
  walkerSelection: WalkerSelection;
  walkerCanChat: boolean;
  livesPerWalker: number;
  wrongStepPenalty: WrongStepPenalty;
  wrongStepDeduction: number;
  lifeRefill: boolean;
  eliminationMode: EliminationMode;
  eliminationsPerRound: number;
  eliminatedPlayers: 'spectate' | 'stay';
  minPlayers: number;
  basePoints: number;
  maxSpeedBonus: number;
  maxVolunteerBonus: number;
  hintPointsEarly: number;
  hintPointsMid: number;
  hintPointsLate: number;
  numRounds: number;
  difficultyEscalation: boolean;
  escalationStep: 'grid' | 'memorise' | 'both';
  chatRateLimit: number;
}

// Human-readable summary rows for the lobby settings preview.
export function settingsRows(s: GameSettings): [string, string][] {
  return [
    ['Grid', `${s.gridSize.rows} × ${s.gridSize.cols}`],
    ['Memorise', `${s.memoriseTime}s`],
    ['Path length', s.pathLengthAuto ? `Auto (${s.pathLength})` : `${s.pathLength}`],
    ['Path style', s.pathStyle],
    ['Lives', `${s.livesPerWalker}`],
    ['Wrong step', s.wrongStepPenalty === 'eliminate' ? 'Eliminate' : 'Lose life + reset'],
    ['Elimination', s.eliminationMode],
    ['Rounds', `${s.numRounds}`],
    ['Chat limit', `${(s.chatRateLimit / 1000).toFixed(1)}s`],
    ['Base points', `${s.basePoints}`],
  ];
}
