// Copyright (C) 2020-2021 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef LEXY_DSL_CONTEXT_FLAG_HPP_INCLUDED
#define LEXY_DSL_CONTEXT_FLAG_HPP_INCLUDED

#include <lexy/dsl/base.hpp>
#include <lexy/error.hpp>

namespace lexyd
{
template <typename Id, bool InitialValue>
struct _ctx_fcreate : rule_base
{
    template <typename NextParser>
    struct parser
    {
        template <typename Context, typename Reader, typename... Args>
        LEXY_DSL_FUNC bool parse(Context& context, Reader& reader, Args&&... args)
        {
            // Add the flag to the context.
            auto flag_ctx = context.insert(Id{}, InitialValue);
            return NextParser::parse(flag_ctx, reader, LEXY_FWD(args)...);
        }
    };
};

template <typename Id, bool Value>
struct _ctx_fset : rule_base
{
    template <typename NextParser>
    struct parser
    {
        template <typename Context, typename Reader, typename... Args>
        LEXY_DSL_FUNC bool parse(Context& context, Reader& reader, Args&&... args)
        {
            context.get(Id{}) = Value;
            return NextParser::parse(context, reader, LEXY_FWD(args)...);
        }
    };
};

template <typename Id>
struct _ctx_ftoggle : rule_base
{
    template <typename NextParser>
    struct parser
    {
        template <typename Context, typename Reader, typename... Args>
        LEXY_DSL_FUNC bool parse(Context& context, Reader& reader, Args&&... args)
        {
            context.get(Id{}) = !context.get(Id{});
            return NextParser::parse(context, reader, LEXY_FWD(args)...);
        }
    };
};

template <typename Id, typename R, typename S>
struct _ctx_fselect : rule_base
{
    template <typename NextParser>
    struct parser
    {
        template <typename Context, typename Reader, typename... Args>
        LEXY_DSL_FUNC bool parse(Context& context, Reader& reader, Args&&... args)
        {
            if (context.get(Id{}))
                return lexy::rule_parser<R, NextParser>::parse(context, reader, LEXY_FWD(args)...);
            else
                return lexy::rule_parser<S, NextParser>::parse(context, reader, LEXY_FWD(args)...);
        }
    };
};

template <typename Id, typename Tag, bool Value>
struct _ctx_frequire : rule_base
{
    template <typename NextParser>
    struct parser
    {
        template <typename Context, typename Reader, typename... Args>
        LEXY_DSL_FUNC bool parse(Context& context, Reader& reader, Args&&... args)
        {
            if (context.get(Id{}) == Value)
                return NextParser::parse(context, reader, LEXY_FWD(args)...);
            else
            {
                auto err = lexy::make_error<Reader, Tag>(reader.cur());
                context.error(err);
                return false;
            }
        }
    };
};
} // namespace lexyd

namespace lexyd
{
template <typename Id, bool Value>
struct _ctx_flag_require
{
    template <typename Tag>
    static constexpr _ctx_frequire<Id, Tag, Value> error = {};
};

template <typename Id>
struct _ctx_flag
{
    struct id
    {};

    template <bool InitialValue = false>
    constexpr auto create() const
    {
        return _ctx_fcreate<id, InitialValue>{};
    }

    constexpr auto set() const
    {
        return _ctx_fset<id, true>{};
    }
    constexpr auto reset() const
    {
        return _ctx_fset<id, false>{};
    }

    constexpr auto toggle() const
    {
        return _ctx_ftoggle<id>{};
    }

    template <typename R, typename S>
    constexpr auto select(R, S) const
    {
        return _ctx_fselect<id, R, S>{};
    }

    template <bool Value = true>
    constexpr auto require() const
    {
        return _ctx_flag_require<id, Value>{};
    }

    template <typename Tag>
    LEXY_DEPRECATED_ERROR("replace `flag.require<Tag>()` by `flag.require().error<Tag>`")
    constexpr auto require() const
    {
        return require().template error<Tag>;
    }
    template <bool Value, typename Tag>
    LEXY_DEPRECATED_ERROR(
        "replace `flag.require<false, Tag>()` by `flag.require<false>().error<Tag>`")
    constexpr auto require() const
    {
        return require<Value>().template error<Tag>;
    }
};

/// Declares a flag.
template <typename Id>
constexpr auto context_flag = _ctx_flag<Id>{};
} // namespace lexyd

#endif // LEXY_DSL_CONTEXT_FLAG_HPP_INCLUDED

