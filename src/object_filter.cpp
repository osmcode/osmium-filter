
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

namespace detail {

    ExprNode* boolean_and(const boost::fusion::vector<std::vector<ExprNode*>>& e) {
        const auto expressions = boost::fusion::at_c<0>(e);
        assert(!expressions.empty());
        if (expressions.size() == 1) {
            return expressions[0];
        } else {
            return new AndExpr{expressions};
        }
    }

    ExprNode* boolean_or(const boost::fusion::vector<std::vector<ExprNode*>>& e) {
        const auto expressions = boost::fusion::at_c<0>(e);
        assert(!expressions.empty());
        if (expressions.size() == 1) {
            return expressions[0];
        } else {
            return new OrExpr{expressions};
        }
    }

}

template <typename Iterator>
struct OSMObjectFilterGrammar : qi::grammar<Iterator, comment_skipper<Iterator>, ExprNode*()> {

    template <typename... T>
    using rs = qi::rule<Iterator, comment_skipper<Iterator>, T...>;

    rs<ExprNode*()> expression, paren_expression, factor, tag, primitive, key, attr, term, int_value, attr_int, str_value, regex_value, attr_str;
    rs<ExprNode*()> subexpression, tags_expr, nodes_expr, members_expr, subexpr_int;
    rs<std::string()> single_q_str, double_q_str, plain_string, string;
    rs<osmium::item_type()> object_type, attr_type;
    rs<std::vector<std::int64_t>()> int_list_value;
    rs<std::tuple<ExprNode*, std::vector<std::int64_t>>()> in_int_list_values;
    rs<std::tuple<ExprNode*, std::string>()> in_int_list_filename;
    rs<integer_op_type> oper_int;
    rs<string_op_type> oper_str;
    rs<string_op_type> oper_regex;
    rs<std::tuple<std::string, string_op_type, std::string>()> key_oper_str_value;
    rs<std::tuple<std::string, string_op_type, std::string, boost::optional<char>>()> key_oper_regex_value;
    rs<std::tuple<ExprNode*, integer_op_type, ExprNode*>()> binary_int_oper;
    rs<std::tuple<ExprNode*, string_op_type, ExprNode*>()> binary_str_oper;

    OSMObjectFilterGrammar() :
        OSMObjectFilterGrammar::base_type(expression, "OSM Object Filter Grammar") {

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

        // a tag key
        key            = string[qi::_val = boost::phoenix::new_<CheckHasKeyExpr>(qi::_1)];
        key.name("tag key");

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

        // a tag
        tag            = key_oper_str_value[qi::_val = boost::phoenix::new_<CheckTagStrExpr>(qi::_1)]
                       | key_oper_regex_value[qi::_val = boost::phoenix::new_<CheckTagRegexExpr>(qi::_1)];
        tag.name("tag");

        attr_int       = ((qi::lit("@id")        > qi::attr(integer_attribute_type::id))
                         |(qi::lit("@version")   > qi::attr(integer_attribute_type::version))
                         |(qi::lit("@uid")       > qi::attr(integer_attribute_type::uid))
                         |(qi::lit("@changeset") > qi::attr(integer_attribute_type::changeset))
                         |(qi::lit("@ref")       > qi::attr(integer_attribute_type::ref))
                         )[qi::_val = boost::phoenix::new_<IntegerAttribute>(qi::_1)];
        attr_int.name("integer attribute");

        attr_str       = ((qi::lit("@user")  > qi::attr(string_attribute_type::user))
                         |(qi::lit("@key")   > qi::attr(string_attribute_type::key))
                         |(qi::lit("@value") > qi::attr(string_attribute_type::value))
                         |(qi::lit("@role")  > qi::attr(string_attribute_type::role))
                         )[qi::_val = boost::phoenix::new_<StringAttribute>(qi::_1)];
        attr_str.name("string attribute");

        subexpression  = qi::lit('[') > expression > qi::lit(']');
        subexpression.name("tags expression");

        tags_expr      = qi::lit("@tags") >> subexpression;
        tags_expr.name("tags expression");

        nodes_expr     = qi::lit("@nodes") >> subexpression;
        nodes_expr.name("nodes expression");

        members_expr   = qi::lit("@members") >> subexpression;
        members_expr.name("members expression");

        int_value      = qi::int_parser<std::int64_t>()[qi::_val = boost::phoenix::new_<IntegerValue>(qi::_1)];
        int_value.name("integer value");

        str_value      = string[qi::_val = boost::phoenix::new_<StringValue>(qi::_1)];
        str_value.name("string value");

        regex_value      = string[qi::_val = boost::phoenix::new_<RegexValue>(qi::_1)];
        regex_value.name("regex value");

        int_list_value = qi::lit("(") >> (qi::int_parser<std::int64_t>() % qi::lit(",")) >> qi::lit(")");

        in_int_list_values = attr_int >> qi::lit("in") >> int_list_value;

        in_int_list_filename = attr_int >> qi::lit("in") >> string;

        subexpr_int    = tags_expr[qi::_val = boost::phoenix::new_<TagsExpr>(qi::_1)]
                       | nodes_expr[qi::_val = boost::phoenix::new_<NodesExpr>(qi::_1)]
                       | members_expr[qi::_val = boost::phoenix::new_<MembersExpr>(qi::_1)];

        // an attribute name, comparison operator and integer
        binary_int_oper  = (attr_int | int_value | subexpr_int)
                         >> oper_int
                         >> (attr_int | int_value | subexpr_int);
        binary_int_oper.name("binary_int_oper");

        binary_str_oper  = (attr_str >> oper_str >> str_value) |
                           (attr_str >> oper_regex >> regex_value);
        binary_str_oper.name("binary_str_oper");

        // name of OSM object type
        object_type    = (qi::lit("node")     > qi::attr(osmium::item_type::node))
                       | (qi::lit("way")      > qi::attr(osmium::item_type::way))
                       | (qi::lit("relation") > qi::attr(osmium::item_type::relation));
        object_type.name("object type");

        // OSM object type as attribute
        attr_type      = qi::lit("@type")
                       > '='
                       > object_type;
        attr_type.name("attr_type");

        // attribute expression
        attr           = attr_type[qi::_val = boost::phoenix::new_<CheckObjectTypeExpr>(qi::_1)]
                       | binary_int_oper[qi::_val = boost::phoenix::new_<BinaryIntOperation>(qi::_1)]
                       | binary_str_oper[qi::_val = boost::phoenix::new_<BinaryStrOperation>(qi::_1)]
                       | in_int_list_values[qi::_val = boost::phoenix::new_<InIntegerList>(qi::_1)]
                       | in_int_list_filename[qi::_val = boost::phoenix::new_<InIntegerList>(qi::_1)];
        attr.name("attr");

        // primitive expression
        primitive      = object_type[qi::_val = boost::phoenix::new_<CheckObjectTypeExpr>(qi::_1)]
                       | qi::lit("closed_way")[qi::_val = boost::phoenix::new_<ClosedWayExpr>()]
                       | qi::lit("true")[qi::_val = boost::phoenix::new_<BoolValue>(true)]
                       | qi::lit("false")[qi::_val = boost::phoenix::new_<BoolValue>(false)]
                       | tag[qi::_val = qi::_1]
                       | key[qi::_val = qi::_1]
                       | attr[qi::_val = qi::_1];
        primitive.name("condition");

        // boolean logic expressions
        expression      = (term % qi::lit("or"))[qi::_val = boost::phoenix::bind(&detail::boolean_or, _1)];
        expression.name("expression");

        term            = (factor % qi::lit("and"))[qi::_val = boost::phoenix::bind(&detail::boolean_and, _1)];
        term.name("term");

        paren_expression = '('
                         > expression[qi::_val = qi::_1]
                         > ')';
        paren_expression.name("parenthesized expression");

        factor          = (qi::lit("not") >> factor)[qi::_val = boost::phoenix::new_<NotExpr>(qi::_1)]
                        | paren_expression[qi::_val = qi::_1]
                        | primitive[qi::_val = qi::_1];
        factor.name("factor");

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

    ExprNode* root = nullptr;
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

    assert(root);
    m_root.reset(root);
}

