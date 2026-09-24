/* 32-bit preload. The game reads the stick itself, so the presenter
 * curve never reached it. At 85% of full tilt, report the edge. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SDL_JOYAXISMOTION 0x600u
#define SDL_CONTROLLERAXISMOTION 0x650u

struct axis_ev {
    uint32_t type;
    uint32_t timestamp;
    int32_t which;
    uint8_t axis;
    uint8_t p1, p2, p3;
    int16_t value;
};

static int16_t snap16(int16_t v)
{
    int mag = v < 0 ? -(int)v : (int)v;
    if (mag > 32767)
        mag = 32767;
    if ((mag * 100) / 32767 >= 85)
        return v < 0 ? (int16_t)-32767 : (int16_t)32767;
    return v;
}

static int swap_l2r2;
static uint8_t (*real_joy_button)(void *, int);

static void *joy_from_id(int32_t which)
{
    void *(*from_id)(int32_t) = dlsym(RTLD_NEXT, "SDL_JoystickFromInstanceID");
    if (!from_id)
        return NULL;
    return from_id(which);
}

static int raw_down(void *joy, int button)
{
    if (!swap_l2r2 || !joy || !real_joy_button)
        return 0;
    return real_joy_button(joy, button) ? 1 : 0;
}

static void snap_ev(void *event)
{
    struct axis_ev *e = event;
    if (!e)
        return;
    if ((e->type == SDL_JOYAXISMOTION || e->type == SDL_CONTROLLERAXISMOTION) &&
        e->axis <= 1)
        e->value = snap16(e->value);
    if (!swap_l2r2)
        return;
    /* Knulli map: L2=b6, R2=b7. Send them as the shoulder buttons the
     * game already uses for aim and fire, including the release. */
    if ((e->type == 0x603u || e->type == 0x604u) &&
        (e->axis == 6 || e->axis == 7)) {
        int down = e->type == 0x603u;
        e->type = down ? 0x651u : 0x652u;
        e->axis = (uint8_t)(e->axis == 6 ? 9 : 10);
        e->p1 = (uint8_t)(down ? 1 : 0);
        return;
    }
    /* Same buttons when SDL already calls them lefttrigger/righttrigger. */
    if (e->type == SDL_CONTROLLERAXISMOTION && (e->axis == 4 || e->axis == 5)) {
        int down = e->value > 8000;
        e->type = down ? 0x651u : 0x652u;
        e->axis = (uint8_t)(e->axis == 4 ? 9 : 10);
        e->p1 = (uint8_t)(down ? 1 : 0);
    }
    /* R2 is also reported as Start. Drop that so it fires without pausing.
     * The real Start button is b9, so it still pauses. */
    if ((e->type == 0x651u || e->type == 0x652u) && e->axis == 6 &&
        raw_down(joy_from_id(e->which), 7))
        e->axis = 255;
}

__attribute__((constructor)) static void stick_snap_init(void)
{
    const char *s = getenv("MASSEFFECT_SWAP_L2R2");
    swap_l2r2 = s && s[0] == '1';
    real_joy_button = dlsym(RTLD_NEXT, "SDL_JoystickGetButton");
    fprintf(stderr, "sticksnap: left stick >=85 -> 100%s\n",
            swap_l2r2 ? " L2/R2=aim/fire" : "");
}

int16_t SDL_GameControllerGetAxis(void *controller, int axis)
{
    static int16_t (*real)(void *, int);
    int16_t v;
    if (!real)
        real = dlsym(RTLD_NEXT, "SDL_GameControllerGetAxis");
    if (!real)
        return 0;
    v = real(controller, axis);
    if (axis == 0 || axis == 1)
        v = snap16(v);
    if (swap_l2r2 && axis == 5)
        return 0;
    return v;
}

uint8_t SDL_JoystickGetButton(void *joystick, int button)
{
    if (!real_joy_button)
        real_joy_button = dlsym(RTLD_NEXT, "SDL_JoystickGetButton");
    if (!real_joy_button)
        return 0;
    if (swap_l2r2 && button == 7)
        return 0;
    return real_joy_button(joystick, button);
}

uint8_t SDL_GameControllerGetButton(void *controller, int button)
{
    static uint8_t (*real)(void *, int);
    void *(*joy_of)(void *) = dlsym(RTLD_NEXT, "SDL_GameControllerGetJoystick");
    void *joy;
    if (!real)
        real = dlsym(RTLD_NEXT, "SDL_GameControllerGetButton");
    if (!real)
        return 0;
    if (!swap_l2r2 || !joy_of)
        return real(controller, button);
    joy = joy_of(controller);
    if (button == 6 && raw_down(joy, 7))
        return 0;
    if (button == 9)
        return (uint8_t)(real(controller, 9) || raw_down(joy, 6));
    if (button == 10)
        return (uint8_t)(real(controller, 10) || raw_down(joy, 7));
    return real(controller, button);
}

int16_t SDL_JoystickGetAxis(void *joystick, int axis)
{
    static int16_t (*real)(void *, int);
    int16_t v;
    if (!real)
        real = dlsym(RTLD_NEXT, "SDL_JoystickGetAxis");
    if (!real)
        return 0;
    v = real(joystick, axis);
    if (axis == 0 || axis == 1)
        v = snap16(v);
    return v;
}

int SDL_PollEvent(void *event)
{
    static int (*real)(void *);
    int r;
    if (!real)
        real = dlsym(RTLD_NEXT, "SDL_PollEvent");
    if (!real)
        return 0;
    r = real(event);
    if (r)
        snap_ev(event);
    return r;
}

int SDL_WaitEvent(void *event)
{
    static int (*real)(void *);
    int r;
    if (!real)
        real = dlsym(RTLD_NEXT, "SDL_WaitEvent");
    if (!real)
        return 0;
    r = real(event);
    if (r)
        snap_ev(event);
    return r;
}

int SDL_PeepEvents(void *events, int numevents, int action, uint32_t min_type,
                   uint32_t max_type)
{
    static int (*real)(void *, int, int, uint32_t, uint32_t);
    int n, i;
    if (!real)
        real = dlsym(RTLD_NEXT, "SDL_PeepEvents");
    if (!real)
        return 0;
    n = real(events, numevents, action, min_type, max_type);
    if (n > 0 && action != 2) {
        unsigned char *p = events;
        for (i = 0; i < n; ++i)
            snap_ev(p + (size_t)i * 56u);
    }
    return n;
}
