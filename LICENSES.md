# Licensing and source availability

This repository holds the Armagetron Advanced game client, including the
WebAssembly port that runs at <https://armagetronad.enrichgenius.com>.

**This document is a description of how the project is organised, not legal
advice, and no part of it should be read as a legal conclusion. The boundary
described in "Separate services" below is the one most worth having an
open-source licensing attorney review before commercial launch.**

## The client is GPL

Armagetron Advanced is licensed under the GNU General Public License, version 2
or later. The full text is in [`COPYING.txt`](COPYING.txt).

Serving the compiled client to a browser downloads it to the user's device, so
it is distributed software. Everyone who loads the page receives the program and
is entitled to its corresponding source.

Everything below is GPL-covered and is published in this repository:

| Component | Path |
| --- | --- |
| Engine, game logic, renderer | `src/` |
| Emscripten build configuration | `webbuild/build.sh`, `webbuild/config.h` |
| libxml2 cross-build script | `webbuild/build-libxml2.sh` |
| Browser compatibility shims compiled into the client | `webbuild/*_compat.c`, `webbuild/glu_shim.c`, `webbuild/prefix.c` |
| Page shell, mobile touch controls, input glue | `webbuild/shell.html` |
| Packaged game data | `webbuild/data/` |
| Built artifacts served to users | `webbuild/dist/` |

`webbuild/shell.html` is listed deliberately. It is not a neutral wrapper: it is
passed to `emcc` as `--shell-file`, so it is the template the client is emitted
into, and it carries the virtual controls, swipe steering and key synthesis that
the game depends on to be playable. It is treated as part of the program.

## Obtaining the source

The deployed client is built from this repository. `webbuild/dist/` is committed
alongside the sources it was built from, so any deployed commit is also the
source for that deployment.

A source link is exposed from the running application via the GitHub link in the
page toolbar.

## Reproducing the build

```bash
git clone <this repository>
cd armagetronad
./webbuild/build-libxml2.sh     # once, cross-compiles the one external dependency
./webbuild/build.sh             # emits webbuild/dist/
```

Toolchain the published artifacts were produced with:

| Tool | Version |
| --- | --- |
| Emscripten | 6.0.2 (`7a2d97d627ff4945eae28847ce0387ac52b92c09`) |
| libxml2 | 2.12.10, configured `--without-http` |
| SDL 1.2, SDL_image, SDL_mixer, libpng | Emscripten ports (`-sUSE_SDL=1` and friends) |

`webbuild/build.sh` stamps a build version into the generated HTML and asset
URLs. That string identifies which build a given deployment is serving.

## Separate services

Accounts, billing, matchmaking, rankings, the server directory, the browser
relay, hosted game servers, and deployment configuration are developed
separately, are not derived from Armagetron Advanced, and are not covered by the
GPL. They live in a different repository and communicate with the client over
network APIs.

Keeping that boundary real means the client must not be linked against, or
compiled together with, any of that code, and the platform must not embed GPL
client code. The client talking to a proprietary service over HTTP or a
WebSocket is an arm's-length relationship; copying code across the line is not.

No technical restriction should be added that stops a user exercising their GPL
rights over the client they received.

## Third-party assets

The GPL on the source code does not by itself settle the licensing of every
bundled model, texture, sound, font or map, and their provenance has **not** yet
been audited. An asset inventory recording author, source, license and
commercial-use permission for each file under `webbuild/data/` is outstanding
work, and needs to be completed before any commercial launch.

"Armagetron Advanced" is the upstream project's name. Whether it may be used to
describe a commercial hosted service, and whether a distinct product name is
more appropriate, is an open question flagged for review.
