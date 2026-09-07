# Orion Master (android-1oom)

This repository is a **fork of [1oom](https://github.com/1oom-fork/1oom)** for
Android. The engine in `src/` is the same GPLv2 1oom code, kept at the
upstream path so the attribution trail and later merges stay obvious.

What this fork adds is a native Android application: a Java/Kotlin shell,
JNI, and Android `hw` / `os` / audio backends so the classic 1993 UI can
run as an ARM/x86_64 shared library and be played on a touch screen.
It is **not** the older DOSBox-on-Android recipe, and it does **not**
include Master of Orion data files.

1oom itself is Free Software (GPLv2), see [COPYING](COPYING) and
[NOTICE](NOTICE). Direct use of AI-generated code is prohibited in
upstream 1oom.

This build has several UI changes which make playing MOO on Android possible/pleasant to use.
Trying to use the DOSBOX version is practically unplayable due to click areas.

###The following UI changes were made:
- i. The slider click areas were enlarged. There are two large hotspots around the lower/raise slider buttons to make it easier to change the sliders. The slider direct click area is larger, but the up/down hotspots override the fast select. This seemed to be the most optimal way of fixing the click issue.
- ii. In the ship design screen, the increase/decrease weapon count button click areas are enlarged.
- iii. In the ship design screen, there is an additional "MAX" button to quickly set weapons to max space available.
- iv. On weapons and specials selection, there are up/down buttons on the right hand side to easily traverse the list of available items for shipment.

Note that no functional changes were made: This is the same 1oom as desktop other than the usability mods I implemented to actually be able to play the game.

## 1. You must own the original game

To use this software you must legally own an original copy of
Master of Orion (v1.3). 1oom requires that version's LBX files.
This project will not ship those files in git, in an APK, or on
F-Droid.

On first launch, pick the folder that contains `fonts.lbx` (a full
v1.3 set also needs `V11.LBX`). Details are in
[doc/usage_android.txt](doc/usage_android.txt).

## 2. Building the Android app

See [COMPILING](COMPILING) section 5. Short version, from the repo root:

    ./gradlew :app:assembleDebug

You still need a v1.3 LBX set on the device. Desktop SDL/Allegro
builds of 1oom from this tree remain possible; follow the rest of
[COMPILING](COMPILING).


##3. Desktop 1oom (upstream)

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

## 3. Acknowledgements

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

## 4. F-Droid

This fork is packaged for F-Droid, not Google Play. F-Droid builds
from a public `v*` tag and signs the APK. Store listing files live
under [fastlane/metadata/android/en-US](fastlane/metadata/android/en-US).
The inclusion checklist and a draft `fdroiddata` recipe are in
[doc/fdroid.md](doc/fdroid.md).

## 5. Licenses for Android extras

- Engine and Android shell: [GNU GPL v2](COPYING)
- Antonio UI font: [SIL Open Font License 1.1](app/src/main/assets/licenses/Antonio-OFL.txt)
- Launcher icon and feature graphic: original to this fork, GPLv2 (see [NOTICE](NOTICE))
