// INPUT:Hello World!
struct name
{
    static constexpr auto rule
        // One or more alpha numeric characters, underscores or hyphens.
        = dsl::while_one(dsl::ascii::alnum / dsl::lit_c<'_'> / dsl::lit_c<'-'>);
};

struct production
{
    // Allow arbitrary spaces between individual tokens.
    // Note that this includes the individual characters of the name.
    static constexpr auto whitespace = dsl::ascii::space;

    static constexpr auto rule = [] {
        auto greeting = LEXY_LIT("Hello");
        return greeting + dsl::inline_<name> + dsl::exclamation_mark + dsl::eof;
    }();
};
