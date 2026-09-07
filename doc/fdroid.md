# F-Droid release checklist

Orion Master is meant for the official F-Droid repository, not Google Play.
F-Droid builds the APK from a public git tag and signs it with their key.

## Already in good shape

- Application ID `com.tallydigital.oomdroid` (do not change this after the first published build)
- GPLv2 (`COPYING`)
- No Google Play Services, Firebase, ads, or analytics
- No `INTERNET` permission
- No prebuilt native libraries; CMake compiles `src/` from this repo
- Gradle wrapper pins a SHA-256
- NDK `27.2.12479018` is pinned in `app/build.gradle.kts`
- Fastlane text under `fastlane/metadata/android/en-US/`
- Store icon and feature graphic under `fastlane/metadata/android/en-US/images/`

## You still need to do

1. Add at least two landscape phone screenshots as:

       fastlane/metadata/android/en-US/images/phoneScreenshots/1.png
       fastlane/metadata/android/en-US/images/phoneScreenshots/2.png

   Suggested shots: the Choose LBX folder screen, and one in-game screen (galaxy or ship design). Do not include LBX files in git.

2. Commit the remaining F-Droid prep (this folder, fastlane images, NOTICE) and push to `origin/master`.

3. Tag the release commit after it is on GitHub:

       git tag -a v1.0.0 -m "Orion Master 1.0.0"
       git push origin v1.0.0

   The tag name must match `versionName` in `app/build.gradle.kts` (`1.0.0`), with or without a `v` prefix.

4. Fork [fdroiddata](https://gitlab.com/fdroid/fdroiddata), copy

       doc/fdroid/com.tallydigital.oomdroid.yml

   to `metadata/com.tallydigital.oomdroid.yml` in that fork. If the tag commit is not exactly `v1.0.0` yet, set `commit:` to the tag or the full hash.

5. Open a merge request titled `New App: com.tallydigital.oomdroid`. GitLab CI will lint and try to build. Watch that pipeline and answer reviewer comments.

Official walkthrough: https://f-droid.org/docs/Submitting_to_F-Droid_Quick_Start_Guide/

## What reviewers will look at

- **LBX files** — none in git or the APK. Same model as OpenMW: engine only, user-owned data.
- **Launcher art** — original to this fork, GPLv2 (see `NOTICE`). Not MicroProse box art.
- **Antonio font** — SIL Open Font License 1.1 (`app/src/main/assets/licenses/Antonio-OFL.txt`).
- **Storage** — no `MANAGE_EXTERNAL_STORAGE`. LBX import uses the SAF folder picker only.
- **AGP 9 / Gradle 9.5 / JDK 21** — current, but if CI cannot resolve the SDK or JDK, the metadata `sudo:` block may need an OpenJDK 21 install.

## After it is listed

Bump `versionCode` and `versionName` together, add `fastlane/metadata/android/en-US/changelogs/<versionCode>.txt`, commit, tag, and push. Auto-update is configured as `Tags`.

F-Droid signs with their key. Switching later to a developer-signed reproducible build means existing users cannot update in place; they would reinstall.
