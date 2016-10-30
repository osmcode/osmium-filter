
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <boost/bind.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/optional.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_grammar.hpp>

#include <osmium/osm/item_type.hpp>

#include "object_filter.hpp"


namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template <typename Iterator>
struct comment_skipper : public qi::grammar<Iterator> {

    qi::rule<Iterator> skip;

    comment_skipper() : comment_skipper::base_type(skip, "Comment skipper") {
        skip = ascii::space
             | ('#' >> *(qi::char_ - '\n') >> ('\n' | qi::eoi));
    }

};

template <typename Iterator>
struct OSMObjectFilterGrammar : qi::grammar<Iterator, comment_skipper<Iterator>, expr_node<ExprNode>()> {

    template <typename... T>
    using rs = qi::rule<Iterator, comment_skipper<Iterator>, T...>;

    rs<std::string()> single_q_str, double_q_str, plain_string, string, list_from_filename;
    rs<integer_op_type> oper_int;
    rs<string_op_type> oper_str, oper_regex;
    rs<list_op_type> oper_list;
    rs<std::vector<std::int64_t>()> int_list_value;

    rs<expr_node<IntegerAttribute>()> attr_int;
    rs<expr_node<StringAttribute>()> attr_str;
    rs<expr_node<BooleanAttribute>()> attr_boolean;

    rs<expr_node<ExprNode>()> start_rule, paren_expression, factor, tag, primitive, subexpression, subexpr_int;
    rs<std::vector<expr_node<ExprNode>>()> expression_v, term_v;
    rs<expr_node<OrExpr>()> expression;
    rs<expr_node<AndExpr>()> term;
    rs<expr_node<NotExpr>()> not_factor;
    rs<expr_node<BooleanValue>()> bool_true, bool_false;
    rs<expr_node<IntegerValue>()> int_value;
    rs<expr_node<CheckHasKeyExpr>()> key;
    rs<expr_node<StringValue>()> str_value;
    rs<expr_node<RegexValue>()> regex_value;
    rs<expr_node<TagsExpr>()> tags_expr;
    rs<expr_node<NodesExpr>()> nodes_expr;
    rs<expr_node<MembersExpr>()> members_expr;

    rs<std::tuple<expr_node<ExprNode>, integer_op_type, expr_node<ExprNode>>()> binary_int_oper_v;
    rs<std::tuple<expr_node<ExprNode>, string_op_type, expr_node<ExprNode>>()> binary_str_oper_v;
    rs<expr_node<BinaryIntOperation>()> binary_int_oper;
    rs<expr_node<BinaryStrOperation>()> binary_str_oper;

    rs<std::tuple<expr_node<ExprNode>, list_op_type, std::vector<std::int64_t>>()> in_int_list_values_v;
    rs<std::tuple<expr_node<ExprNode>, list_op_type, std::string>()> in_int_list_filename_v;
    rs<expr_node<InIntegerList>()> in_int_list_values;
    rs<expr_node<InIntegerList>()> in_int_list_filename;

    rs<std::tuple<std::string, string_op_type, std::string>()> tag_str_v;
    rs<std::tuple<std::string, string_op_type, std::string, boost::optional<char>>()> tag_regex_v;
    rs<expr_node<CheckTagStrExpr>()> tag_str;
    rs<expr_node<CheckTagRegexExpr>()> tag_regex;

    OSMObjectFilterGrammar() :
        OSMObjectFilterGrammar::base_type(start_rule, "OSM Object Filter Grammar") {

        // single quoted string
        single_q_str   = qi::lit('\'')
                       > *(~qi::char_('\''))
                       > qi::lit('\'');
        single_q_str.name("single quoted string");

        // double quoted string (XXX TODO: escapes, unicode)
        double_q_str   = qi::lit('"')
                       > *(~qi::char_('"'))
                       > qi::lit('"');
        double_q_str.name("double quoted string");

        // plain string as used in keys and values
        plain_string   =   qi::char_("a-zA-Z")
                       >> *qi::char_("a-zA-Z0-9:_");
        plain_string.name("plain string");

        // any kind of string
        string         = plain_string
                       | single_q_str
                       | double_q_str;
        string.name("string");

        // operator for integer comparison
        oper_int       = (qi::lit("==") > qi::attr(integer_op_type::equal))
                       | (qi::lit("!=") > qi::attr(integer_op_type::not_equal))
                       | (qi::lit("<=") > qi::attr(integer_op_type::less_or_equal))
                       | (qi::lit("<")  > qi::attr(integer_op_type::less_than))
                       | (qi::lit(">=") > qi::attr(integer_op_type::greater_or_equal))
                       | (qi::lit(">")  > qi::attr(integer_op_type::greater_than));
        oper_int.name("integer comparison operand");

        // operator for simple string comparison
        oper_str       = (qi::lit("==") > qi::attr(string_op_type::equal))
                       | (qi::lit("!=") > qi::attr(string_op_type::not_equal))
                       | (qi::lit("=^") > qi::attr(string_op_type::prefix_equal))
                       | (qi::lit("!^") > qi::attr(string_op_type::prefix_not_equal));
        oper_str.name("string comparison operand");

        // operator for regex string comparison
        oper_regex     = (qi::lit("=~") > qi::attr(string_op_type::match))
                       | (qi::lit("!~") > qi::attr(string_op_type::not_match));
        oper_regex.name("string regex comparison operand");

        oper_list      = (qi::lit("in")     > qi::attr(list_op_type::in))
                       | (qi::lit("not in") > qi::attr(list_op_type::not_in));
        oper_list.name("list comparison operand");

        // IntegerAttribute
        attr_int       = (qi::lit("@id")        > qi::attr(integer_attribute_type::id))
                       | (qi::lit("@version")   > qi::attr(integer_attribute_type::version))
                       | (qi::lit("@uid")       > qi::attr(integer_attribute_type::uid))
                       | (qi::lit("@changeset") > qi::attr(integer_attribute_type::changeset))
                       | (qi::lit("@ref")       > qi::attr(integer_attribute_type::ref));
        attr_int.name("integer attribute");

        // StringAttribute
        attr_str       = (qi::lit("@user")  > qi::attr(string_attribute_type::user))
                       | (qi::lit("@key")   > qi::attr(string_attribute_type::key))
                       | (qi::lit("@value") > qi::attr(string_attribute_type::value))
                       | (qi::lit("@role")  > qi::attr(string_attribute_type::role));
        attr_str.name("string attribute");

        // BooleanAttribute
        attr_boolean   = (qi::lit("@node")       > qi::attr(boolean_attribute_type::node))
                       | (qi::lit("@way")        > qi::attr(boolean_attribute_type::way))
                       | (qi::lit("@relation")   > qi::attr(boolean_attribute_type::relation))
                       | (qi::lit("@visible")    > qi::attr(boolean_attribute_type::visible))
                       | (qi::lit("@closed_way") > qi::attr(boolean_attribute_type::closed_way))
                       | (qi::lit("@open_way")   > qi::attr(boolean_attribute_type::open_way));
        attr_boolean.name("boolean attribute");

        // BooleanValue
        bool_true      = qi::lit("true")  > qi::attr(true);
        bool_true.name("true");

        bool_false     = qi::lit("false") > qi::attr(false);
        bool_true.name("false");

        // IntegerValue
        int_value      = qi::int_parser<std::int64_t>();
        int_value.name("integer value");

        // StringValue
        str_value      = string;
        str_value.name("string value");

        // RegexValue
        regex_value    = string;
        regex_value.name("regex value");

        // CheckHasKeyExpr
        key            = string;
        key.name("tag key");

        // CheckTagStrExpr
        tag_str_v      = string
                       >> oper_str
                       >> string;
        tag_str_v.name("tag_str_v");

        tag_str        = tag_str_v;
        tag_str.name("tag_str");

        // CheckTagRegexExpr
        tag_regex_v    = string
                       >> oper_regex
                       >> string
                       >> -ascii::char_('i');
        tag_regex_v.name("tag_regex_v");

        tag_regex      = tag_regex_v;
        tag_regex.name("tag_regex");

        // Tag check
        tag            = tag_str | tag_regex;
        tag.name("tag");

        subexpression  = qi::lit('[') > expression > qi::lit(']');
        subexpression.name("subexpression");

        // TagsExpr
        tags_expr      = qi::lit("@tags") >> subexpression;
        tags_expr.name("tags expression");

        // NodesExpr
        nodes_expr     = qi::lit("@nodes") >> subexpression;
        nodes_expr.name("nodes expression");

        // MembersExpr
        members_expr   = qi::lit("@members") >> subexpression;
        members_expr.name("members expression");

        int_list_value   = qi::lit("(")
                         >> (qi::int_parser<std::int64_t>() % qi::lit(","))
                         >> qi::lit(")");
        int_list_value.name("int_list_value");

        list_from_filename     = qi::lit("(")
                               >> qi::lit("<")
                               >> string
                               >> qi::lit(")");
        list_from_filename.name("list_from_filename");

        in_int_list_values_v   = attr_int >> oper_list >> int_list_value;
        in_int_list_values_v.name("in_int_list_values_v");

        in_int_list_values     = in_int_list_values_v;
        in_int_list_values.name("in_int_list_values");

        in_int_list_filename_v = attr_int >> oper_list >> list_from_filename;
        in_int_list_filename_v.name("in_int_list_filename_v");

        in_int_list_filename   = in_int_list_filename_v;
        in_int_list_filename.name("in_int_list_filename");

        subexpr_int      = tags_expr
                         | nodes_expr
                         | members_expr;
        subexpr_int.name("subexpr_int");

        // an attribute name, comparison operator and integer
        binary_int_oper_v  = (attr_int | int_value | subexpr_int)
                           >> oper_int
                           >> (attr_int | int_value | subexpr_int);
        binary_int_oper_v.name("binary_int_oper_v");

        binary_int_oper  = binary_int_oper_v;
        binary_int_oper.name("binary_int_oper");

        binary_str_oper_v = (attr_str >> oper_str >> str_value) | (attr_str >> oper_regex >> regex_value);
        binary_str_oper_v.name("binary_str_oper_v");

        binary_str_oper  = binary_str_oper_v;
        binary_str_oper.name("binary_str_oper");

        primitive        = bool_true
                         | bool_false
                         | attr_boolean
                         | tag
                         | key
                         | binary_int_oper
                         | binary_str_oper
                         | in_int_list_values
                         | in_int_list_filename;
        primitive.name("condition");

        paren_expression = '('
                         > expression
                         > ')';
        paren_expression.name("parenthesized expression");

        not_factor       = qi::lit("not")
                         > factor;
        not_factor.name("not_factor");

        factor           = not_factor
                         | paren_expression
                         | primitive;
        factor.name("factor");

        term_v           = factor % qi::lit("and");
        term_v.name("term_v");

        term             = term_v;
        term.name("term");

        expression_v     = term % qi::lit("or");
        expression_v.name("expression_v");

        expression       = expression_v;
        expression.name("expression");

        start_rule       = expression;
        start_rule.name("start_rule");

        qi::on_error<qi::fail>(start_rule,
            std::cerr
                << boost::phoenix::val("ERROR: Expecting ")
                << boost::spirit::_4
                << boost::phoenix::val(" here: \"")
                << boost::phoenix::construct<std::string>(boost::spirit::_3, boost::spirit::_2)
                << boost::phoenix::val("\"\n")
        );

    }

}; // struct OSMObjectFilterGrammar

OSMObjectFilter::OSMObjectFilter(const std::string& input) {
    comment_skipper<std::string::const_iterator> skip;

    OSMObjectFilterGrammar<std::string::const_iterator> grammar;

    expr_node<ExprNode> root;
    auto first = input.cbegin();
    auto last  = input.cend();
    const bool result = qi::phrase_parse(
        first,
        last,
        grammar,
        skip,
        root
    );

    if (!result || first != last) {
        throw std::runtime_error{"Can not parse expression"};
    }

    assert(root.get());
    m_root.reset(root.release());
}

