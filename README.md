# glOSports-gtactw — GTA Chinatown Wars, native, on the GA36-MB

Grand Theft Auto: Chinatown Wars through Rockstar's Android build (War Drum
Studios, 2014): a loader (`loader/`) maps the game's own `libCTW.so`
(armeabi-v7a), resolves its imports to goodluckOS's glibc, ARM's Mali GLES 2.0
driver (from the folder's `glos/`, see `goodluckOS-mali-driver`), OpenAL Soft and
SDL's pad, answers its Java calls through a fake JNI, and runs it, the way
[gtactw_vita](https://github.com/TheOfficialFloW/gtactw_vita) (MIT) does on
the Vita. The loader core is the one in `glOSports-gtasa`, carried over from
`glOSports-maxpayne`. The console stays stock goodluckOS.

The mobile game wants a touchscreen for its minigames and the PDA, which the
GA36-MB doesn't have: the right stick moves a pointer (a cross on screen)
and R3 is the finger, press to tap, hold to drag. The top-down game has no
camera on the right stick, so nothing is lost; while the pointer is on, the
game doesn't see the right stick or R3.

Status: runs on the GA36-MB, playable; 4:3 is the game's own
(its layout is chosen by the screen's shape, and 640x480 is below its 1.34
widescreen threshold), 30 fps. Tested with the Vita release's
`data/gtactw` (1.04, libCTW.so md5 e928c8fb).

Fixes found on the first runs: `NVThreadGetCurrentJNIEnv` stays the game's
(its thread wrapper reads the env back from its own pthread key; the
hook crashed the first thread), and `alBufferDataStatic`, which OpenAL Soft
lacks and the game asks for on every buffer, is answered with
`alBufferData`.

## Game files

From your own install of the Android game (package
`com.rockstargames.gtactw`), into `/home/player/ports/gtactw/game/`:

| From | Version | What goes into `game/` |
|---|---|---|
| the `.apk` | **1.04** | `lib/armeabi-v7a/libCTW.so`, and the contents of `assets/` |
| `main.4.com.rockstargames.gtactw.obb` | 1.04 | its contents (it is a zip) |

The OBB is in `/sdcard/Android/obb/com.rockstargames.gtactw/` on the phone.

## The game folder on the console (`/home/player/ports/gtactw/`)

| | |
|---|---|
| `gtactw` | the loader (DT_RPATH `$ORIGIN/lib`) |
| `lib/` | OpenAL Soft 1.19 and its dependencies (sndio, bsd, md) |
| `alsoft.conf` | OpenAL reads it from the game folder by itself (ALSA) |
| `game/` | the game files above; saves land here too |
| `glos/` | the ARM driver set from `goodluckOS-mali-driver` |
| `start.sh` | pad mapping and settings, then `exec ./glos/glos-gpu run mali ./gtactw` |

Launcher entry: `gtactw.puppy`, its icon a 3:2 crop of a banner. Log: `/dev/shm/gtactw.log` (a crash prints the
PC and LR as `libCTW.so+offset`).

## Settings (`start.sh`)

| Variable | Default | |
|---|---|---|
| `GAME_STICK_SCALE`, `GAME_STICK_DEADZONE` | 1.5, 0.12 | stick shaping (radial dead zone after scaling) |
| `GAME_FPS_COUNTER` | 0 (1 in `start.sh`) | frames per second, bottom right |
| `GAME_LOG_FPS` | 0 | fps, swap time, swaps and flips into the log |
| `GAME_VSYNC` | 1 | swap interval and the wait for the previous flip (no tearing) |
| `GAME_AUDIO_RATE` | 44100 | OpenAL mixing rate (the game asks for 22050) |
| `GAME_LANGUAGE` | 0 | 0 English, 1 French, 2 German, 3 Italian, 4 Spanish, 5 Japanese |
| `CTW_TOUCH_POINTER` | 1 | right stick moves a pointer, R3 touches |
| `CTW_POINTER_SPEED` | 450 | pointer speed at full tilt, pixels a second |

## The videos

On Android a Java MediaPlayer shows the game's videos over it. Both play
here as they come in the OBB (H.264 Main, AAC), no conversion:
`CTW_iPhone_intro.mp4` (1024x512), Huang's arrival and ambush under the
credits, started by the first mission (`kena01::cIntroDrive`) through
`OS_MoviePlayinWindow`, above a strip the mission goes on drawing in; and the
end credits, `CTW_iPhone_outro.mp4` (1024x768, 186 s), started by
`cEndGame::InitDelay` through `OS_MoviePlay` while the game runs on and polls
`OS_MovieIsPlaying` (`UpdateMovieCallback`).

The player (`loader/movie.c`, FFmpeg's MP4 demuxer and H.264 / AAC decoders
linked in statically) shows it as a cutscene: the whole screen, centred, and
the game waits until it ends (`movie_play_wait`: the calling thread holds the
screen, draws the pictures and reads the pad; with the mission running on
under it, it repeated). Its own GL context leaves the game's GL state alone;
the sound goes through the folder's OpenAL; A or Start skips. The end
credits play on their own (`movie_play`) and replace the game's frame at each
swap. Measured: the intro's 143 s at 30 fps (converted to 640x320 then), the
end credits unconverted, 5,563 frames, none dropped.

`GAME_MOVIE_TEST=<file>@<seconds>` (test runs) makes the game's
`OS_MoviePlay` call for that file that many seconds after the first frame:
how the end credits were checked without finishing the game.

## From gtactw_vita, what carries over and what doesn't

- **Carried over:** the War Drum hooks (threads, `OS_ThreadWait`, screen,
  system init), the Java methods (window, storage, device, pad), touch through
  `AND_TouchEvent`, here driven by the pointer instead of the Vita's screen.
- **Left out:** the Vita's rendering work (its Cg shader generator, GL_FIXED
  vertex conversion for vitaGL): Mali-400 runs the game's GLSL ES shaders and
  GL_FIXED vertices as Android does. The Vita's touchscreen L3/R3 zones.
- **Different:** threads are real pthreads; `sem_trywait` is the real one
  (the Vita always failed it); EGL is ARM's fbdev EGL with the tear-free flip
  wait from `glOSports-maxpayne`.

## The loader (`loader/`)

| File | |
|---|---|
| `so_util.c` | maps the .so, relocates, finds symbols through its hash table, patches code (`hook_addr`: Thumb or ARM by the address's low bit) |
| `imports.c` | bionic → glibc (softfp wrappers for every float import, bionic-sized pthread / sem / stat / FILE / jmp_buf); every `gl*` / `egl*` resolves to ARM's driver, through wrappers where floats pass by value |
| `jni.c`, `jni_names.inc` | the fake Java VM and JNIEnv: all 233 slots, unknown ones log their name |
| `android.c` | War Drum's threads and screen, and the Java methods the game calls (window, storage, device, pad) |
| `egl.c` | GLES 2.0 on EGL on `/dev/fb0`, tear-free flips, FPS counter |
| `openal.c` | every `al*` / `alc*` entry point of the game's own OpenAL Soft jumps to the folder's (found by walking the .so's symbols) |
| `pad.c` | SDL2 game controller, stick shaping |
| `movie.c` | the story video: FFmpeg decoding, its own GL context, OpenAL sound |
| `game.c` | what is GTA Chinatown Wars's: hooks, the touch pointer |

## Build

```
./build.sh      # loader/ -> build/gtactw (Docker, Debian bookworm armhf)
./package.sh    # -> out/ports/gtactw/ and out/.local/share/applications/gtactw.puppy
```

`build.sh` runs `tools/build-ffmpeg.sh` first when `build/ffmpeg/` is
missing: a static FFmpeg 6.1.2 (MP4 demuxer, H.264 and AAC decoders, NEON),
from ffmpeg.org's `ffmpeg-6.1.2.tar.xz` (sha256 `3b624649…`) unpacked in
`upstream/ffmpeg-6.1.2`.

Test tools are shared: `../goodluckOS-tools/testing-sync.sh gtactw` puts them
on the console (`glos/testing/`), `testing-clean.sh gtactw` takes them off,
`shot.sh gtactw file.png` takes a screenshot (see `goodluckOS-tools`).

## Reference loader (`upstream/`, not committed)

| Folder | Source | Commit |
|---|---|---|
| `upstream/gtactw_vita` | [TheOfficialFloW/gtactw_vita](https://github.com/TheOfficialFloW/gtactw_vita) (MIT) | `90809be` |

## Third-party components

Code carried over from the Vita loaders it names keeps their MIT notices;
those, FFmpeg (LGPL-2.1, linked statically for the videos), OpenAL Soft and
the other bundled libraries, and ARM's Mali driver are listed with their
licences in `licenses/`, which `package.sh` copies into the game folder. The
game itself is not included.
