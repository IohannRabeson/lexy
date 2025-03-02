// Copyright (C) 2020-2021 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef LEXY_DSL_INTEGER_HPP_INCLUDED
#define LEXY_DSL_INTEGER_HPP_INCLUDED

#include <limits>

#include <lexy/_detail/assert.hpp>
#include <lexy/dsl/base.hpp>
#include <lexy/dsl/digit.hpp>

namespace lexy
{
// Number of digits to express the given value.
template <typename Integer>
constexpr std::size_t _digit_count(int radix, Integer value)
{
    LEXY_PRECONDITION(value >= Integer(0));

    if (value == 0)
        return 1;

    std::size_t result = 0;
    while (value > 0)
    {
        value = Integer(value / Integer(radix));
        ++result;
    }
    return result;
}

template <typename T>
struct integer_traits
{
    using _limits = std::numeric_limits<T>;
    static_assert(_limits::is_integer);

    using type = T;

    static constexpr auto is_bounded = _limits::is_bounded;

    template <int Radix>
    static constexpr std::size_t max_digit_count = _digit_count(Radix, _limits::max());

    template <int Radix>
    static constexpr void add_digit_unchecked(T& result, unsigned digit)
    {
        result = T(result * T(Radix) + T(digit));
    }

    template <int Radix>
    static constexpr bool add_digit_checked(T& result, unsigned digit)
    {
        constexpr auto can_use_unsigned = [] {
            if constexpr (!std::is_integral_v<T>)
                // If it's not a built-in integer, be careful and don't do it.
                return false;
            else if constexpr (sizeof(T) >= sizeof(unsigned))
                // If it's bigger than unsigned, we can't use unsigned.
                return false;
            else
            {
                // We can do it if the worst-case does not overflow unsigned.
                auto worst_case = static_cast<unsigned>(_limits::max());
                return integer_traits<unsigned>::add_digit_checked<Radix>(worst_case, Radix - 1);
            }
        }();

        // Check whether we can do an optimization for small integers,
        // where we do the operation on unsigned and check later.
        if constexpr (can_use_unsigned)
        {
            // This can't overflow, we've checked it above.
            auto value = static_cast<unsigned>(result) * Radix + digit;

            // Check whether the final value can fit.
            if (value > static_cast<unsigned>(_limits::max()))
                return false;
            else
            {
                result = static_cast<T>(value);
                return true;
            }
        }
        else
        {
            // result *= Radix
            constexpr auto max_per_radix = _limits::max() / Radix;
            if (result > max_per_radix)
                return false;
            result = T(result * Radix);

            // result += digit
            if (result > T(_limits::max() - digit))
                return false;
            result = T(result + T(digit));

            return true;
        }
    }
};

template <>
struct integer_traits<code_point>
{
    using type = code_point;

    static constexpr auto is_bounded = true;

    template <int Radix>
    static constexpr std::size_t max_digit_count = _digit_count(Radix, 0x10'FFFF);

    template <int Radix>
    static constexpr void add_digit_unchecked(type& result, unsigned digit)
    {
        std::uint_least32_t value = result.value();
        integer_traits<std::uint_least32_t>::add_digit_unchecked<Radix>(value, digit);
        result = code_point(value);
    }
    template <int Radix>
    static constexpr bool add_digit_checked(type& result, unsigned digit)
    {
        std::uint_least32_t value = result.value();
        if (!integer_traits<std::uint_least32_t>::add_digit_checked<Radix>(value, digit))
            return false;
        result = code_point(value);
        return result.is_valid();
    }
};

template <typename T>
struct unbounded
{};
template <typename T>
struct integer_traits<unbounded<T>>
{
    using type                       = typename integer_traits<T>::type;
    static constexpr auto is_bounded = false;

    template <int Radix>
    static constexpr void add_digit_unchecked(type& result, unsigned digit)
    {
        integer_traits<T>::template add_digit_unchecked<Radix>(result, digit);
    }
};
} // namespace lexy

namespace lexy
{
struct integer_overflow
{
    static LEXY_CONSTEVAL auto name()
    {
        return "integer overflow";
    }
};
} // namespace lexy

namespace lexyd
{
template <typename T>
constexpr bool _is_bounded = lexy::integer_traits<T>::is_bounded;

template <typename T, std::size_t N, int Radix>
constexpr bool _ndigits_can_overflow()
{
    using traits         = lexy::integer_traits<T>;
    auto max_digit_count = traits::template max_digit_count<Radix>;
    // We don't know whether the maximal value is a power of Radix,
    // so we have to be conservative and don't rule out overflow on the same count.
    return N >= max_digit_count;
}

// Parses T in the Base while checking for overflow.
template <typename T, typename Base>
struct _unbounded_integer_parser
{
    using traits      = lexy::integer_traits<T>;
    using result_type = typename traits::type;

    static constexpr auto radix = Base::radix;

    template <typename Iterator>
    static constexpr bool parse(result_type& result, Iterator cur, Iterator end)
    {
        // Just parse digits until we've run out of digits.
        while (cur != end)
        {
            auto digit = Base::value(*cur++);
            if (digit >= Base::radix)
                // Skip digit separator.
                continue;

            traits::template add_digit_unchecked<radix>(result, digit);
        }

        return true;
    }
};

// Parses T in the Base without checking for overflow.
template <typename T, typename Base, bool AssumeOnlyDigits>
struct _bounded_integer_parser
{
    using traits      = lexy::integer_traits<T>;
    using result_type = typename traits::type;

    static constexpr auto radix = Base::radix;

    template <typename Iterator>
    static constexpr unsigned find_digit(Iterator& cur, Iterator end)
    {
        auto digit = 0u;
        while (true)
        {
            if (cur == end)
                // No more digits.
                return unsigned(-1);

            digit = Base::value(*cur++);
            if constexpr (AssumeOnlyDigits)
                break;
            else if (digit < Base::radix)
                break;
        }
        return digit;
    }

    template <typename Iterator>
    static constexpr bool parse(result_type& result, Iterator cur, Iterator end)
    {
        constexpr auto max_digit_count = traits::template max_digit_count<radix>;
        static_assert(max_digit_count > 1,
                      "integer must be able to store all possible digit values");

        // Skip leading zeroes.
        while (true)
        {
            if (cur == end)
                return true; // We only had zeroes.

            const auto digit = Base::value(*cur++);
            if (digit == 0 || digit >= radix)
                continue; // Zero or digit separator.

            // First non-zero digit, so we can assign it instead of adding.
            result = result_type(digit);
            break;
        }
        // At this point, we've parsed exactly one non-zero digit.

        // Handle max_digit_count - 1 digits without checking for overflow.
        // We cannot overflow, as the maximal value has one digit more.
        for (std::size_t digit_count = 1; digit_count < max_digit_count - 1; ++digit_count)
        {
            auto digit = find_digit(cur, end);
            if (digit == unsigned(-1))
                return true;

            traits::template add_digit_unchecked<radix>(result, digit);
        }

        // Handle the final digit, if there is any, while checking for overflow.
        {
            auto digit = find_digit(cur, end);
            if (digit == unsigned(-1))
                return true;

            if (!traits::template add_digit_checked<radix>(result, digit))
                return false;
        }

        // If we've reached this point, we've parsed the maximal number of digits allowed.
        // Now we can only fail if there are still digits left.
        return cur == end;
    }
};

// Continuation of integer that assumes the rule is already dealt with.
template <typename T, typename Base, bool AssumeOnlyDigits, typename Tag>
struct _int_p : rule_base
{
    using integer_parser
        = std::conditional_t<_is_bounded<T>, _bounded_integer_parser<T, Base, AssumeOnlyDigits>,
                             _unbounded_integer_parser<T, Base>>;

    template <typename NextParser>
    struct parser
    {
        template <typename Context, typename Reader, typename Iterator, typename... Args>
        LEXY_DSL_FUNC bool parse(Context& context, Reader& reader, Iterator begin, Args&&... args)
        {
            using tag        = std::conditional_t<std::is_void_v<Tag>, lexy::integer_overflow, Tag>;
            using error_type = lexy::error<typename Reader::canonical_reader, tag>;

            auto result = typename integer_parser::result_type(0);
            if (integer_parser::parse(result, begin, reader.cur()))
                return NextParser::parse(context, reader, LEXY_FWD(args)..., result);
            else
            {
                context.error(error_type(begin, reader.cur()));
                return false;
            }
        }
    };
};

// Captures the rule which is then parsed.
// Must be followed by _int_p.
template <typename Rule>
struct _int_c : rule_base
{
    template <typename NextParser>
    struct parser
    {
        template <typename Context, typename Reader, typename... Args>
        LEXY_DSL_FUNC bool parse(Context& context, Reader& reader, Args&&... args)
        {
            return lexy::rule_parser<Rule, NextParser>::parse(context, reader, reader.cur(),
                                                              LEXY_FWD(args)...);
        }
    };
};

/// Parses the digits matched by the rule into an integer type.
template <typename T, typename Base, typename Rule>
constexpr auto integer(Rule)
{
    return _int_c<Rule>{} + _int_p<T, Base, false, void>{};
}

template <typename T, typename Base>
constexpr auto integer(_digits<Base>)
{
    return _int_c<_digits<Base>>{} + _int_p<T, Base, true, void>{};
}
template <typename T, typename Base, typename Sep>
constexpr auto integer(_digits_s<Base, Sep>)
{
    return _int_c<_digits_s<Base, Sep>>{} + _int_p<T, Base, false, void>{};
}
template <typename T, typename Base>
constexpr auto integer(_digits_t<Base>)
{
    return _int_c<_digits_t<Base>>{} + _int_p<T, Base, true, void>{};
}
template <typename T, typename Base, typename Sep>
constexpr auto integer(_digits_st<Base, Sep>)
{
    return _int_c<_digits_st<Base, Sep>>{} + _int_p<T, Base, false, void>{};
}

template <typename T, typename Base, std::size_t N>
constexpr auto integer(_ndigits<N, Base>)
{
    return _int_c<_ndigits<N, Base>>{} + _int_p<T, Base, true, void>{};
}
template <typename T, typename Base, std::size_t N, typename Sep>
constexpr auto integer(_ndigits_s<N, Base, Sep>)
{
    using type
        = std::conditional_t<_ndigits_can_overflow<T, N, Base::radix>(), T, lexy::unbounded<T>>;
    return _int_c<_ndigits_s<N, Base, Sep>>{} + _int_p<type, Base, true, void>{};
}
} // namespace lexyd

namespace lexy
{
struct invalid_code_point
{
    static LEXY_CONSTEVAL auto name()
    {
        return "invalid code point";
    }
};
} // namespace lexy

namespace lexyd
{
/// Matches the number of a code point.
template <std::size_t N, typename Base = hex>
constexpr auto code_point_id = [] {
    using type = std::conditional_t<_ndigits_can_overflow<lexy::code_point, N, Base::radix>(),
                                    lexy::code_point, lexy::unbounded<lexy::code_point>>;

    auto digits = _int_c<_ndigits<N, Base>>{};
    auto parser = _int_p<type, Base, true, lexy::invalid_code_point>{};
    return digits + parser;
}();
} // namespace lexyd

#endif // LEXY_DSL_INTEGER_HPP_INCLUDED

