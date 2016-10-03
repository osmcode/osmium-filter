
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <boost/bind.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_grammar.hpp>
#include <boost/optional.hpp>

#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>

#include <osmium/osm.hpp>

#include "object_filter.hpp"


namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template <typename Iterator>
struct comment_skipper : public qi::grammar<Iterator> {

    qi::rule<Iterator> skip;

    comment_skipper() : comment_skipper::base_type(skip, "Comment skipper") {
        skip = ascii::space
             | ('#' >> *(qi::char_ - '\n') >> '\n');
    }

};

ExprNode* boolean_and(const boost::fusion::vector<std::vector<ExprNode*>>& e) {
    auto expressions = boost::fusion::at_c<0>(e);
    assert(!expressions.empty());
    if (expressions.size() == 1) {
        return expressions[0];
    } else {
        return new AndExpr(expressions);
    }
}

ExprNode* boolean_or(const boost::fusion::vector<std::vector<ExprNode*>>& e) {
    auto expressions = boost::fusion::at_c<0>(e);
    assert(!expressions.empty());
    if (expressions.size() == 1) {
        return expressions[0];
    } else {
        return new OrExpr(expressions);
    }
}

ExprNode* boolean_not(const boost::fusion::vector<ExprNode*>& e) {
    auto expression = boost::fusion::at_c<0>(e);
    return new NotExpr(expression);
}

ExprNode* check_has_key_expr(const boost::fusion::vector<std::string>& e) {
    auto key = boost::fusion::at_c<0>(e);
    return new CheckHasKeyExpr(key);
}

ExprNode* check_tag_str_expr(const boost::fusion::vector<std::tuple<std::string, std::string, std::string>>& e) {
    auto key   = std::get<0>(boost::fusion::at_c<0>(e));
    auto oper  = std::get<1>(boost::fusion::at_c<0>(e));
    auto value = std::get<2>(boost::fusion::at_c<0>(e));
    return new CheckTagStrExpr(key, oper, value);
}

ExprNode* check_tag_regex_expr(const boost::fusion::vector<std::tuple<std::string, std::string, std::string, boost::optional<char>>>& e) {
    auto key   = std::get<0>(boost::fusion::at_c<0>(e));
    auto oper  = std::get<1>(boost::fusion::at_c<0>(e));
    auto value = std::get<2>(boost::fusion::at_c<0>(e));
    auto cins  = std::get<3>(boost::fusion::at_c<0>(e));
    return new CheckTagRegexExpr(key, oper, value, cins);
}

ExprNode* check_object_type_expr(const boost::fusion::vector<std::string>& e) {
    auto type = boost::fusion::at_c<0>(e);
    return new CheckObjectTypeExpr(type);
}

ExprNode* check_attr_int_expr(const boost::fusion::vector<std::tuple<std::string, std::string, int64_t>>& e) {
    auto key   = std::get<0>(boost::fusion::at_c<0>(e));
    auto oper  = std::get<1>(boost::fusion::at_c<0>(e));
    auto value = std::get<2>(boost::fusion::at_c<0>(e));
    return new CheckAttrIntExpr(key, oper, value);
}

ExprNode* check_id_expr(const boost::fusion::vector<int64_t>& e) {
    auto value = boost::fusion::at_c<0>(e);
    return new CheckAttrIntExpr("@id", "=", value);
}

template <typename Iterator>
struct OSMObjectFilterGrammar : qi::grammar<Iterator, comment_skipper<Iterator>, ExprNode*()> {

    template <typename... T>
    using rs = qi::rule<Iterator, comment_skipper<Iterator>, T...>;

    rs<ExprNode*()> expression, paren_expression, factor, tag, primitive, key, attr, term;
    rs<std::string()> single_q_str, double_q_str, plain_string, string, oper_int, oper_str, oper_regex, attr_int, object_type, attr_type;
    rs<std::tuple<std::string, std::string, std::string>()> key_oper_str_value;
    rs<std::tuple<std::string, std::string, std::string, boost::optional<char>>()> key_oper_regex_value;
    rs<std::tuple<std::string, std::string, int64_t>()> attr_oper_int;

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
        oper_int       = ascii::string("=")
                       | ascii::string(">=")
                       | ascii::string("<=")
                       | ascii::string("<")
                       | ascii::string(">")
                       | ascii::string("!=");
        oper_int.name("integer comparison operand");

        // operator for simple string comparison
        oper_str       = ascii::string("=")
                       | ascii::string("!=");
        oper_str.name("string comparison operand");

        // operator for regex string comparison
        oper_regex     = ascii::string("~")
                       | ascii::string("=~")
                       | ascii::string("!~");
        oper_regex.name("string comparison operand");

        // a tag key
        key            = string[qi::_val = boost::phoenix::bind(&check_has_key_expr, _1)];
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
        tag            = key_oper_str_value[qi::_val = boost::phoenix::bind(&check_tag_str_expr, _1)]
                       | key_oper_regex_value[qi::_val = boost::phoenix::bind(&check_tag_regex_expr, _1)];
        tag.name("tag");

        // attributes of type int
        attr_int       = ascii::string("@id")
                       | ascii::string("@version")
                       | ascii::string("@uid")
                       | ascii::string("@changeset")
                       | ascii::string("@nodes")
                       | ascii::string("@members")
                       | ascii::string("@tags");
        attr_int.name("attribute");

        // an attribute name, comparison operator and integer
        attr_oper_int  = attr_int
                       > oper_int
                       > qi::long_;
        attr_oper_int.name("attr_oper_int");

        // name of OSM object type
        object_type    = ascii::string("node")
                       | ascii::string("way")
                       | ascii::string("relation");
        object_type.name("object type");

        // OSM object type as attribute
        attr_type      = qi::lit("@type")
                       > '='
                       > object_type;
        attr_type.name("attr_type");

        // attribute expression
        attr           = attr_type[qi::_val = boost::phoenix::bind(&check_object_type_expr, _1)]
                       | attr_oper_int[qi::_val = boost::phoenix::bind(&check_attr_int_expr, _1)];
        attr.name("attr");

        // primitive expression
        primitive      = object_type[qi::_val = boost::phoenix::bind(&check_object_type_expr, _1)]
                       | qi::long_[qi::_val = boost::phoenix::bind(&check_id_expr, _1)]
                       | tag[qi::_val = qi::_1]
                       | key[qi::_val = qi::_1]
                       | attr[qi::_val = qi::_1];
        primitive.name("condition");

        // boolean logic expressions
        expression      = (term % '|')[qi::_val = boost::phoenix::bind(&boolean_or, _1)];
        expression.name("expression");

        term            = (factor % '&')[qi::_val = boost::phoenix::bind(&boolean_and, _1)];
        term.name("term");

        paren_expression = '('
                         > expression[qi::_val = qi::_1]
                         > ')';
        paren_expression.name("parenthesized expression");

        factor          = ('!' >> factor)[qi::_val = boost::phoenix::bind(&boolean_not, _1)]
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

OSMObjectFilter::OSMObjectFilter(std::string& input) {
    comment_skipper<std::string::iterator> skip;

    OSMObjectFilterGrammar<std::string::iterator> grammar;

    auto first = input.begin();
    auto last = input.end();
    const bool result = qi::phrase_parse(
        first,
        last,
        grammar,
        skip,
        m_root
    );

    if (!result || first != last) {
        throw std::runtime_error{"Can not parse expression"};
    }

    assert(m_root);
}

