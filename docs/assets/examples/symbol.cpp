#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy/parse.hpp>
#include <lexy_ext/cfile.hpp>
#include <lexy_ext/report_error.hpp>

namespace dsl = lexy::dsl;

//{
struct production
{
    // Map names of the entities to their replacement value.
    static constexpr auto entities = lexy::symbol_table<char>
                                         .map<LEXY_SYMBOL("quot")>('"')
                                         .map<LEXY_SYMBOL("amp")>('&')
                                         .map<LEXY_SYMBOL("apos")>('\'')
                                         .map<LEXY_SYMBOL("lt")>('<')
                                         .map<LEXY_SYMBOL("gt")>('>');

    static constexpr auto rule = [] {
        auto name      = dsl::identifier(dsl::ascii::alpha);
        auto reference = dsl::symbol<entities>(name);
        return dsl::lit_c<'&'> >> reference + dsl::lit_c<';'>;
    }();

    static constexpr auto value = lexy::forward<char>;
};
//}

int main()
{
    auto input  = lexy_ext::read_file<>(stdin);
    auto result = lexy::parse<production>(input, lexy_ext::report_error);
    if (!result)
        return 1;

    std::printf("The replacement is: '%c'\n", result.value());
}

