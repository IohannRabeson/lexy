// Copyright (C) 2020-2021 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef LEXY_CALLBACK_BIND_HPP_INCLUDED
#define LEXY_CALLBACK_BIND_HPP_INCLUDED

#include <lexy/_detail/tuple.hpp>
#include <lexy/callback/base.hpp>

//=== placeholder details ===//
namespace lexy::_detail
{
struct placeholder_base
{};
template <typename T>
constexpr auto is_placeholder = std::is_base_of_v<placeholder_base, T>;

template <typename BoundArg, typename Context, typename... Args>
constexpr decltype(auto) expand_bound_arg(const BoundArg& bound, const Context& context,
                                          const _detail::tuple<Args...>& actual_args)
{
    if constexpr (is_placeholder<std::decay_t<BoundArg>>)
        return bound(context, actual_args);
    else
        return bound;
}

struct all_values_placeholder
{};

struct no_bind_context
{};

template <std::size_t Idx, typename Fn, typename... BoundArgs, typename Context,
          typename... ActualArgs, std::size_t... ActualIdx, typename... ProducedArgs>
constexpr decltype(auto) _invoke_bound(Fn&& fn, const _detail::tuple<BoundArgs...>& bound_args,
                                       const Context&                       context,
                                       const _detail::tuple<ActualArgs...>& actual_args,
                                       _detail::index_sequence<ActualIdx...>,
                                       ProducedArgs&&... produced_args)
{
    if constexpr (Idx == sizeof...(BoundArgs))
    {
        (void)bound_args;
        (void)context;
        (void)actual_args;
        return LEXY_FWD(fn)(LEXY_FWD(produced_args)...);
    }
    else
    {
        using bound_arg_t
            = std::decay_t<typename _detail::tuple<BoundArgs...>::template element_type<Idx>>;
        if constexpr (std::is_same_v<bound_arg_t, all_values_placeholder>)
        {
            return _invoke_bound<Idx + 1>(LEXY_FWD(fn), bound_args, context, actual_args,
                                          actual_args.index_sequence(), LEXY_FWD(produced_args)...,
                                          // Expand to all actual arguments.
                                          LEXY_FWD(actual_args.template get<ActualIdx>())...);
        }
        else
        {
            return _invoke_bound<Idx + 1>(LEXY_FWD(fn), bound_args, context, actual_args,
                                          actual_args.index_sequence(), LEXY_FWD(produced_args)...,
                                          // Expand the currently bound argument
                                          expand_bound_arg(bound_args.template get<Idx>(), context,
                                                           actual_args));
        }
    }
}

template <typename Fn, typename... BoundArgs, std::size_t... Idx, typename Context,
          typename... Args>
constexpr decltype(auto) invoke_bound(Fn&&                                fn, //
                                      const _detail::tuple<BoundArgs...>& bound_args,
                                      _detail::index_sequence<Idx...>, //
                                      const Context& context, Args&&... args)
{
    auto actual_args = _detail::forward_as_tuple(LEXY_FWD(args)...);
    if constexpr ((std::is_same_v<std::decay_t<BoundArgs>, all_values_placeholder> || ...))
    {
        // If we're having the placeholder we need to recursively expand every argument.
        return _invoke_bound<0>(LEXY_FWD(fn), bound_args, context, actual_args,
                                actual_args.index_sequence());
    }
    else
    {
        // If we're not having the all_values_placeholder, every placeholder expands to a single
        // argument. We can thus expand it easily by mapping each value of bound args to the actual
        // argument.
        return LEXY_FWD(fn)(
            expand_bound_arg(bound_args.template get<Idx>(), context, actual_args)...);
    }
}
} // namespace lexy::_detail

//=== placeholders ===//
namespace lexy
{
/// Placeholder for bind that expands to all values produced by the rule.
constexpr auto values = _detail::all_values_placeholder{};

struct nullopt;

template <std::size_t N, typename T, typename Fn>
struct _nth_value : _detail::placeholder_base // fallback + map
{
    LEXY_EMPTY_MEMBER T  _fallback;
    LEXY_EMPTY_MEMBER Fn _fn;

    template <typename Context, typename... Args>
    LEXY_FORCE_INLINE constexpr decltype(auto) operator()(const Context&,
                                                          const _detail::tuple<Args...>& args) const
    {
        if constexpr (N > sizeof...(Args))
            return _fallback; // Argument is missing.
        else
        {
            using arg_t = typename _detail::tuple<Args...>::template element_type<N - 1>;
            if constexpr (std::is_same_v<std::decay_t<arg_t>, nullopt>)
                return _fallback; // Argument is nullopt.
            else
                return _detail::invoke(_fn, args.template get<N - 1>());
        }
    }
};
template <std::size_t N, typename T>
struct _nth_value<N, T, void> : _detail::placeholder_base // fallback only
{
    LEXY_EMPTY_MEMBER T _fallback;

    template <typename Context, typename... Args>
    LEXY_FORCE_INLINE constexpr decltype(auto) operator()(const Context&,
                                                          const _detail::tuple<Args...>& args) const
    {
        if constexpr (N > sizeof...(Args))
            return _fallback; // Argument is missing.
        else
        {
            using arg_t = typename _detail::tuple<Args...>::template element_type<N - 1>;
            if constexpr (std::is_same_v<std::decay_t<arg_t>, nullopt>)
                return _fallback; // Argument is nullopt.
            else
                return args.template get<N - 1>();
        }
    }

    template <typename Fn>
    constexpr auto map(Fn&& fn) const
    {
        return _nth_value<N, T, std::decay_t<Fn>>{{}, _fallback, LEXY_FWD(fn)};
    }
};
template <std::size_t N, typename Fn>
struct _nth_value<N, void, Fn> : _detail::placeholder_base // map only
{
    LEXY_EMPTY_MEMBER Fn _fn;

    template <typename Context, typename... Args>
    LEXY_FORCE_INLINE constexpr decltype(auto) operator()(const Context&,
                                                          const _detail::tuple<Args...>& args) const
    {
        static_assert(N <= sizeof...(Args), "not enough arguments for nth_value<N>");
        return _detail::invoke(_fn, args.template get<N - 1>());
    }

    template <typename Arg>
    constexpr auto operator||(Arg&& fallback) const
    {
        return _nth_value<N, std::decay_t<Arg>, Fn>{{}, LEXY_FWD(fallback), _fn};
    }
    template <typename Arg>
    constexpr auto or_(Arg&& fallback) const
    {
        return *this || LEXY_FWD(fallback);
    }
};
template <std::size_t N>
struct _nth_value<N, void, void> : _detail::placeholder_base
{
    // I'm sorry, but this is for consistency with std::bind.
    static_assert(N > 0, "values are 1-indexed");

    template <typename Context, typename... Args>
    LEXY_FORCE_INLINE constexpr decltype(auto) operator()(const Context&,
                                                          const _detail::tuple<Args...>& args) const
    {
        static_assert(N <= sizeof...(Args), "not enough arguments for nth_value<N>");
        return args.template get<N - 1>();
    }

    template <typename Arg>
    constexpr auto operator||(Arg&& fallback) const
    {
        return _nth_value<N, std::decay_t<Arg>, void>{{}, LEXY_FWD(fallback)};
    }
    template <typename Arg>
    constexpr auto or_(Arg&& fallback) const
    {
        return *this || LEXY_FWD(fallback);
    }

    template <typename Fn>
    constexpr auto map(Fn&& fn) const
    {
        return _nth_value<N, void, std::decay_t<Fn>>{{}, LEXY_FWD(fn)};
    }
};

/// Placeholder for bind that expands to the nth value produced by the rule.
template <std::size_t N>
constexpr auto nth_value = _nth_value<N, void, void>{};

inline namespace placeholders
{
    constexpr auto _1 = nth_value<1>;
    constexpr auto _2 = nth_value<2>;
    constexpr auto _3 = nth_value<3>;
    constexpr auto _4 = nth_value<4>;
    constexpr auto _5 = nth_value<5>;
    constexpr auto _6 = nth_value<6>;
    constexpr auto _7 = nth_value<7>;
    constexpr auto _8 = nth_value<8>;
} // namespace placeholders

template <typename Fn>
struct _parse_state : _detail::placeholder_base
{
    LEXY_EMPTY_MEMBER Fn _fn;

    template <typename Context, typename... Args>
    constexpr decltype(auto) operator()(const Context& context,
                                        const _detail::tuple<Args...>&) const
    {
        static_assert(!std::is_same_v<Context, _detail::no_bind_context>,
                      "lexy::parse_state requires that a state is passed to lexy::parse()");
        return _detail::invoke(_fn, context);
    }
};
template <>
struct _parse_state<void> : _detail::placeholder_base
{
    template <typename Context, typename... Args>
    constexpr decltype(auto) operator()(const Context& context,
                                        const _detail::tuple<Args...>&) const
    {
        static_assert(!std::is_same_v<Context, _detail::no_bind_context>,
                      "lexy::parse_state requires that a state is passed to lexy::parse()");
        return context;
    }

    template <typename Fn>
    constexpr auto map(Fn&& fn) const
    {
        return _parse_state<std::decay_t<Fn>>{{}, LEXY_FWD(fn)};
    }
};

constexpr auto parse_state = _parse_state<void>{};
} // namespace lexy

//=== bind ===//
namespace lexy
{
// the callback specific part of bind
template <typename Derived, typename Callback, typename Void = void>
struct _bind_impl_callback
{};
template <typename Derived, typename Callback>
struct _bind_impl_callback<Derived, Callback, _detail::void_t<typename Callback::return_type>>
{
    using return_type = typename Callback::return_type;

    template <typename Context>
    struct _with_context
    {
        const Derived& _derived;
        const Context& _context;

        template <typename... Args>
        constexpr auto operator()(Args&&... args) const&&
        {
            return _detail::invoke_bound(_derived._callback, _derived._bound,
                                         _derived._bound.index_sequence(), _context,
                                         LEXY_FWD(args)...);
        }
    };

    template <typename Context>
    constexpr auto operator[](const Context& context) const
    {
        auto& derived = static_cast<const Derived&>(*this);
        return _with_context<Context>{derived, context};
    }

    template <typename... Args>
    constexpr return_type operator()(Args&&... args) const
    {
        auto& derived = static_cast<const Derived&>(*this);
        return _detail::invoke_bound(derived._callback, derived._bound,
                                     derived._bound.index_sequence(), _detail::no_bind_context{},
                                     LEXY_FWD(args)...);
    }
};

// the sink specific part of bind
template <typename Derived, typename Callback, typename Void = void>
struct _bind_impl_sink
{};
template <typename Derived, typename Callback>
struct _bind_impl_sink<Derived, Callback, _detail::void_t<lexy::sink_callback<Callback>>>
{
    template <typename Context>
    struct _sink_callback
    {
        const Derived&    _derived;
        LEXY_EMPTY_MEMBER lexy::sink_callback<Callback> _callback;
        LEXY_EMPTY_MEMBER Context                       _context;

        using return_type = typename lexy::sink_callback<Callback>::return_type;

        template <typename... Args>
        constexpr void operator()(Args&&... args)
        {
            _detail::invoke_bound(_callback, _derived._bound, _derived._bound.index_sequence(),
                                  _context, LEXY_FWD(args)...);
        }

        constexpr decltype(auto) finish() &&
        {
            return LEXY_MOV(_callback).finish();
        }
    };

    constexpr auto sink() const -> _sink_callback<_detail::no_bind_context>
    {
        auto& derived = static_cast<const Derived&>(*this);
        return {derived, derived._callback.sink(), _detail::no_bind_context{}};
    }
    template <typename Context>
    constexpr auto sink(const Context& context) const -> _sink_callback<Context>
    {
        auto& derived = static_cast<const Derived&>(*this);
        return {derived, derived._callback.sink(), context};
    }
};

/// For a callback, binds the `operator()` of the callback with pre-defined/remapped values.
/// For a sink, binds the `operator()` of the sink callback invoked for every item.
template <typename Callback, typename... BoundArgs>
constexpr auto bind(Callback&& callback, BoundArgs&&... args)
{
    using callback_t = std::decay_t<Callback>;
    struct bound : _bind_impl_callback<bound, callback_t>, _bind_impl_sink<bound, callback_t>
    {
        LEXY_EMPTY_MEMBER callback_t _callback;
        LEXY_EMPTY_MEMBER _detail::tuple<std::decay_t<BoundArgs>...> _bound;
    };

    return bound{{}, {}, LEXY_FWD(callback), _detail::make_tuple(LEXY_FWD(args)...)};
}
} // namespace lexy

namespace lexy
{
template <typename Sink>
struct _sink_wrapper
{
    const Sink& _sink;

    template <typename... Args>
    constexpr auto operator()(Args&&... args)
    {
        return _sink.sink(LEXY_FWD(args)...);
    }
};

template <typename Sink, typename... BoundArgs>
struct _bound_sink
{
    LEXY_EMPTY_MEMBER Sink _sink;
    LEXY_EMPTY_MEMBER _detail::tuple<BoundArgs...> _bound;

    constexpr auto sink() const
    {
        return _detail::invoke_bound(_sink_wrapper<Sink>{_sink}, _bound, _bound.index_sequence(),
                                     _detail::no_bind_context{});
    }
    template <typename Context>
    constexpr auto sink(const Context& context) const
    {
        return _detail::invoke_bound(_sink_wrapper<Sink>{_sink}, _bound, _bound.index_sequence(),
                                     context);
    }
};

/// Binds the `.sink()` function of a sink.
/// The result has a `.sink()` function that accepts the context (i.e. the parse state), but no
/// additional values.
template <typename Sink, typename... BoundArgs>
constexpr auto bind_sink(Sink&& sink, BoundArgs&&... args)
{
    static_assert(
        (!std::is_same_v<std::decay_t<BoundArgs>, _detail::all_values_placeholder> && ...),
        "lexy::values as a placeholder for bind_sink() doesn't make sense - there won't be any values");
    using bound = _bound_sink<std::decay_t<Sink>, std::decay_t<BoundArgs>...>;
    return bound{LEXY_FWD(sink), _detail::make_tuple(LEXY_FWD(args)...)};
}
} // namespace lexy

#endif // LEXY_CALLBACK_BIND_HPP_INCLUDED

