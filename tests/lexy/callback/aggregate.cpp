// Copyright (C) 2020-2021 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <lexy/callback/aggregate.hpp>

#include <doctest/doctest.h>

TEST_CASE("as_aggregate")
{
    struct agg
    {
        int         i;
        float       f;
        const char* str;
    };
    using member_i   = lexy::make_member_ptr<&agg::i>;
    using member_f   = lexy::make_member_ptr<&agg::f>;
    using member_str = lexy::make_member_ptr<&agg::str>;

    static constexpr auto callback = lexy::as_aggregate<agg>;
    SUBCASE("callback")
    {
        constexpr auto result = callback(member_f{}, 3.14f, member_str{}, "hello", member_i{}, 42);
        CHECK(result.i == 42);
        CHECK(result.f == 3.14f);
        CHECK(*result.str == 'h');

        constexpr auto result2 = callback(agg(result), member_f{}, 2.71f, member_i{}, 11);
        CHECK(result2.i == 11);
        CHECK(result2.f == 2.71f);
        CHECK(*result2.str == 'h');
    }
    SUBCASE("sink")
    {
        constexpr auto result = [] {
            auto sink = callback.sink();
            sink(member_i{}, 11);
            sink(member_str{}, "hello");
            sink(member_f{}, 3.14f);
            sink(member_i{}, 42);
            return LEXY_MOV(sink).finish();
        }();
        CHECK(result.i == 42);
        CHECK(result.f == 3.14f);
        CHECK(*result.str == 'h');
    }
}

