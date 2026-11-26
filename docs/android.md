# Android

A game that `scripts/port.sh` translated can also be built as an Android app (an APK) for a phone or tablet. The app is the
same window runner as `ps2run` on the PC (`host/main.cpp`, `host/window.cpp`, ...), started by SDL's Java activity, with
the translated game and the runtime compiled for ARM64 by the Android NDK. Pictures go through wgpu-native's Vulkan backend.

Like `build/native-<name>/ps2run`, the APK holds the game's own code translated to C++: build it from your own disc for
your own phone, and do not share it.

## What you need

- the game ported on the PC first: `bash scripts/port.sh <name> <image> --native-only` (the Android build uses
  `generated/<name>`; the exploring runs on the PC decide which code and vector programs are compiled in)
- Android Studio's SDK with NDK 27.2.12479018 and CMake 3.22.1 (SDK Manager), and a JDK 17 or newer
- wgpu-native for Android: `sh scripts/fetch_wgpu_android.sh` (aarch64 for phones, x86_64 for the emulator)

## Build

```
cd android
./gradlew assembleRelease -Pps2Game=<name> -Pps2Title="<title>"
```

The APK is `android/app/build/outputs/apk/release/app-release.apk`, signed with the debug key (fine for installing it
yourself; `-Pps2Keystore=... -Pps2KeyAlias=... -Pps2KeyPassword=...` signs it with your own). `-Pps2Abis=arm64-v8a,x86_64`
adds the Android emulator. The game code is always compiled optimised and with `-ffp-contract=off` (ARM always has FMA,
which would change the vector units' results: scripts/cxx.sh).

## On the phone

1. `adb install -r app-release.apk`
2. The disc image: start the app once, then either choose the image with the file picker it shows (the app keeps the
   permission), or copy it into the app's folder:
   `adb push game.iso /sdcard/Android/data/ps2native.<name>/files/<name>.iso`
3. The same folder holds the memory card (`<name>.ps2`), `keys.ini`, quick saves, reports and `log.txt`; the log also
   goes to logcat: `adb logcat -s ps2native`. Every 5 seconds a `[speed]` line gives the frames per second. A new memory
   card starts formatted and empty (on every platform), so the game saves on it without asking to format it first
   (`PS2_MEMCARD_BLANK=1` gives the unformatted card of a new console card instead).

On the screen are touch controls, a PS2 controller (host/touch.cpp; the model is OpenTricky's on-screen pad): a stick appears
where the left thumb lands on the left half (pushed far it also presses the d-pad, for menus that read only that), triangle
circle cross square on the right, L1 L2 and R1 R2 in the top corners, SELECT and START at the top and beside them a button
that opens the quick menu (save, load, look; a tap on an entry chooses it, a tap beside them goes back). Every press lasts at
least 200 ms, since a game's menus may miss a quicker tap. A game controller (Bluetooth or USB, any controller SDL knows) or a
keyboard (`keys.ini`) works too; the touch controls hide while one is used and come back at the next touch. On a computer
`PS2_TOUCH=1` shows them, with the left mouse button as the finger.

The picture look starts as "crisp + smooth edges" on Android: the sharp looks cost the phone's graphics chip too much
(on a Mali-G68 the THPS4 menu ran at ~30 fps with "sharp + smooth edges", 50 with the crisp or soft looks).

For tests, the launcher takes extras: `args` (more options) and `env` (PS2_ variables), for example
`adb shell "am start -n ps2native.<name>/ps2native.app.LauncherActivity -e env 'PS2_SCALE=2'"`.

## Profiling

The app is `profileable` (AndroidManifest.xml), so the phone's own sampler sees the release build over adb, without root:

```
adb shell "simpleperf record --app ps2native.<name> -f 2000 --duration 20 -o /data/local/tmp/perf.data"
adb pull /data/local/tmp/perf.data
python <ndk>/simpleperf/binary_cache_builder.py -i perf.data -lib <dir with the unstripped libmain.so>
<ndk>/simpleperf/bin/<host>/simpleperf report -i perf.data --symfs binary_cache --sort comm,symbol
```

The unstripped library is `android/app/build/intermediates/cxx/Release/*/obj/arm64-v8a/libmain.so`; `--sort vaddr_in_file` with
`llvm-addr2line -i` on it finds the hot lines of the translated code. In THPS4 that showed the game waiting for a flag in
a loop of a few instructions for two thirds of the emulation thread, which the waiting-loop skip now passes over
(runtime/src/hw.cpp, spin_turn).

## What is different from the PC

- No compiler on the phone: code or vector programs the PC build did not see are not translated while playing. Vector
  programs run in the interpreter; game code the build does not have stops the game, as in the browser build. Let the
  exploring runs on the PC go far, or play on the PC first and port again (`--no-explore`), before building the APK.
- When the app goes to the background Android takes the window away; SDL pauses the app, and on return the picture goes
  to the new window (`ps2::detach_window`, host/window.cpp).
- The Android emulator's Vulkan driver (gfxstream, `vulkan.ranchu.so`) crashes on the first GS work. In the emulator use
  the software renderer: `-e env 'PS2_GS=cpu PS2_SDL_RENDERER=1'`.
