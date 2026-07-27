# Contributing

This repository is the game client: the Armagetron Advanced engine, its
WebAssembly port, and the browser shell. It is GPL-2.0-or-later — see
[`LICENSES.md`](LICENSES.md) for what that covers and how to reproduce a build.

Wasmagetron is an independent port. Armagetron Advanced is the work of its own
authors and contributors, and changes that are not browser-specific are usually
better sent upstream than carried here.

## What is wanted

- Fixes to the WebAssembly port: rendering, input, audio, persistence
- Mobile and touch improvements
- Browser networking work (see the relay design in the platform repository)
- **Ports of other open-source games to WebAssembly.** Enrich Genius is
  intended as a platform for browser-native ports, not just this one game. If
  you have a port in mind, open an issue before starting so we can talk about
  whether it fits and how it would be hosted.

## Building

```bash
./webbuild/build-libxml2.sh     # once
./webbuild/build.sh             # emits webbuild/dist/
```

Exact toolchain versions are in [`LICENSES.md`](LICENSES.md). `webbuild/dist/` is
committed, so a change to the client is not complete until it is rebuilt.

## How changes ship

```text
branch  ->  pull request  ->  preview deploy  ->  merge  ->  release
```

- A pull request deploys a preview to `pr-<number>.armagetronad-wasm.pages.dev`
  and comments the link. Previews sit behind Cloudflare Access; sign in with a
  Cloudflare account to view one.
- Merging to `release_0.2.9-wasm` publishes a client release tarball.
- Production is deployed from the platform repository, which consumes that
  release. This repository never deploys production, so a deploy always names an
  immutable artifact rather than whatever a branch pointed at.

`webbuild/dist` is committed, and CI publishes exactly what a reviewer saw
rather than rebuilding. So **rebuild before you push**: CI fails the branch when
tracked sources are newer than the committed build, rather than shipping a
client that silently lacks your change.

```bash
./webbuild/build.sh
git add webbuild/dist && git commit
```

## Before you open a pull request

- Say which browsers and devices you actually tested on. "Should work" is not a
  test, and this port has been bitten repeatedly by things that look correct and
  are not.
- Keep Emscripten-specific code behind `__EMSCRIPTEN__` and confined to as few
  files as possible.
- Do not put platform concerns — accounts, billing, matchmaking, relay policy —
  in this repository. They live in a separate, non-GPL service.
- Do not add secrets, API keys or infrastructure configuration to the client. It
  is downloaded to every player's device.

## Contributor License Agreement

Outside contributions of source ports or WebAssembly work require a signed CLA
before they can be merged.

**This is not set up yet.** The CLA text, the signing mechanism, and how it
interacts with the GPL on contributions to this repository all still need to be
settled with a licensing attorney. Until that is done:

- Small bug fixes are welcome and will be treated as ordinary GPL contributions.
- Larger contributions, and anything intended for the commercial hosted service,
  should wait or be discussed on an issue first, so nobody spends effort on work
  that cannot yet be accepted.

If you are reading this and the CLA still is not here, please ask on the issue
tracker rather than assuming.

## Reporting problems

Include the build version, shown in the page source as `assetVersion`, plus the
browser, device, and anything in the developer console. For rendering problems a
screenshot is worth more than a description.
