
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

    rs<std::string()> single_q_str, double_q_str, plain_string, string;
    rs<integer_op_type> oper_int;
    rs<string_op_type> oper_str, oper_regex;
    rs<std::vector<std::int64_t>()> int_list_value;

    rs<expr_node<IntegerAttribute>()> attr_int;
    rs<expr_node<StringAttribute>()> attr_str;

    rs<expr_node<ExprNode>()> start_rule, paren_expression, factor, tag, primitive, subexpression, subexpr_int;
    rs<std::vector<expr_node<ExprNode>>()> expression_v, term_v;
    rs<expr_node<OrExpr>()> expression;
    rs<expr_node<AndExpr>()> term;
    rs<expr_node<NotExpr>()> not_factor;
    rs<expr_node<ClosedWayExpr>()> closed_way;
    rs<expr_node<BoolValue>()> bool_true, bool_false;
    rs<expr_node<IntegerValue>()> int_value;
    rs<expr_node<CheckHasKeyExpr>()> key;
    rs<expr_node<StringValue>()> str_value;
    rs<expr_node<RegexValue>()> regex_value;
    rs<expr_node<TagsExpr>()> tags_expr;
    rs<expr_node<NodesExpr>()> nodes_expr;
    rs<expr_node<MembersExpr>()> members_expr;
    rs<expr_node<CheckObjectTypeExpr>()> attr_type, object_type;
    rs<std::tuple<expr_node<ExprNode>, std::vector<std::int64_t>>()> in_int_list_values_tuple;
    rs<std::tuple<expr_node<ExprNode>, std::string>()> in_int_list_filename_tuple;
    rs<expr_node<InIntegerList>()> in_int_list_values;
    rs<expr_node<InIntegerList>()> in_int_list_filename;
    rs<std::tuple<std::string, string_op_type, std::string>()> key_oper_str_value;
    rs<std::tuple<std::string, string_op_type, std::string, boost::optional<char>>()> key_oper_regex_value;
    rs<std::tuple<expr_node<ExprNode>, integer_op_type, expr_node<ExprNode>>()> binary_int_oper_tuple;
    rs<std::tuple<expr_node<ExprNode>, string_op_type, expr_node<ExprNode>>()> binary_str_oper_tuple;
    rs<expr_node<BinaryIntOperation>()> binary_int_oper;
    rs<expr_node<BinaryStrOperation>()> binary_str_oper;
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
        oper_int       = (qi::lit("=")  > qi::attr(integer_op_type::equal))
                       | (qi::lit("!=") > qi::attr(integer_op_type::not_equal))
                       | (qi::lit("<=") > qi::attr(integer_op_type::less_or_equal))
                       | (qi::lit("<")  > qi::attr(integer_op_type::less_than))
                       | (qi::lit(">=") > qi::attr(integer_op_type::greater_or_equal))
                       | (qi::lit(">")  > qi::attr(integer_op_type::greater_than));
        oper_int.name("integer comparison operand");

        // operator for simple string comparison
        oper_str       = (qi::lit("=")   > qi::attr(string_op_type::equal))
                       | (qi::lit("!=")  > qi::attr(string_op_type::not_equal))
                       | (qi::lit("^=")  > qi::attr(string_op_type::prefix_equal))
                       | (qi::lit("!^=") > qi::attr(string_op_type::prefix_not_equal));
        oper_str.name("string comparison operand");

        // operator for regex string comparison
        oper_regex     = (qi::lit("~")  > qi::attr(string_op_type::match))
                       | (qi::lit("!~") > qi::attr(string_op_type::not_match));
        oper_regex.name("string regex comparison operand");

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

        // BoolValue
        bool_true      = qi::lit("true")  > qi::attr(true);
        bool_false     = qi::lit("false") > qi::attr(false);

        // IntegerValue
        int_value      = qi::int_parser<std::int64_t>();
        int_value.name("integer value");

        // StringValue
        str_value      = string;
        str_value.name("string value");

        // RegexValue
        regex_value    = string;
        regex_value.name("regex value");

        // CheckObjectTypeExpr
        object_type    = (qi::lit("node")     > qi::attr(osmium::item_type::node))
                       | (qi::lit("way")      > qi::attr(osmium::item_type::way))
                       | (qi::lit("relation") > qi::attr(osmium::item_type::relation));
        object_type.name("object type");

        // CheckObjectTypeExpr
        attr_type      = qi::lit("@type")
                       > '='
                       > object_type;
        attr_type.name("object type");

        // CheckHasKeyExpr
        key            = string;
        key.name("tag key");

        // ClosedWayExpr
        closed_way     = qi::lit("closed_way") > qi::attr(unused());

        // a tag (key operator value)
        key_oper_str_value =  string
                           >> oper_str
                           >> string;
        key_oper_str_value.name("key_oper_str_value");

        key_oper_regex_value =  string
                             >> oper_regex
                             >> string
                             >> -ascii::char_('i');
        key_oper_regex_value.name("key_oper_regex_value");


        tag_str = key_oper_str_value;
        tag_regex = key_oper_regex_value;

        // a tag
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

        in_int_list_values_tuple = attr_int >> qi::lit("in") >> int_list_value;
        in_int_list_values = in_int_list_values_tuple;

        in_int_list_filename_tuple = attr_int >> qi::lit("in") >> string;
        in_int_list_filename = in_int_list_filename_tuple;

        subexpr_int      = tags_expr
                         | nodes_expr
                         | members_expr;

        // an attribute name, comparison operator and integer
        binary_int_oper_tuple  = (attr_int | int_value | subexpr_int)
                         >> oper_int
                         >> (attr_int | int_value | subexpr_int);
        binary_int_oper  = binary_int_oper_tuple;
        binary_int_oper.name("binary_int_oper");

        binary_str_oper_tuple = (attr_str >> oper_str >> str_value) | (attr_str >> oper_regex >> regex_value);

        binary_str_oper  = binary_str_oper_tuple;
        binary_str_oper.name("binary_str_oper");

        primitive        = bool_true
                         | bool_false
                         | object_type
                         | closed_way
                         | tag
                         | key
                         | attr_type
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

        factor           = not_factor
                         | paren_expression
                         | primitive;
        factor.name("factor");

        term_v           = factor % qi::lit("and");
        term             = term_v;
        term.name("term");

        expression_v     = term % qi::lit("or");
        expression       = expression_v;
        expression.name("expression");

        start_rule       = expression;

        qi::on_error<qi::fail>(expression,
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

