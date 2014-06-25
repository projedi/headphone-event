/*
  This work is based on the pactl utility from PulseAudio.
  Below follows the original header.
*/
/***

  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pulse/pulseaudio.h>

#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#define UNUSED(x) x __attribute__((unused))

#define to_log(...) fprintf(stderr, __VA_ARGS__);

#define fail_log(ctx, name, cleanup) {\
	to_log(name " failed: %s", pa_strerror(pa_context_errno(ctx)));\
	goto cleanup;\
}

struct state_t {
	pa_mainloop_api* mainloop_api;
	Display* display;
};

static void graceful_exit(struct state_t* state, int code, char const* format, ...) {
	assert(state->mainloop_api);
	va_list args;
	va_start(args, format);
	to_log(format, args);
	va_end(args);
	state->mainloop_api->quit(state->mainloop_api, code);
}

static void press_key(struct state_t* data, KeySym keysym) {
	KeyCode code = XKeysymToKeycode(data->display, keysym);

	XTestFakeKeyEvent(data->display, code, True, CurrentTime);
	XTestFakeKeyEvent(data->display, code, False, CurrentTime);

	XFlush(data->display);
}

static void headphones_plugged(struct state_t* data) {
	KeySym keysym = XF86XK_Launch1;
	press_key(data, keysym);
}

static void headphones_unplugged(struct state_t* data) {
	// Can also do:
	// KeySym keysym = XStringToKeysym("XF86MonBrightnessDown");
	KeySym keysym = XF86XK_Launch2;
	press_key(data, keysym);
}

static void get_card_info_callback(pa_context* ctx, pa_card_info const* card, int is_last, void *data) {
	if(is_last < 0) {
		to_log("Failed to get card information: %s", pa_strerror(pa_context_errno(ctx)));
		return;
	}
	if(is_last) return;
	assert(card);
	if (card->ports) {
		for(pa_card_port_info** p = card->ports; *p; ++p) {
			if(!strcmp((*p)->name, "analog-output-headphones")) {
				switch((*p)->available) {
					case PA_PORT_AVAILABLE_YES: headphones_plugged(data); break;
					case PA_PORT_AVAILABLE_NO: headphones_unplugged(data); break;
					default: break;
				}
			}
		}
	}
}

static void context_subscribe_callback(pa_context *ctx, pa_subscription_event_type_t type,
		uint32_t card_idx, void *data) {
	assert(ctx);
	if((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE)
		pa_operation_unref(pa_context_get_card_info_by_index(ctx, card_idx, get_card_info_callback, data));
}

static void context_state_callback(pa_context* ctx, void* data) {
	assert(ctx);
	switch (pa_context_get_state(ctx)) {
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			break;
		case PA_CONTEXT_READY:
			pa_context_set_subscribe_callback(ctx, context_subscribe_callback, data);
			pa_operation_unref(pa_context_subscribe(ctx, PA_SUBSCRIPTION_MASK_CARD, NULL, NULL));
			break;
		case PA_CONTEXT_TERMINATED:
			graceful_exit(data, 0, "Terminating.\n");
			break;
		default:
			graceful_exit(data, 1, "Connection error: %s\n", pa_strerror(pa_context_errno(ctx)));
	}
}

int main() {
	static struct state_t state;
	Display* display = NULL;
	pa_mainloop* mainloop = NULL;
	pa_context* context = NULL;
	int code = 1;

	if(!(display = XOpenDisplay(NULL))) {
		fprintf(stderr, "Error: Can't open display\n");
		goto cleanup;
	}

	state.display = display;

	if(!(mainloop = pa_mainloop_new()))
		fail_log(context, "pa_mainloop_new", cleanup);

	state.mainloop_api = pa_mainloop_get_api(mainloop);

	if(!(context = pa_context_new(state.mainloop_api, NULL)))
		fail_log(context, "pa_context_new", cleanup);

	pa_context_set_state_callback(context, context_state_callback, &state);
	if(pa_context_connect(context, NULL, 0, NULL) < 0)
		fail_log(context, "pa_context_connect", cleanup);

	if(pa_mainloop_run(mainloop, &code) < 0)
		fail_log(context, "pa_mainloop_run", cleanup);
cleanup:
	if(context)
		pa_context_unref(context);

	if(mainloop)
		pa_mainloop_free(mainloop);

	if(display)
		XCloseDisplay(display);

	return code;
}
