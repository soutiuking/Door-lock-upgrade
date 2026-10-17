#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "../knock_control.h"

static void advance(KnockControl *k, unsigned int ms, unsigned char high)
{
    while (ms--) knock_tick(k, high);
}

static void three(KnockControl *k)
{
    knock_edge(k);
    advance(k, 1000, 0);
    knock_edge(k);
    advance(k, 1000, 0);
    knock_edge(k);
    assert(k->state == KNOCK_POWER);
}

int main(void)
{
    KnockControl k;
    unsigned int i;
    assert(USHRT_MAX == 65535U);

    knock_init(&k, 0);
    assert(k.state == KNOCK_WAIT);
    three(&k);
    advance(&k, 59999, 0);
    assert(k.state == KNOCK_POWER);
    knock_tick(&k, 0);
    assert(k.state == KNOCK_WAIT);
    puts("PASS: three knocks and exact 60 s power timeout");

    knock_init(&k, 0);
    knock_edge(&k);
    for (i = 0; i < 999; ++i) {
        knock_tick(&k, (unsigned char)(i & 1));
        knock_edge(&k);
        assert(k.count == 1);
    }
    advance(&k, 30, 0);
    knock_edge(&k);
    assert(k.count == 2);
    puts("PASS: repeated edges within 1 s do not count or extend lockout");

    knock_init(&k, 0);
    knock_edge(&k);
    advance(&k, 999, 0);
    knock_edge(&k);
    assert(k.count == 1);
    knock_tick(&k, 0);
    knock_edge(&k);
    assert(k.count == 2);
    puts("PASS: 999 ms rejected, 1000 ms accepted");

    knock_init(&k, 0);
    knock_edge(&k);
    advance(&k, 1000, 0);
    knock_edge(&k);
    advance(&k, 9000, 0);
    knock_edge(&k);
    assert(k.state == KNOCK_POWER);
    puts("PASS: third knock at exactly 10 s accepted");

    knock_init(&k, 0);
    knock_edge(&k);
    advance(&k, 9000, 0);
    knock_edge(&k);
    advance(&k, 1001, 0);
    assert(k.state == KNOCK_WAIT && k.count == 0);
    knock_edge(&k);
    assert(k.state == KNOCK_COLLECT && k.count == 1);
    puts("PASS: window anchored to first knock; timeout resets next sequence");

    knock_init(&k, 0);
    knock_edge(&k);
    for (i = 0; i < 10000; ++i) {
        knock_tick(&k, 1);
        knock_edge(&k);
    }
    assert(k.count == 1);
    knock_tick(&k, 1);
    assert(k.state == KNOCK_WAIT);
    puts("PASS: stuck-high input cannot produce three knocks");

    knock_init(&k, 0);
    knock_edge(&k);
    advance(&k, 1000, 1);
    advance(&k, 29, 0);
    knock_edge(&k);
    assert(k.count == 1);
    knock_tick(&k, 0);
    knock_edge(&k);
    assert(k.count == 2);
    puts("PASS: rearm requires 30 ms stable low");

    knock_init(&k, 0);
    three(&k);
    for (i = 0; i < 60000; ++i) {
        knock_edge(&k);
        knock_tick(&k, 0);
    }
    assert(k.state == KNOCK_WAIT);
    puts("PASS: knocks during power do not extend 60 s supply");

    knock_init(&k, 1);
    knock_edge(&k);
    advance(&k, 65000, 0);
    assert(k.state == KNOCK_TEST && k.count == 0);
    knock_init(&k, 0);
    assert(k.state == KNOCK_WAIT);
    three(&k);
    knock_init(&k, 1);
    assert(k.state == KNOCK_TEST);
    knock_init(&k, 0);
    assert(k.state == KNOCK_WAIT && k.count == 0);
    puts("PASS: test mode supply and reset-time mode selection");

    knock_init(&k, 0);
    k.now = 65000;
    for (i = 0; i < 20; ++i) {
        three(&k);
        advance(&k, 60000, 0);
        assert(k.state == KNOCK_WAIT);
    }
    puts("PASS: repeated cycles crossing 16-bit millisecond rollover");
    return 0;
}
