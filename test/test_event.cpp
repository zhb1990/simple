#include <gtest/gtest.h>

#include <simple/application/event_system.hpp>

struct event1 {
    int a{0};
};

static int h1_val = 10;

static void event1_h1(const event1& ev) { h1_val = ev.a; }

static int h2_val = 10;

static void event1_h2(event1 ev) { h2_val = ev.a * ev.a; }

struct event1_h3 {
    int b{10};

    void on_event(const event1& ev) { b += ev.a; }
};

TEST(service, event_system) {
    simple::event_system system;
    auto r1 = system.register_handler<event1>(event1_h1);
    {
        auto r2 = system.register_handler<event1>(event1_h2);
    }
    event1_h3 h3{120};
    auto r3 = system.register_handler<event1>(&event1_h3::on_event, &h3);
    event1 ev1{100};
    system.fire_event(ev1);
    EXPECT_EQ(h1_val, 100);
    EXPECT_EQ(h2_val, 10);
    EXPECT_EQ(h3.b, 220);
}
