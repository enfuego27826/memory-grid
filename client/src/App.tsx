import { useEffect } from 'react';
import { useGameStore } from './store/useGameStore';
import { resume } from './net/WebSocketService';
import HomeScreen from './screens/HomeScreen';
import LobbyScreen from './screens/LobbyScreen';
import PatternScreen from './screens/PatternScreen';
import WalkScreen from './screens/WalkScreen';
import ScoreScreen from './screens/ScoreScreen';
import EliminationScreen from './screens/EliminationScreen';
import FinalScreen from './screens/FinalScreen';
import { ConnectionBanner, NoticeFeed } from './components/panels';

export default function App() {
  const roomCode = useGameStore((s) => s.roomCode);
  const phase = useGameStore((s) => s.phase);

  // Resume an in-progress session after a page reload.
  useEffect(() => { if (useGameStore.getState().status === 'idle') resume(); }, []);

  let screen: JSX.Element;
  if (!roomCode) screen = <HomeScreen />;
  else switch (phase) {
    case 'pattern': screen = <PatternScreen />; break;
    case 'walk': screen = <WalkScreen />; break;
    case 'score': screen = <ScoreScreen />; break;
    case 'elimination': screen = <EliminationScreen />; break;
    case 'ended': screen = <FinalScreen />; break;
    default: screen = <LobbyScreen />;
  }

  return (
    <>
      <NoticeFeed />
      {screen}
      <ConnectionBanner />
    </>
  );
}
