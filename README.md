# BerkIDE TUI

**Terminal UI client** for BerkIDE Core.

A lightweight terminal-based user interface that connects to a running [berkide-core](https://github.com/berkide/berkide-core) server via WebSocket. Renders editor state in the terminal with real-time updates.

## Quick Start

### Prerequisites

- C++20 compiler
- CMake 3.16+
- A running berkide-core server

### Build

```bash
git clone https://github.com/berkide/berkide-tui.git
cd berkide-tui
./build.sh
```

### Run

```bash
# Start berkide-core first (default: ws://127.0.0.1:1882)
./build/berkide-tui
```

## Architecture

```
┌────────────────────┐     WebSocket      ┌──────────────────┐
│    berkide-tui      │ ◄────────────────► │   berkide-core   │
│  Terminal Renderer  │    Real-time sync  │  Headless Server │
└────────────────────┘                    └──────────────────┘
```

The TUI is a **thin client** — all editor logic lives in berkide-core. The TUI only:
- Captures keyboard input
- Sends key events to core via WebSocket
- Receives state updates
- Renders to terminal

## Project Structure

```
src/
├── main.cpp        # Entry point
├── TuiClient.h/cpp # WebSocket client + state sync
├── BerkTerm.h/cpp  # Terminal rendering engine
└── input.h/cpp     # Keyboard input handling
```

## Related Projects

| Project | Description |
|---------|-------------|
| [berkide](https://github.com/berkide/berkide) | Umbrella project |
| [berkide-core](https://github.com/berkide/berkide-core) | Headless editor server (required) |
| [berkidectl](https://github.com/berkide/berkidectl) | CLI management tool |
| [berkide-plugins](https://github.com/berkide/berkide-plugins) | Official plugin collection |

## Branch Strategy

- `main` — Stable releases only. Protected.
- `dev` — Active development. All PRs target `dev`.
- Feature branches from `dev`: `feature/xxx`, `fix/xxx`

**Never push directly to `main`.** All changes go through `dev` first.

## License

MIT
