# Third-party notices

OpenDrummer is licensed under the GNU Affero General Public License v3.0 (see
`LICENSE`). It builds on, bundles, or works with the following.

## Downloaded at build time (not in this repository)

**JUCE 9.0.2** — https://github.com/juce-framework/JUCE
Used under the GNU AGPLv3. © Raw Material Software Limited.

**sfizz 1.2.3** — https://github.com/sfztools/sfizz
BSD 2-Clause License. © 2021-2023 sfizz contributors. sfizz and its submodules
carry their own licence files, which are downloaded alongside it.

## Included in this repository

**Press Start 2P** — `Resources/fonts/PressStart2P-Regular.ttf`
© 2012 The Press Start 2P Project Authors, with Reserved Font Name "Press Start 2P".
Licensed under the SIL Open Font License 1.1 (`Resources/fonts/OFL.txt`).
`Source/PixelFontData.h` is an 8×8 bitmap conversion of this font and is
distributed under the same licence. It is not offered under the Reserved Font Name.

## Recorded drum kits (not included — fetched by `fetch-kits.ps1`)

**DRS Kit** — https://github.com/sfzinstruments/DrumGizmo.DRSKit
By Lars Muldjord, Bent Bisballe Nyeng (DrumGizmo) and Jes Eiler (DRSDrums).
Creative Commons Attribution 4.0. SFZ mapping by kinwie.

**Big Rusty Drums** — https://github.com/sfzinstruments/karoryfer.big-rusty-drums
By Karoryfer Samples. Creative Commons Zero (public domain dedication).

**MuldjordKit** — https://github.com/sfzinstruments/DrumGizmo.MuldjordKit
By Lars Muldjord (DrumGizmo). Creative Commons Attribution 4.0. SFZ mapping by kinwie.

OpenDrummer shows each kit's credit on screen while that kit is loaded.

## Not included, and never downloaded

**Steinberg ASIO SDK.** Its licence does not permit redistribution. To build
with ASIO support, download it from Steinberg and set `ASIO_SDK_DIR`.
ASIO is a trademark and software of Steinberg Media Technologies GmbH.
