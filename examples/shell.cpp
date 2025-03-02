// Copyright (C) 2020-2021 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <map>
#include <memory>
#include <string>

#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/parse.hpp>

#include <lexy_ext/report_error.hpp> // lexy_ext::report_error
#include <lexy_ext/shell.hpp>

// A shell with a couple of basic commands.
namespace shell
{
struct interpreter
{
    // Manages I/O.
    lexy_ext::shell<lexy_ext::default_prompt<lexy::utf8_encoding>> shell;
    // The values of variables.
    std::map<std::string, std::string> variables;

    // Retrieves the value of a variable.
    const std::string& lookup_var(const std::string& name) const
    {
        auto iter = variables.find(name);
        if (iter == variables.end())
        {
            static const std::string not_found;
            return not_found;
        }

        return iter->second;
    }
};

class cmd_base
{
public:
    virtual ~cmd_base() = default;

    // Returns true if the shell should exit.
    virtual bool execute(interpreter& intp) const = 0;
};
using command = std::unique_ptr<cmd_base>;

// Exits the shell.
class cmd_exit : public cmd_base
{
public:
    bool execute(interpreter& intp) const override
    {
        intp.shell.write_message()("Goodbye.");
        return true;
    }
};

// Prints output.
class cmd_echo : public cmd_base
{
public:
    explicit cmd_echo(std::string msg) : _msg(LEXY_MOV(msg)) {}

    bool execute(interpreter& intp) const override
    {
        intp.shell.write_message()(_msg.data(), _msg.size());
        return false;
    }

private:
    std::string _msg;
};

// Sets the value of a variable.
class cmd_set : public cmd_base
{
public:
    explicit cmd_set(std::string name, std::string value)
    : _name(LEXY_MOV(name)), _value(LEXY_MOV(value))
    {}

    bool execute(interpreter& intp) const override
    {
        intp.variables[_name] = _value;
        return false;
    }

private:
    std::string _name;
    std::string _value;
};
} // namespace shell

namespace grammar
{
namespace dsl = lexy::dsl;

// A bare argument or command name.
constexpr auto identifier = dsl::identifier(dsl::ascii::alnum);

//=== The arguments ===//
// The content of a string literal: any unicode code point except for control characters.
constexpr auto str_char = dsl::code_point - dsl::ascii::control;

// An unquoted sequence of characters.
struct arg_bare
{
    static constexpr auto rule  = identifier;
    static constexpr auto value = lexy::as_string<std::string>;
};

// A string without escape characters.
struct arg_string
{
    static constexpr auto rule  = dsl::single_quoted(str_char);
    static constexpr auto value = lexy::as_string<std::string>;
};

// A string with escape characters.
struct arg_quoted
{
    struct interpolation
    {
        static constexpr auto rule = dsl::curly_bracketed(dsl::recurse<struct argument>);
        static constexpr auto value
            = lexy::bind(lexy::callback<std::string>(&shell::interpreter::lookup_var),
                         lexy::parse_state, lexy::values);
    };

    static constexpr auto escaped_sequences = lexy::symbol_table<char> //
                                                  .map<'"'>('"')
                                                  .map<'\\'>('\\')
                                                  .map<'/'>('/')
                                                  .map<'b'>('\b')
                                                  .map<'f'>('\f')
                                                  .map<'n'>('\n')
                                                  .map<'r'>('\r')
                                                  .map<'t'>('\t');

    static constexpr auto rule = [] {
        // C style escape sequences.
        auto backslash_escape = dsl::backslash_escape.symbol<escaped_sequences>();
        // Interpolation.
        auto dollar_escape = dsl::dollar_escape.rule(dsl::p<interpolation>);

        return dsl::quoted(str_char, backslash_escape, dollar_escape);
    }();
    static constexpr auto value = lexy::as_string<std::string>;
};

// An argument that expands to the value of a variable.
struct arg_var
{
    static constexpr auto rule = [] {
        auto bare      = dsl::p<arg_bare>;
        auto bracketed = dsl::curly_bracketed(dsl::recurse<argument>);
        auto name      = bracketed | dsl::else_ >> bare;

        return dsl::dollar_sign >> name;
    }();
    static constexpr auto value
        = lexy::bind(lexy::callback<std::string>(&shell::interpreter::lookup_var),
                     lexy::parse_state, lexy::values);
};

// An argument to a command.
struct argument
{
    struct invalid
    {
        static constexpr auto name = "invalid argument character";
    };

    static constexpr auto rule = dsl::p<arg_string> | dsl::p<arg_quoted> //
                                 | dsl::p<arg_var> | dsl::p<arg_bare>    //
                                 | dsl::error<invalid>;
    static constexpr auto value = lexy::forward<std::string>;
};

// The separator between arguments.
constexpr auto arg_sep = [] {
    struct missing_argument
    {
        static constexpr auto name()
        {
            return "missing argument";
        }
    };

    // It's either blank or a backlash that escapes the newline.
    // We turn it into a token to be able to trigger a single error if it doesn't match.
    // If we used `... | dsl::error<missing_argument>` without the token,
    // the `dsl::if_(arg_sep)` would always take the branch as the choice is now unconditional.
    auto sep = dsl::token(dsl::ascii::blank | dsl::backslash >> dsl::newline);
    return dsl::while_one(sep.error<missing_argument>);
}();

//=== the commands ===//
struct cmd_exit
{
    static constexpr auto rule  = LEXY_KEYWORD("exit", identifier) | dsl::eof;
    static constexpr auto value = lexy::new_<shell::cmd_exit, shell::command>;
};

struct cmd_echo
{
    static constexpr auto rule  = LEXY_KEYWORD("echo", identifier) >> arg_sep + dsl::p<argument>;
    static constexpr auto value = lexy::new_<shell::cmd_echo, shell::command>;
};

struct cmd_set
{
    static constexpr auto rule = LEXY_KEYWORD("set", identifier)
                                 >> arg_sep + dsl::p<argument> + arg_sep + dsl::p<argument>;
    static constexpr auto value = lexy::new_<shell::cmd_set, shell::command>;
};

// Parses one of three commands.
struct command
{
    struct unknown_command
    {
        static constexpr auto name = "unknown command";
    };
    struct trailing_args
    {
        static constexpr auto name = "trailing command arguments";
    };

    static constexpr auto rule = [] {
        auto unknown  = dsl::error<unknown_command>(identifier);
        auto commands = dsl::p<cmd_exit> | dsl::p<cmd_echo> //
                        | dsl::p<cmd_set> | unknown;

        // Allow an optional argument separator after the final command,
        // but then there should not be any other arguments after that.
        return commands + dsl::if_(arg_sep) + dsl::eol.error<trailing_args>;
    }();
    static constexpr auto value = lexy::forward<shell::command>;
};
} // namespace grammar

#ifndef LEXY_TEST
int main()
{
    for (shell::interpreter intp; intp.shell.is_open();)
    {
        // We repeatedly prompt for a new line.
        // Note: everytime we do it, the memory of the previous line is cleared.
        auto input = intp.shell.prompt_for_input();

        // Then we parse the command.
        auto result = lexy::parse<grammar::command>(input, intp, lexy_ext::report_error);
        if (!result)
            continue;

        // ... and execute it.
        auto exit = result.value()->execute(intp);
        if (exit)
            break;
    }
}
#endif

