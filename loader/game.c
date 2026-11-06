/* game.c -- Grand Theft Auto: Chinatown Wars 1.04 (libCTW.so): hooks, and a
 * pointer for the touch parts. From gtactw_vita (Andy Nguyen, Rinnegatamante;
 * MIT); its Vita-only rendering work (the Cg shader generator, GL_FIXED
 * conversion for vitaGL) is left out: the game's own GLSL ES shaders and
 * GL_FIXED vertices run on Mali-400 as on Android.
 *
 * Touch: the GA36-MB has no touchscreen, and the mobile Chinatown Wars wants
 * one for its minigames and the PDA. The right stick moves a pointer (a
 * cross on screen) and R3 is the finger: press to tap, hold to drag. While
 * CTW_TOUCH_POINTER is on, the game doesn't see the right stick or R3. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "android.h"
#include "so_util.h"
#include "util.h"

const char *game_so_name = "libCTW.so";
const char *const game_required_files[] = { "libCTW.so", NULL };

static int touch_pointer;

static int java_GetGamepadButtonsCTW(const uintptr_t *a, float *f)
{
	int b = java_GetGamepadButtons(a, f);
	return touch_pointer ? b & ~0x2000 : b;
}

static int java_GetGamepadAxisCTW(const uintptr_t *a, float *f)
{
	java_GetGamepadAxis(a, f);
	if (touch_pointer && (a[1] == 2 || a[1] == 3))
		*f = 0.0f;
	return 0;
}

const JavaMethod game_java_methods[] = {
	{ "GetGamepadButtons", java_GetGamepadButtonsCTW },
	{ "GetGamepadAxis", java_GetGamepadAxisCTW },
	COMMON_JAVA_METHODS,
	{ NULL, NULL },
};

/* ---- the pointer ---- */

enum { ACTION_UP = 1, ACTION_DOWN = 2, ACTION_MOVE = 3 };
static int (*AND_TouchEvent)(int action, int pointer, int x, int y);

static float px = -1, py = -1;
static int down, shown;
static double last_t, last_moved;

static double now_s(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

static SOFTFP int OS_MoviePlay(const char *file, int a2, int a3, float a4);

static int ProcessEventsCTW(void)
{
	pad_update();
	const char *test = movie_test_due();
	if (test)
		OS_MoviePlay(test, 0, 0, 1.0f);
	if (!touch_pointer)
		return pad_quit_requested();

	double t = now_s();
	double dt = last_t ? t - last_t : 0;
	last_t = t;
	if (dt > 0.1)
		dt = 0.1;
	int w = egl_width(), h = egl_height();
	if (px < 0) {
		px = w / 2;
		py = h / 2;
	}
	float speed = env_float("CTW_POINTER_SPEED", 450.0f); /* pixels a second at full tilt */
	float sx = pad_axis(2), sy = pad_axis(3);
	if (sx != 0.0f || sy != 0.0f) {
		px += sx * speed * dt;
		py += sy * speed * dt;
		if (px < 0) px = 0;
		if (py < 0) py = 0;
		if (px > w - 1) px = w - 1;
		if (py > h - 1) py = h - 1;
		last_moved = t;
		shown = 1;
		if (down)
			AND_TouchEvent(ACTION_MOVE, 0, (int)px, (int)py);
	}
	int pressed = (pad_buttons() & 0x2000) != 0;
	if (pressed && !down) {
		shown = 1;
		last_moved = t;
		AND_TouchEvent(ACTION_DOWN, 0, (int)px, (int)py);
	} else if (!pressed && down) {
		AND_TouchEvent(ACTION_UP, 0, (int)px, (int)py);
	}
	down = pressed;
	if (shown && !down && t - last_moved > 3.0)
		shown = 0; /* hide after 3 s untouched */
	egl_cursor(shown ? (int)px : -1, (int)py, down);
	return pad_quit_requested();
}

/* videos: on Android a Java MediaPlayer shows them over the game, which
 * waits on AND_IsMoviePlaying; movie.c plays them as they come in the OBB.
 * OS_MoviePlay: the end credits (cEndGame::InitDelay, CTW_iPhone_outro.mp4),
 * which play while the game runs on and replace its frame at each swap. */
static SOFTFP int OS_MoviePlay(const char *file, int a2, int a3, float a4)
{
	movie_play(file);
	return 0;
}

/* the intro (kena01::cIntroDrive::StartMovie) asks for a window above a
 * strip the mission goes on drawing in; here it is a cutscene: the whole
 * screen, centred, and the game waits until it ends or is skipped (the
 * mission ran on and repeated under it otherwise) */
static SOFTFP int OS_MoviePlayinWindow(const char *file, int x, int y, int w, int h,
	int a6, int a7, float a8)
{
	movie_play_wait(file);
	return 0;
}

static int AND_IsMoviePlaying(void) { return movie_playing(); }
static void AND_StopMovie(void) { movie_stop(); }

void patch_game(void)
{
	hook_sym("_Z12OS_MoviePlayPKcbbf", OS_MoviePlay);
	hook_sym("_Z20OS_MoviePlayinWindowPKciiiibbf", OS_MoviePlayinWindow);
	hook_sym("_Z18AND_IsMoviePlayingv", AND_IsMoviePlaying);
	hook_sym("_Z13AND_StopMoviev", AND_StopMovie);
	touch_pointer = env_int("CTW_TOUCH_POINTER", 1);

	/* NVThreadGetCurrentJNIEnv stays the game's: it keeps the env in a
	 * pthread key (glibc's here) that NVThreadSpawnJNIThread's threads read
	 * back; the Vita loader hooks it because its keys are stubs */
	hook_sym("_Z15OS_ThreadLaunchPFjPvES_jPKci16OSThreadPriority", OS_ThreadLaunch);
	hook_sym("_Z13OS_ThreadWaitPv", OS_ThreadWait);
	hook_sym("_Z17OS_ScreenGetWidthv", OS_ScreenGetWidth);
	hook_sym("_Z18OS_ScreenGetHeightv", OS_ScreenGetHeight);
	hook_sym("_Z20AND_SystemInitializev", ret0);

	AND_TouchEvent = (void *)so_find_addr("_Z14AND_TouchEventiiii");
	hook_sym("_Z13ProcessEventsb", ProcessEventsCTW);
}
