# ReXGlue patch series

Base: `rexglue-sdk` tag `nightly-20260904-0c7b01a0`
(`0c7b01a0ac0479801757507d80533f662fa0815d`).

Apply these patches in numeric order with
`python tools/apply_rexglue_patches.py` from the project root:

1. Runtime compatibility fixes used by the game.
2. D3D12 draw-outcome diagnostics.
3. Guest PPC context on access violations.
4. Windowed Windows title bar and frame.

The patches are project source changes, not generated game code. Keep the SDK
base pinned; the patch tool refuses a different revision or a partially patched
checkout.
