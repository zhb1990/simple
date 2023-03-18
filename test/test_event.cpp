#include <gtest/gtest.h>

#include <iostream>
#include <simple/application/call_router.hpp>
#include <simple/application/event_system.hpp>
#include <simple/utils/func_traits.hpp>

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
    { auto r2 = system.register_handler<event1>(event1_h2); }
    event1_h3 h3{120};
    auto r3 = system.register_handler<event1>(&event1_h3::on_event, &h3);
    event1 ev1{100};
    system.fire_event(ev1);
    EXPECT_EQ(h1_val, 100);
    EXPECT_EQ(h2_val, 10);
    EXPECT_EQ(h3.b, 220);
}

struct event2 {
    std::vector<int> a{1, 2};
};

struct event2_h1 {
    int b{10};
    std::string c;

    void test3(event2& ev) {
        for (auto i : ev.a) {
            b += i;
        }

        ev.a.push_back(b);
    }

    int& test4(int p1, int p2, std::string& p3) {
        c = std::move(p3);
        b = p1 + p2;
        return b;
    }
};

TEST(service, call_router) {
    event2 ev33{{9, 8, 7}};
    event2_h1 h1;
    simple::call_router router;
    router.register_call("test3", &event2_h1::test3, &h1);
    router.register_call("test4", &event2_h1::test4, &h1);
    router.call("test3", ev33);
    EXPECT_EQ(ev33.a.size(), 4);
    EXPECT_EQ(ev33.a[3], h1.b);
    EXPECT_EQ(h1.b, 9 + 8 + 7 + 10);
    std::string s1 = "hello";
    auto& r1 = router.call<int&>("test4", 2, 3, s1);
    EXPECT_EQ(h1.b, 2 + 3);
    EXPECT_EQ(h1.c, "hello");
    EXPECT_EQ(s1.size(), 0);
    r1 = 100;
    EXPECT_EQ(h1.b, 100);
}
