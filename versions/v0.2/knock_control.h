#ifndef KNOCK_CONTROL_H
#define KNOCK_CONTROL_H

/* Portable C89 state machine, also exercised by tests/test_knock.c.
   STM8 unsigned short is 16 bits; all intervals fit within 65535 ms. */
#define KNOCK_MIN_GAP_MS       1000U
#define KNOCK_WINDOW_MS        10000U
#define KNOCK_POWER_MS         60000U
#define KNOCK_LOW_STABLE_MS    30U

#define KNOCK_WAIT     0
#define KNOCK_COLLECT  1
#define KNOCK_POWER    2
#define KNOCK_TEST     3

typedef struct {
    unsigned char state;
    unsigned char count;
    unsigned char armed;
    unsigned char low_ms;
    unsigned short now;
    unsigned short first;
    unsigned short last;
    unsigned short powered_at;
} KnockControl;

static unsigned short knock_elapsed(unsigned short now, unsigned short then)
{
    return (unsigned short)(now - then);
}

static void knock_reset(KnockControl *k, unsigned char state)
{
    k->state = state;
    k->count = 0;
    k->armed = 0;
    k->low_ms = 0;
}

static void knock_init(KnockControl *k, unsigned char test)
{
    k->now = 0;
    k->first = k->last = k->powered_at = 0;
    knock_reset(k, test ? KNOCK_TEST : KNOCK_WAIT);
}

static void knock_edge(KnockControl *k)
{
    if (k->state == KNOCK_WAIT) {
        k->state = KNOCK_COLLECT;
        k->count = 1;
        k->first = k->last = k->now;
        k->armed = 0;
        k->low_ms = 0;
    } else if (k->state == KNOCK_COLLECT && k->armed &&
               knock_elapsed(k->now, k->last) >= KNOCK_MIN_GAP_MS &&
               knock_elapsed(k->now, k->first) <= KNOCK_WINDOW_MS) {
        ++k->count;
        k->last = k->now;
        k->armed = 0;
        k->low_ms = 0;
        if (k->count == 3) {
            k->state = KNOCK_POWER;
            k->powered_at = k->now;
        }
    }
}

static void knock_tick(KnockControl *k, unsigned char sensor_high)
{
    ++k->now;
    if (k->state == KNOCK_COLLECT) {
        if (knock_elapsed(k->now, k->first) > KNOCK_WINDOW_MS) {
            knock_reset(k, KNOCK_WAIT);
            return;
        }
        if (sensor_high) k->low_ms = 0;
        else if (k->low_ms < KNOCK_LOW_STABLE_MS) ++k->low_ms;
        if (k->low_ms >= KNOCK_LOW_STABLE_MS &&
            knock_elapsed(k->now, k->last) >= KNOCK_MIN_GAP_MS)
            k->armed = 1;
    } else if (k->state == KNOCK_POWER &&
               knock_elapsed(k->now, k->powered_at) >= KNOCK_POWER_MS) {
        knock_reset(k, KNOCK_WAIT);
    }
}

#endif
