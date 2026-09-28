# Tekken 6 Recomp

This is the isolated public-source project for the ReXGlue recompilation. The
working goal is that a user supplies a compatible Xbox 360 disc image and the
project extracts it locally, generates PPC code, builds the patched runtime,
and launches it against the extracted game files.

**The ISO-to-play workflow is not complete yet.** This repository intentionally
does not contain a game ISO, extracted game data, generated PPC code, save data,
or a prebuilt game executable. A matching user-provided image will be required.

## Project patches

`patches/rexglue/` holds the project-owned SDK changes, applied to the pinned
upstream ReXGlue revision. The series includes the runtime compatibility,
rendering diagnostics, guest access-violation context, and Windows window-frame
changes used by the working build. Run `python tools/apply_rexglue_patches.py`
after initializing the SDK submodule and before configuring CMake.

The game's missing thunk/helper entrypoints are kept in `tekken6_config.toml`
so code generation can recreate them from the user's XEX. Generated output
belongs under `generated/default/` and is ignored by Git.

## Current layout

- `src/`: project-owned ReXGlue app and runtime hooks.
- `tekken6_config.toml`, `tekken6_manifest.toml`: code-generation settings.
- `tools/xdvdfs/`: ISO filesystem reader and extractor.
- `patches/rexglue/`: SDK patch series against the pinned SDK base.
- `third_party/rexglue-sdk/`: upstream SDK submodule.

There is no release license yet. Third-party and game-derived material must be
reviewed before public release.
