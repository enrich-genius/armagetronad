# Web Port Lessons from WebMonkeyBall

This note captures the WebMonkeyBall lessons that should guide Wasmageddon.
WebMonkeyBall is useful as an architecture reference, not as a direct template:
it is largely a TypeScript reimplementation of Super Monkey Ball behavior, while
this project already has the Armagetron Advanced C++ source available.

## Core Decision

Keep the existing Armagetron C++ engine authoritative for simulation, gameplay,
rendering, and protocol behavior. Use JavaScript or TypeScript for private
browser shell and platform work only: loading, responsive layout, mobile
controls, browser storage, matchmaking, lobby codes, error states, attribution,
and future account UI.

The current code already follows this direction:

- `webbuild/build.sh` compiles the existing C++ client sources with Emscripten,
  SDL 1.2 ports, legacy OpenGL emulation, Asyncify, IDBFS, and preloaded assets.
- `src/tron/gGame.cpp` remains the central game loop and owns input processing,
  game time, networking calls, simulation updates, camera updates, and rendering.
- `webbuild/shell.html` handles browser lifecycle concerns: canvas sizing,
  persistent storage mounting, relay URL setup, touch controls, loading UI, and
  mobile keyboard integration.

Do not start a TypeScript or Rust rewrite of the Armagetron simulation as an MVP
strategy.

## Boundaries

### C++ Engine

The C++ engine owns:

- cycle movement, turning, braking, rubber, trails, wall collision, scoring, AI,
  camera semantics, game rules, replay semantics, and Armagetron protocol logic
- compatibility with native servers and existing community expectations
- any behavior that affects match outcomes or game feel

Browser changes in this layer should be minimal, targeted, and justified by a
specific browser constraint.

### Web Shell

The browser shell owns:

- progressive loading and useful failure messages
- mobile controls, gestures, touch menu behavior, fullscreen state, and virtual
  keyboard integration
- lobby creation, lobby listing, join-by-code, QR join, and account/community UI
- settings UI that adapts desktop-era options to browser-safe choices
- browser storage, asset caching, source/license screens, and attribution

The shell should expose a narrow, versioned bridge to the engine over time rather
than calling into arbitrary internal C++ objects.

The modern TypeScript website/shell is a private platform layer. Do not design
the project as though that source code will be published. Any source publication
obligations should be satisfied by publishing the modified GPL client source and
corresponding build scripts/artifacts for the distributed C++/Wasm client, not
by exposing private product UI or service code.

### Platform Services

The platform services own:

- Cloudflare Durable Object lobby codes and public/private room listing
- OTP integration
- WebSocket-to-UDP relay or future gateway
- service auth, rate limits, diagnostics, moderation, and billing/service access

The relay must stay protocol-thin. It should translate browser transport to the
existing game protocol and should not reimplement game rules.

Platform services are private service code. Keep them out of the GPL client
repository and interact through documented HTTPS/WebSocket APIs.

## Timing and Determinism

Preserve Armagetron timing semantics instead of making the browser refresh rate
authoritative.

Known current anchors:

- `src/tron/gGame.cpp` derives `gtime` from `se_GameTime()` and calls
  `Timestep(gtime, true)` when game time advances.
- `src/tools/tSysTime.cpp` uses `emscripten_sleep()` under `__EMSCRIPTEN__` so
  the blocking desktop-style loop yields through Asyncify instead of freezing
  the tab.
- `webbuild/build.sh` currently enables `-sASYNCIFY=1` with an enlarged Asyncify
  stack.

Implementation rule: browser rendering can adapt to canvas size and frame
presentation, but movement, collision, networking, and match state should not be
rewired to a variable `requestAnimationFrame` timestep without a parity plan.

## Rendering Lessons

Recent wall/camera bugs reinforce this boundary: rendering fixes should correct
browser projection, depth, texture-coordinate, or fixed-function emulation
behavior without changing simulation state.

When debugging browser render issues:

- compare native behavior first
- identify whether the bug is camera math, WebGL projection/depth precision,
  legacy GL emulation, texture wrapping, asset loading, or model data
- avoid changing collision or wall generation just to hide visual artifacts
- prefer a small compatibility adapter or render-path clamp over broad gameplay
  changes

Relevant current files include `src/engine/eCamera.cpp`,
`src/engine/eDisplay.cpp`, `src/tron/gWall.cpp`, `src/tron/gCycle.cpp`,
`src/render/rScreen.cpp`, `src/render/rSysdep.cpp`, and
`webbuild/gl_compat.c`.

## Asset and Storage Lessons

Keep assets separate from the Wasm binary. The current build already packages
`webbuild/data` into `/data` and mounts writable settings at `/persist` with
IDBFS from `webbuild/shell.html`.

Next steps should move toward a browser content manifest:

```json
{
  "id": "classic-arena-pack",
  "version": "1.0.0",
  "arenas": [
    {
      "id": "square",
      "config": "arenas/square.xml",
      "preview": "previews/square.webp"
    }
  ],
  "textures": "textures/",
  "sounds": "sounds/",
  "license": "LICENSE.txt"
}
```

All future community maps, packs, replays, and imported settings must be treated
as untrusted input with size limits, path validation, schema checks, and clear
license metadata.

## Licensing Boundary

The public source boundary should be intentional:

- Publish the modified GPL Armagetron Advanced client source that is distributed
  to users as WebAssembly, along with scripts and instructions needed to produce
  the corresponding browser client build.
- Keep the TypeScript website, matchmaking UI, account UI, analytics,
  monetization, administrative tools, and Cloudflare Worker/platform services in
  private repositories.
- Do not copy private TypeScript platform code into the GPL client repo.
- Do not link private service logic into the C++/Wasm client.
- Keep communication between the GPL client and private platform over stable,
  documented HTTP/WebSocket APIs.
- Keep attribution, upstream notices, third-party notices, and source links
  visible in the browser-accessible About/license UI.

This is an engineering boundary, not legal advice. It should be reviewed before
commercial launch, but implementation work should preserve this separation by
default.

## Multiplayer Lessons

Do not put the authoritative server in the browser for the MVP.

Short-term architecture:

```text
Browser client
      |
      | WebSocket
      v
Protocol-thin relay or gateway
      |
      | Existing Armagetron protocol
      v
Native authoritative server
```

The platform Durable Object lobby layer should be the user-facing MVP:

- create public/private lobby
- issue short code through the OTP service when configured
- list public lobbies
- join by code on mobile
- resolve room code to a relay endpoint

The raw relay URL field should remain a developer escape hatch, not the primary
user experience.

## Testing Requirements

Before large simulation or networking changes, add deterministic comparison
tests. A useful harness should feed identical input recordings into native and
Wasm builds and compare snapshots with a documented floating-point tolerance.

Initial snapshot fields:

- cycle position, direction, speed, trail points, alive/collision state
- rubber state and wall proximity effects
- camera position and orientation for known camera modes
- round state, timer, score, and spawned players

This is more important than making internals look cleaner. Existing native
behavior is the specification.

## Browser Platform Research Checklist

Each browser-port change should improve or update this map:

| Area | Current anchor | Browser risk | Near-term action |
| --- | --- | --- | --- |
| Build system | `webbuild/build.sh` | hidden native-only source assumptions | keep source list explicit, document excluded systems |
| SDL/input | `src/ui/uInput.cpp`, `webbuild/sdl_compat.c`, `webbuild/shell.html` | SDL 1 key mapping and touch translation | keep touch in shell; feed SDL-compatible key/mouse events |
| OpenGL | `src/render/*`, `webbuild/gl_compat.c` | fixed-function emulation and depth/texture precision | isolate WebGL-specific clamps behind `__EMSCRIPTEN__` |
| Main loop | `src/tron/gGame.cpp`, `src/tools/tSysTime.cpp` | blocking loop vs browser event loop | keep Asyncify yield points narrow and documented |
| Storage | `webbuild/shell.html`, `webbuild/config.h` | over-syncing IDBFS or storing large assets poorly | localStorage for UI prefs, IDBFS/IndexedDB for game data |
| Networking | `src/network/*`, platform `relay/` | browser cannot use UDP | use WebSocket relay/gateway; document protocol assumptions |
| Assets | `webbuild/data`, `--preload-file` | large downloads and licensing boundaries | add manifest, hashes, provenance, and CDN/cache plan |
| Audio | `webbuild/audio_compat.c` | current audio is incomplete/silent | keep optional until loader/mixer path is real |
| Threads | build currently single-threaded | pthreads require COOP/COEP and SharedArrayBuffer | do not enable by default without a measured need |

## Milestone Order

1. Stabilize existing browser build: camera, walls, cycle rendering, menus,
   touch input, fullscreen, and storage.
2. Add deterministic native-vs-Wasm simulation recordings.
3. Make the Durable Object lobby flow the primary multiplayer entry point.
4. Connect lobby codes to relay-backed native servers.
5. Add public server listing, mobile join-by-code, and QR join.
6. Add content manifests, source/license screens, and asset provenance.
7. Add replay browsing, spectating, account/community features, and donations.

## Principle

Use WebMonkeyBall's lesson on boundaries, not its rewrite strategy. Wasmageddon
should preserve the Armagetron C++ engine and native server ecosystem while
building a modern browser-native shell and platform around it.
