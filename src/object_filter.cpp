
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <boost/bind.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_grammar.hpp>
#include <boost/optional.hpp>

#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>

#include <osmium/osm.hpp>

#include "object_filter.hpp"

namespace detail {

    int get_type(const osmium::OSMObject& object) noexcept {
        return int(object.type());
    }

    std::int64_t get_id(const osmium::OSMObject& object) noexcept {
        return int64_t(object.id());
    }

    std::int64_t get_uid(const osmium::OSMObject& object) noexcept {
        return int64_t(object.uid());
    }

    std::int64_t get_version(const osmium::OSMObject& object) noexcept {
        return int64_t(object.version());
    }

    std::int64_t get_changeset(const osmium::OSMObject& object) noexcept {
        return int64_t(object.changeset());
    }

    std::int64_t get_count_tags(const osmium::OSMObject& object) noexcept {
        return int64_t(object.tags().size());
    }

    std::int64_t get_count_nodes(const osmium::OSMObject& object) noexcept {
        if (object.type() != osmium::item_type::way) {
            return 0;
        }
        return int64_t(static_cast<const osmium::Way&>(object).nodes().size());
    }

    std::int64_t get_count_members(const osmium::OSMObject& object) noexcept {
        if (object.type() != osmium::item_type::relation) {
            return 0;
        }
        return int64_t(static_cast<const osmium::Relation&>(object).members().size());
    }

    bool has_key(const osmium::OSMObject& object, const Context* context, size_t key) {
        return object.tags().has_key(context->strings[key].c_str());
    }

    bool check_tag_equals(const osmium::OSMObject& object, const Context* context, size_t key, size_t value) noexcept {
        const char* tag_value = object.tags().get_value_by_key(context->strings[key].c_str());
        return tag_value && !std::strcmp(context->strings[value].c_str(), tag_value);
    }

    bool check_tag_not_equals(const osmium::OSMObject& object, const Context* context, size_t key, size_t value) noexcept {
        const char* tag_value = object.tags().get_value_by_key(context->strings[key].c_str());
        return tag_value && std::strcmp(context->strings[value].c_str(), tag_value);
    }

    bool check_tag_match(const osmium::OSMObject& object, const Context* context, size_t key, size_t value) noexcept {
        const char* tag_value = object.tags().get_value_by_key(context->strings[key].c_str());
        return tag_value && std::regex_search(tag_value, context->regexes[value]);
    }

    bool check_tag_not_match(const osmium::OSMObject& object, const Context* context, size_t key, size_t value) noexcept {
        const char* tag_value = object.tags().get_value_by_key(context->strings[key].c_str());
        return tag_value && !std::regex_search(tag_value, context->regexes[value]);
    }

} // namespace detail

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

template <typename Iterator>
struct OSMObjectFilterGrammar : qi::grammar<Iterator, comment_skipper<Iterator>> {

    template <typename... T>
    using rs = qi::rule<Iterator, comment_skipper<Iterator>, T...>;

    rs<> expression, paren_expression, term, factor, tag, primitive, key, attr_type, attr;
    rs<std::string()> single_q_str, double_q_str, clean_string, string, oper_int, oper_str, oper_regex, attr_int, object_type;
    rs<std::tuple<std::string, std::string, std::string>()> key_oper_str_value;
    rs<std::tuple<std::string, std::string, std::string, boost::optional<char>>()> key_oper_regex_value;
    rs<std::tuple<std::string, std::string, int64_t>()> attr_oper_int;

    OSMObjectFilter* m_filter;

    explicit OSMObjectFilterGrammar(OSMObjectFilter* filter) :
        OSMObjectFilterGrammar::base_type(expression, "OSM Object Filter Grammar"),
        m_filter(filter) {

        // single quoted string
        single_q_str   = qi::lit('\'')
                       > *(~qi::char_('\''))
                       > qi::lit('\'');

        // double quoted string (XXX TODO: escapes, unicode)
        double_q_str   = qi::lit('"')
                       > *(~qi::char_('"'))
                       > qi::lit('"');

        // simple string as used in keys and values
        clean_string   =   qi::char_("a-zA-Z")
                       >> *qi::char_("a-zA-Z0-9:_");

        // any kind of string
        string         = clean_string
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
        key            = string[boost::bind(&OSMObjectFilter::check_has_key, m_filter, _1)];
        key.name("tag key");

        // a tag (key operator value)
        key_oper_str_value =  string
                           >> oper_str
                           >> string;

        key_oper_regex_value =  string
                             >> oper_regex
                             >> string
                             >> -ascii::char_('i');

        // a tag
        tag            = key_oper_str_value[boost::bind(&OSMObjectFilter::check_tag_str, m_filter, _1)]
                       | key_oper_regex_value[boost::bind(&OSMObjectFilter::check_tag_regex, m_filter, _1)];

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
//        attr_oper_int.name("attribute name");

        // name of OSM object type
        object_type    = ascii::string("node")
                       | ascii::string("way")
                       | ascii::string("relation");
        object_type.name("object type");

        // OSM object type as attribute
        attr_type      = qi::lit("@type")
                       > '='
                       > object_type[boost::bind(&OSMObjectFilter::check_object_type, m_filter, _1)];
        attr_type.name("attr_type");

        // attribute expression
        attr           = (attr_type | attr_oper_int[boost::bind(&OSMObjectFilter::check_attr_int, m_filter, _1)]);
        attr.name("attr");

        // primitive expression
        primitive      = object_type[boost::bind(&OSMObjectFilter::check_object_type, m_filter, _1)]
                       | qi::long_[boost::bind(&OSMObjectFilter::check_id, m_filter, _1)]
                       | tag
                       | key
                       | attr;
        attr.name("condition");

        // boolean logic expressions
        expression      = term
                        > *( '|' >> term )[boost::bind(&OSMObjectFilter::boolean_or, m_filter)];
        expression.name("expression");

        term            = factor
                        > *( -qi::lit("&") >> factor )[boost::bind(&OSMObjectFilter::boolean_and, m_filter)];
        term.name("term");

        paren_expression = '('
                         > expression
                         > ')';
        paren_expression.name("parenthesized expression");

        factor          = ('!' >> factor)[boost::bind(&OSMObjectFilter::boolean_not, m_filter)]
                        | paren_expression
                        | primitive;
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


void OSMObjectFilter::boolean_and() {
    if (m_verbose) {
        std::cerr << "AND\n";
    }

    assert(m_expression_stack.size() >= 2);
    auto* e1 = m_expression_stack.top();
    m_expression_stack.pop();
    auto* e2 = m_expression_stack.top();
    m_expression_stack.pop();

    m_expression_stack.push(&m_expression.And(*e1, *e2));
}

void OSMObjectFilter::boolean_or() {
    if (m_verbose) {
        std::cerr << "OR\n";
    }

    assert(m_expression_stack.size() >= 2);
    auto* e1 = m_expression_stack.top();
    m_expression_stack.pop();
    auto* e2 = m_expression_stack.top();
    m_expression_stack.pop();

    m_expression_stack.push(&m_expression.Or(*e1, *e2));
}

void OSMObjectFilter::boolean_not() {
    if (m_verbose) {
        std::cerr << "NOT\n";
    }

    assert(m_expression_stack.size() >= 1);
    auto* e = m_expression_stack.top();
    m_expression_stack.pop();

    m_expression_stack.push(&m_expression.If(*e, m_expression.Immediate(false), m_expression.Immediate(true)));
}

void OSMObjectFilter::check_has_key(const std::string& str) {
    if (m_verbose) {
        std::cerr << "HAS KEY [\"" << str << "\"]\n";
    }

    auto& func = m_expression.Immediate(detail::has_key);

    auto& call = m_expression.Call(func,
        m_expression.GetP1(),
        m_expression.Immediate(context()),
        m_expression.Immediate(m_context.strings.size())
    );

    m_expression_stack.push(&call);

    m_context.strings.push_back(str);
}

void OSMObjectFilter::check_tag_str(const std::tuple<std::string, std::string, std::string>& data) {
    const std::string& key   = std::get<0>(data);
    const std::string& oper  = std::get<1>(data);
    const std::string& value = std::get<2>(data);

    if (m_verbose) {
        std::cerr << "HAS TAG [\"" << key << "\" " << oper << " \"" << value << "\"]\n";
    }

    assert(!oper.empty());
    auto& func = m_expression.Immediate(
        oper[0] == '!' ? detail::check_tag_not_equals
                       : detail::check_tag_equals
    );

    auto& call = m_expression.Call(func,
        m_expression.GetP1(),
        m_expression.Immediate(context()),
        m_expression.Immediate(m_context.strings.size()),
        m_expression.Immediate(m_context.strings.size() + 1)
    );

    m_expression_stack.push(&call);

    m_context.strings.push_back(key);
    m_context.strings.push_back(value);
}

void OSMObjectFilter::check_tag_regex(const std::tuple<std::string, std::string, std::string, boost::optional<char>>& data) {
    const std::string& key   = std::get<0>(data);
    const std::string& oper  = std::get<1>(data);
    const std::string& value = std::get<2>(data);

    bool case_insensitive    = std::get<3>(data) == 'i';

    if (m_verbose) {
        std::cerr << "HAS TAG [\"" << key << "\" " << oper << " \"" << value << "\" " << (case_insensitive ? "(case-insensitive)" : "(case-sensitive)") << "]\n";
    }

    assert(!oper.empty());
    auto& func = m_expression.Immediate(
        oper[0] == '!' ? detail::check_tag_not_match
                       : detail::check_tag_match
    );

    auto options = std::regex::nosubs | std::regex::optimize;
    if (case_insensitive) {
        options |= std::regex::icase;
    }

    auto& call = m_expression.Call(func,
        m_expression.GetP1(),
        m_expression.Immediate(context()),
        m_expression.Immediate(m_context.strings.size()),
        m_expression.Immediate(m_context.regexes.size())
    );

    m_expression_stack.push(&call);

    m_context.strings.push_back(key);
    m_context.regexes.emplace_back(value, options);
}

void OSMObjectFilter::check_attr_int(const std::tuple<std::string, std::string, int64_t>& data) {
    const std::string& attr = std::get<0>(data);
    const std::string& oper = std::get<1>(data);
    const int64_t value     = std::get<2>(data);

    if (m_verbose) {
        std::cerr << "ATTR [" << attr << " " << oper << " " << value << "]\n";
    }

    auto& func = m_expression.Immediate(
        attr == "@id"        ? detail::get_id :
        attr == "@uid"       ? detail::get_uid :
        attr == "@changeset" ? detail::get_changeset :
        attr == "@version"   ? detail::get_version :
        attr == "@nodes"     ? detail::get_count_nodes :
        attr == "@members"   ? detail::get_count_members :
        attr == "@tags"      ? detail::get_count_tags :
                               0
    );

    auto& call = m_expression.Call(func, m_expression.GetP1());

    NativeJIT::Node<bool>* cond;

    if (oper == "=") {
        auto& compare = m_expression.Compare<NativeJIT::JccType::JE>(call, m_expression.Immediate(value));
        cond = &m_expression.Conditional(compare, m_expression.Immediate(true), m_expression.Immediate(false));
    } else if (oper == "!=") {
        auto& compare = m_expression.Compare<NativeJIT::JccType::JNE>(call, m_expression.Immediate(value));
        cond = &m_expression.Conditional(compare, m_expression.Immediate(true), m_expression.Immediate(false));
    } else if (oper == ">") {
        auto& compare = m_expression.Compare<NativeJIT::JccType::JG>(call, m_expression.Immediate(value));
        cond = &m_expression.Conditional(compare, m_expression.Immediate(true), m_expression.Immediate(false));
    } else if (oper == ">=") {
        auto& compare = m_expression.Compare<NativeJIT::JccType::JGE>(call, m_expression.Immediate(value));
        cond = &m_expression.Conditional(compare, m_expression.Immediate(true), m_expression.Immediate(false));
    } else if (oper == "<") {
        auto& compare = m_expression.Compare<NativeJIT::JccType::JL>(call, m_expression.Immediate(value));
        cond = &m_expression.Conditional(compare, m_expression.Immediate(true), m_expression.Immediate(false));
    } else if (oper == "<=") {
        auto& compare = m_expression.Compare<NativeJIT::JccType::JLE>(call, m_expression.Immediate(value));
        cond = &m_expression.Conditional(compare, m_expression.Immediate(true), m_expression.Immediate(false));
    } else {
        assert(false);
    }

    m_expression_stack.push(cond);
}

void OSMObjectFilter::check_object_type(const std::string& type) {
    assert(!type.empty());
    if (m_verbose) {
        std::cerr << "HAS TYPE [" << type << "]\n";
    }

    int object_type = int(osmium::char_to_item_type(type[0]));

    auto& func = m_expression.Immediate(detail::get_type);
    auto& call = m_expression.Call(func, m_expression.GetP1());
    auto& compare = m_expression.Compare<NativeJIT::JccType::JE>(call, m_expression.Immediate(object_type));
    auto& e = m_expression.Conditional(compare, m_expression.Immediate(true), m_expression.Immediate(false));

    m_expression_stack.push(&e);
}

void OSMObjectFilter::check_id(int64_t value) {
    check_attr_int(std::tuple<std::string, std::string, int64_t>{"id", "=", value});
}

OSMObjectFilter::OSMObjectFilter(std::string& input, bool verbose) :
    m_verbose(verbose) {

    comment_skipper<std::string::iterator> skip;

    OSMObjectFilterGrammar<std::string::iterator> grammar(this);

    auto first = input.begin();
    auto last = input.end();
    const bool result = qi::phrase_parse(
        first,
        last,
        grammar,
        skip
    );

    if (!result || first != last) {
        throw std::runtime_error{"Can not parse expression"};
    }

    assert(m_expression_stack.size() == 1);

    const auto top = m_expression_stack.top();
    m_expression_stack.pop();

    m_function = m_expression.Compile(*top);
}

