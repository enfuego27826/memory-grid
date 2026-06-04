#!/usr/bin/env bash
# Build the server + client and (re)start the Memory Grid systemd --user service.
# Serves the whole app (client + API + WebSocket) on a single origin, port 8080.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
export PATH="$HOME/.local/bin:$PATH"   # pip-installed cmake

echo "[deploy] building server (mg_server)…"
# IMPORTANT: -j1 (single job). The uWebSockets template TU needs ~1GB to compile;
# a bare `-j` spawns unlimited parallel compiles and OOM-kills on this 1.8GB box.
cmake -S "$ROOT/server" -B "$ROOT/server/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/server/build" -j1 --target mg_server

# Port 80 needs cap_net_bind_service; a rebuild replaces the binary and drops it,
# so re-apply (prompts for sudo password once).
echo "[deploy] re-applying cap_net_bind_service (sudo)…"
sudo setcap 'cap_net_bind_service=+ep' "$ROOT/server/build/mg_server"

echo "[deploy] building client (Vite)…"
cd "$ROOT/client"
npm install --no-audit --no-fund
npm run build

echo "[deploy] restarting service…"
systemctl --user daemon-reload
systemctl --user enable --now memory-grid.service
systemctl --user restart memory-grid.service
echo "[deploy] done → http://anurag.tnkr.be/"
