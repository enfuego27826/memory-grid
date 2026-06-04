# Deployment (this server)

The whole app — React client **and** the API/WebSocket — is served from a single
origin by `mg_server` (the uWebSockets binary), so there's no reverse proxy and
no mixed-content (`http://` + `ws://`, same host/port).

- **URL:** http://anurag.tnkr.be/  (DNS → `13.206.15.73`, this box's public IP).
- **Port:** 80. The server runs as a normal user; `mg_server` was granted
  `cap_net_bind_service` (`sudo setcap cap_net_bind_service=+ep …`) so it can bind
  80 without running as root. A rebuild replaces the binary and drops the cap —
  `deploy.sh` re-applies it (prompts for sudo).
- **Firewall:** inbound **TCP 80** must be allowed in security group
  `sg-0d7e67294439f9c40` (`india-bootcamp-2025`, region `ap-south-1`). This is an
  AWS-side rule, set in the console / via the AWS CLI — not on the box.
- **Process:** a systemd **user** service, `memory-grid.service`
  (`~/.config/systemd/user/memory-grid.service`), `Restart=always`, enabled on
  `default.target`. `loginctl enable-linger` is on, so it survives logout/reboot.
  - `systemctl --user status memory-grid` — state
  - `systemctl --user restart memory-grid` — restart
  - `journalctl --user -u memory-grid` — logs
- **ExecStart:** `mg_server 80 /home/user/memory-grid/client/dist`
  (arg 1 = port, arg 2 = static dir; empty arg 2 serves the built-in smoke page).

## Rebuild + redeploy

```
/home/user/memory-grid/deploy.sh
```

Builds `mg_server` (Release) and the Vite client, then restarts the service.

## Memory (1.8 GB box — important)

- **Build with `-j1`.** A bare `cmake --build -j` uses *unlimited* parallel jobs;
  the uWebSockets template TU (`uws_transport.cpp`) needs ~1 GB on its own, so
  parallel compiles OOM-kill the box (exit 137). `deploy.sh` pins `-j1`. The
  lightweight targets (`mg_tests`, `mg_transport_tests`, `mg_core`, `mg_transport`)
  don't pull uWS and build cheaply.
- **Runtime footprint is small** — the server is in-memory only; stopping it frees
  ~10 MB. The memory pressure is the *build*, not the running process.
- **Leak/bloat audit:** core + transport are valgrind-clean. Fixed: session tokens
  are now revoked on player-leave and room-teardown (`byToken_` previously grew
  unbounded — `revoke()` was never called); HTTP POST bodies are capped at 64 KB
  (only WS frames were capped before). Timer ctx alloc is paired (new/delete).

## Notes

- The client uses **same-origin relative URLs** (`/create`, `/join`, `/ws`), so it
  works behind any host/port without rebuild.
- For TLS, terminate at a proxy and the client auto-uses `wss://` (it derives the
  scheme from `location.protocol`).
