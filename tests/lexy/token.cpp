// Copyright (C) 2020-2021 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <lexy/token.hpp>

#include <doctest/doctest.h>
#include <lexy/dsl/punctuator.hpp>
#include <lexy/dsl/token.hpp>
#include <lexy/input/string_input.hpp>

namespace
{
enum class token_kind
{
    a,
    b,
    c,
};

const char* token_kind_name(token_kind k)
{
    switch (k)
    {
    case token_kind::a:
        return "a";
    case token_kind::b:
        return "b";
    case token_kind::c:
        return "c";
    }

    return "";
}
} // namespace

template <>
constexpr auto lexy::token_kind_map_for<::token_kind> = lexy::token_kind_map.map<::token_kind::c>(
    LEXY_LIT("."));

TEST_CASE("token_kind")
{
    SUBCASE("void")
    {
        lexy::token_kind<> def;
        CHECK(!def);
        CHECK(def.is_predefined());
        CHECK(def.get() == -1);
        CHECK(def.name() == lexy::_detail::string_view("token"));
        CHECK(def == lexy::unknown_token_kind);

        lexy::token_kind unknown(lexy::unknown_token_kind);
        CHECK(!unknown);
        CHECK(unknown.is_predefined());
        CHECK(unknown.get() == -1);
        CHECK(unknown.name() == lexy::_detail::string_view("token"));
        CHECK(unknown == lexy::unknown_token_kind);

        lexy::token_kind whitespace(lexy::whitespace_token_kind);
        CHECK(whitespace);
        CHECK(whitespace.is_predefined());
        CHECK(whitespace.get() == -2);
        CHECK(whitespace.name() == lexy::_detail::string_view("whitespace"));
        CHECK(whitespace == lexy::whitespace_token_kind);

        lexy::token_kind position(lexy::position_token_kind);
        CHECK(position);
        CHECK(position.is_predefined());
        CHECK(position.get() == -3);
        CHECK(position.name() == lexy::_detail::string_view("position"));
        CHECK(position == lexy::position_token_kind);

        lexy::token_kind eof(lexy::eof_token_kind);
        CHECK(eof);
        CHECK(eof.is_predefined());
        CHECK(eof.get() == -4);
        CHECK(eof.name() == lexy::_detail::string_view("EOF"));
        CHECK(eof == lexy::eof_token_kind);

        lexy::token_kind value(0);
        CHECK(value);
        CHECK(!value.is_predefined());
        CHECK(value.get() == 0);
        CHECK(value.name() == lexy::_detail::string_view("token"));
        CHECK(value == 0);

        lexy::token_kind period(lexy::dsl::period);
        CHECK(!period);
        CHECK(period.is_predefined());
        CHECK(period.get() == -1);
        CHECK(period.name() == lexy::_detail::string_view("token"));
        CHECK(period == lexy::unknown_token_kind);
        CHECK(period == lexy::dsl::period);

        lexy::token_kind manual(lexy::dsl::period.kind<42>);
        CHECK(manual);
        CHECK(!manual.is_predefined());
        CHECK(manual.get() == 42);
        CHECK(manual.name() == lexy::_detail::string_view("token"));
        CHECK(manual == 42);
        CHECK(manual == lexy::dsl::period.kind<42>);
    }
    SUBCASE("enum")
    {
        lexy::token_kind<token_kind> def;
        CHECK(!def);
        CHECK(def.is_predefined());
        CHECK(def.name() == lexy::_detail::string_view("token"));
        CHECK(def == lexy::unknown_token_kind);

        lexy::token_kind<token_kind> unknown(lexy::unknown_token_kind);
        CHECK(!unknown);
        CHECK(unknown.is_predefined());
        CHECK(unknown.name() == lexy::_detail::string_view("token"));
        CHECK(unknown == lexy::unknown_token_kind);

        lexy::token_kind position(lexy::position_token_kind);
        CHECK(position);
        CHECK(position.is_predefined());
        CHECK(position.name() == lexy::_detail::string_view("position"));
        CHECK(position == lexy::position_token_kind);

        lexy::token_kind<token_kind> eof(lexy::eof_token_kind);
        CHECK(eof);
        CHECK(eof.is_predefined());
        CHECK(eof.name() == lexy::_detail::string_view("EOF"));
        CHECK(eof == lexy::eof_token_kind);

        lexy::token_kind value(token_kind::a);
        CHECK(value);
        CHECK(!value.is_predefined());
        CHECK(value.get() == token_kind::a);
        CHECK(value.name() == lexy::_detail::string_view("a"));
        CHECK(value == token_kind::a);

        lexy::token_kind<token_kind> period(lexy::dsl::period);
        CHECK(period);
        CHECK(!period.is_predefined());
        CHECK(period.get() == token_kind::c);
        CHECK(period.name() == lexy::_detail::string_view("c"));
        CHECK(period == token_kind::c);
        CHECK(period == lexy::dsl::period);

        lexy::token_kind manual(lexy::dsl::period.kind<token_kind::b>);
        CHECK(manual);
        CHECK(!manual.is_predefined());
        CHECK(manual.get() == token_kind::b);
        CHECK(manual.name() == lexy::_detail::string_view("b"));
        CHECK(manual == token_kind::b);
        CHECK(manual == lexy::dsl::period.kind<token_kind::b>);
    }
}

TEST_CASE("token")
{
    lexy::string_input input = lexy::zstring_input("abc");

    auto lexeme = [&] {
        auto reader = input.reader();

        auto begin = reader.cur();
        reader.bump();
        reader.bump();
        reader.bump();

        return lexy::lexeme(reader, begin);
    }();

    SUBCASE("void")
    {
        lexy::token zero(0, lexeme);
        CHECK(zero.kind() == 0);
        CHECK(zero.name() == lexy::_detail::string_view("token"));
        CHECK(zero.position() == input.begin());
        CHECK(zero.lexeme().begin() == input.begin());
        CHECK(zero.lexeme().size() == 3);

        lexy::token_for<decltype(input)> unknown(lexy::unknown_token_kind, lexeme);
        CHECK(unknown.kind() == lexy::unknown_token_kind);
        CHECK(unknown.name() == lexy::_detail::string_view("token"));
        CHECK(unknown.position() == input.begin());
        CHECK(unknown.lexeme().begin() == input.begin());
        CHECK(unknown.lexeme().size() == 3);

        lexy::token_for<decltype(input)> period(lexy::dsl::period, lexeme);
        CHECK(period.kind() == lexy::unknown_token_kind);
        CHECK(period.name() == lexy::_detail::string_view("token"));
        CHECK(period.position() == input.begin());
        CHECK(period.lexeme().begin() == input.begin());
        CHECK(period.lexeme().size() == 3);
    }
    SUBCASE("enum")
    {
        lexy::token b(token_kind::b, lexeme);
        CHECK(b.kind() == token_kind::b);
        CHECK(b.name() == lexy::_detail::string_view("b"));
        CHECK(b.position() == input.begin());
        CHECK(b.lexeme().begin() == input.begin());
        CHECK(b.lexeme().size() == 3);

        lexy::token_for<decltype(input), token_kind> unknown(lexy::unknown_token_kind, lexeme);
        CHECK(unknown.kind() == lexy::unknown_token_kind);
        CHECK(unknown.name() == lexy::_detail::string_view("token"));
        CHECK(unknown.position() == input.begin());
        CHECK(unknown.lexeme().begin() == input.begin());
        CHECK(unknown.lexeme().size() == 3);

        lexy::token_for<decltype(input), token_kind> period(lexy::dsl::period, lexeme);
        CHECK(period.kind() == token_kind::c);
        CHECK(period.name() == lexy::_detail::string_view("c"));
        CHECK(period.position() == input.begin());
        CHECK(period.lexeme().begin() == input.begin());
        CHECK(period.lexeme().size() == 3);
    }
}

