// Copyright (C) 2020-2021 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef LEXY_PRODUCTION_HPP_INCLUDED
#define LEXY_PRODUCTION_HPP_INCLUDED

#include <lexy/_detail/config.hpp>
#include <lexy/_detail/detect.hpp>
#include <lexy/_detail/type_name.hpp>
#include <lexy/callback/base.hpp>

#ifdef LEXY_IGNORE_DEPRECATED_LIST
#    define LEXY_DEPRECATED_LIST
#else
#    define LEXY_DEPRECATED_LIST                                                                   \
        [[deprecated("Production::list has been deprecated; use ::value instead.")]]
#endif

namespace lexy
{
template <typename Production>
using production_rule = std::decay_t<decltype(Production::rule)>;

template <typename Production>
constexpr bool is_production = _detail::is_detected<production_rule, Production>;

/// Base class to indicate that this production is conceptually a token.
/// This inhibits whitespace skipping inside the production.
///
/// When generating a parse tree, it will also merge tokens of the same kind into the same node.
struct token_production
{};

template <typename Production>
constexpr bool is_token_production = std::is_base_of_v<token_production, Production>;

/// Base class to indicate that this production is transparent for the parse tree generation.
/// It will not create a node in the tree, all children will be added to the its parent.
/// If parse tree generation is not used, it has no effect.
struct transparent_production
{};

template <typename Production>
constexpr bool is_transparent_production = std::is_base_of_v<transparent_production, Production>;
} // namespace lexy

namespace lexy
{
template <typename Production>
LEXY_CONSTEVAL const char* production_name()
{
    return _detail::type_name<Production>();
}

template <typename Production>
using _detect_whitespace = decltype(Production::whitespace);

template <typename Production, typename Root>
auto _production_whitespace()
{
    if constexpr (is_token_production<Production>)
        return; // void
    else if constexpr (lexy::_detail::is_detected<_detect_whitespace, Production>)
        return Production::whitespace;
    else if constexpr (lexy::_detail::is_detected<_detect_whitespace, Root>)
        return Root::whitespace;
    else
        return; // void
}
template <typename Production, typename Root>
using production_whitespace = decltype(_production_whitespace<Production, Root>());
} // namespace lexy

namespace lexy
{
template <typename Production>
using _detect_value = decltype(&Production::value);
template <typename Production>
using _detect_list = decltype(&Production::list);

template <typename Production, bool HasList = _detail::is_detected<_detect_list, Production>,
          bool HasValue = _detail::is_detected<_detect_value, Production>>
struct _prod_value;
template <typename Production>
struct _prod_value<Production, true, true>
{
    // Before:
    //    static constexpr auto list = sink;
    //    static constexpr auto value = callback;
    // After:
    //    static constexpr auto value = sink >> callback;
    // Define LEXY_IGNORE_DEPRECATED_LIST to fix later.
    LEXY_DEPRECATED_LIST
    static constexpr auto get = Production::list >> Production::value;
};
template <typename Production>
struct _prod_value<Production, true, false>
{
    // Before:
    //    static constexpr auto list = sink;
    // After:
    //    static constexpr auto value = sink;
    // Define LEXY_IGNORE_DEPRECATED_LIST to fix later.
    LEXY_DEPRECATED_LIST
    static constexpr auto get = Production::list;
};
template <typename Production>
struct _prod_value<Production, false, true>
{
    static constexpr auto get = Production::value;
};
template <typename Production>
struct _prod_value<Production, false, false>
{
    static_assert(_detail::error<Production>, "missing Production::value member");
    static constexpr auto get = Production::value;
};

template <typename Production>
struct production_value
{
    static constexpr auto get = _prod_value<Production>::get;
    using type                = std::decay_t<decltype(get)>;
};
} // namespace lexy

#endif // LEXY_PRODUCTION_HPP_INCLUDED

