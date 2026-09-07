Orion Master (android-1oom)
===========================

This repository is a **fork of [1oom](https://github.com/1oom-fork/1oom)** for
Android. The engine in `src/` is the same GPLv2 1oom code, kept at the
upstream path so the attribution trail and later merges stay obvious.

What this fork adds is a native Android application: a Java/Kotlin shell,
JNI, and Android `hw` / `os` / audio backends so the classic 1993 UI can
run as an ARM/x86_64 shared library and be played on a touch screen.
It is **not** the older DOSBox-on-Android recipe, and it does **not**
include Master of Orion data files.

1oom itself is Free Software (GPLv2), see [COPYING](COPYING).
Direct use of AI-generated code is prohibited in upstream 1oom.


1. You must own the original game
=================================

To use this software you must legally own an original copy of
Master of Orion (v1.3). 1oom requires that version's LBX files.
This project will not ship those files in git, in an APK, or on
F-Droid.

On first launch, pick the folder that contains `fonts.lbx` (a full
v1.3 set also needs `V11.LBX`). Details are in
[doc/usage_android.txt](doc/usage_android.txt).


2. Why F-Droid, not Google Play
===============================

The intended binary distribution is [F-Droid](https://f-droid.org/).

Google Play is a poor fit: the APK is not a complete game until the
user copies in copyrighted LBX files they already own. Play also
restricts the broad storage permission this first version may use as
a fallback. F-Droid is the usual home for GPLv2 engines that expect
the player to supply original data (the same model as ScummVM or
OpenMW).

This repo does not yet contain an `fdroiddata` metadata file; that
lives in F-Droid's data repository when the app is submitted.
Store listing text for that submission is under
[fastlane/metadata/android/](fastlane/metadata/android/).


3. Layout
=========

Do **not** rename the root `src/` directory. That is the upstream 1oom
engine (`src/game`, `src/ui/classic`, `src/hw`, `src/os`, …). Renaming
it would break autotools, obscure `git blame` / merges from
`1oom-fork/1oom`, and hide that this is the same code.

Android Studio's `app/src/` tree is a different, Gradle-conventional
source set sitting beside it:

    src/                 1oom engine (upstream path, GPLv2)
    app/src/main/cpp/    JNI + Android hw/os/audio backends
    app/src/main/java/   Kotlin activity, touch view, LBX import
    app/src/main/res/    Android resources
    doc/                 Upstream 1oom documentation
    doc/usage_android.txt
    fastlane/            F-Droid / store descriptions

The Android CMake build compiles `src/*.c`, `src/game/*.c`, and
`src/ui/classic/*.c` and links them with the files in
`app/src/main/cpp/`.


4. Building the Android app
===========================

See [COMPILING](COMPILING) section 5. Short version, from the repo root:

    ./gradlew :app:assembleDebug

You still need a v1.3 LBX set on the device. Desktop SDL/Allegro
builds of 1oom from this tree remain possible; follow the rest of
[COMPILING](COMPILING).


5. Desktop 1oom (upstream)
==========================

1oom aims to accurately reproduce the original DOS version of
Master of Orion (1993) on modern computers.

Installation, configuration, and the various `1oom_*` tools are
unchanged. See:

- [doc/usage_common.txt](doc/usage_common.txt)
- [doc/usage_classic.txt](doc/usage_classic.txt)
- [HACKING](HACKING)
- [PHILOSOPHY](PHILOSOPHY)

Older saved games may not work in newer 1oom versions. Read
[CHANGES](CHANGES) for breaking changes.

5.1 Libraries (desktop)
-----------------------

- SDL (libsdl1.2 or libsdl2): http://www.libsdl.org
- SDL_mixer: http://www.libsdl.org/projects/SDL_mixer/
- libsamplerate (recommended): http://www.mega-nerd.com/libsamplerate/


6. Acknowledgements
===================

Most of the credit for this software belongs to the programmer who
authored [1oom v1.0](https://kilgoretroutmaskreplicant.gitlab.io/plain-html)
under the pseudonym Kilgore Trout Mask Replicant. Thank you for
creating this and publishing it free and open source!

Thanks to MyName aka Duzh87_54MSU for restoring 1oom from its broken
state and for creating
[1oom v2.0 (vanilla)](https://sourcecraft.dev/fork1oom/1oom).

The original game Master of Orion was developed by Simtex Software
and published 1993 by MicroProse. Thanks for the great game!

Thanks to Alan Emrich and Tom Hughes for documenting the game mechanics
and AI decision making in the official strategy guide.

Ideas and text snippets have been taken from kyrub's unofficial patch
1.40m Readme. Thanks for the patch!

Thanks to [shikadi.net](http://www.shikadi.net) for documenting the
[music format](https://moddingwiki.shikadi.net/wiki/XMI_Format).

Thanks to CivFanatics forum user sargon0 for
[partial save game format info](http://forums.civfanatics.com/threads/moo-save-file-layout.275055/).

Special thanks to Zachary Kline (BlindGuyNW) for improving the text
version of the game.

Thanks to those who contributed code, ideas or bug reports.

Some code has been pilfered from Chocolate Doom and VICE.

The files [HACKING](HACKING) and [PHILOSOPHY](PHILOSOPHY) are based on
Chocolate Doom.

Mirrors of upstream 1oom are listed in [MIRRORS.md](MIRRORS.md).
