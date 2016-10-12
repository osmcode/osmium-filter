#pragma once

#include <cstdint>
#include <iostream>
#include <numeric>
#include <regex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <boost/optional.hpp>

#include <osmium/osm/item_type.hpp>
#include <osmium/osm/entity_bits.hpp>

enum class expr_node_type : int {
    and_expr,
    or_expr,
    not_expr,
    integer_attribute,
    string_attribute,
    binary_int_op,
    binary_str_op,
    integer_value,
    string_value,
    regex_value,
    check_has_type,
    check_has_key,
    check_tag_str,
    check_tag_regex,
    check_attr_int
};

using entity_bits_pair = std::pair<osmium::osm_entity_bits::type, osmium::osm_entity_bits::type>;

class ExprNode {

public:

    constexpr ExprNode() = default;

    virtual ~ExprNode() {
    }

    virtual expr_node_type expression_type() const noexcept = 0;

    virtual void do_print(std::ostream& out, int level) const = 0;

    virtual entity_bits_pair calc_entities() const noexcept = 0;

    osmium::osm_entity_bits::type entities() const noexcept {
        return calc_entities().first;
    }

    void print(std::ostream& out, int level) const {
        const int this_level = level;
        while (level > 0) {
            out << ' ';
            --level;
        }
        do_print(out, this_level);
    }

}; // class ExprNode

class WithSubExpr : public ExprNode {

protected:

    std::vector<ExprNode*> m_children;

public:

    WithSubExpr(std::vector<ExprNode*> children) :
        m_children(children) {
    }

    const std::vector<ExprNode*>& children() const noexcept {
        return m_children;
    }

}; // class WithSubExpr

class AndExpr : public WithSubExpr {

public:

    AndExpr(std::vector<ExprNode*> children) :
        WithSubExpr(children) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::and_expr;
    }

    void do_print(std::ostream& out, int level) const override {
        out << "BOOL_AND\n";
        for (const auto* child : m_children) {
            child->print(out, level + 1);
        }
    }

    entity_bits_pair calc_entities() const noexcept override {
        const auto bits = std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
        return std::accumulate(children().begin(), children().end(), bits, [](entity_bits_pair b, const ExprNode* e) {
            const auto x = e->calc_entities();
            return std::make_pair(b.first & x.first, b.second & x.second);
        });
    }

}; // class AndExpr

class OrExpr : public WithSubExpr {

public:

    OrExpr(std::vector<ExprNode*> children) :
        WithSubExpr(children) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::or_expr;
    }

    void do_print(std::ostream& out, int level) const override {
        out << "BOOL_OR\n";
        for (const auto* child : m_children) {
            child->print(out, level + 1);
        }
    }

    entity_bits_pair calc_entities() const noexcept override {
        const auto bits = std::make_pair(osmium::osm_entity_bits::nothing, osmium::osm_entity_bits::nothing);
        return std::accumulate(children().begin(), children().end(), bits, [](entity_bits_pair b, const ExprNode* e) {
            const auto x = e->calc_entities();
            return std::make_pair(b.first | x.first, b.second | x.second);
        });
    }

}; // class OrExpr

class NotExpr : public ExprNode {

    ExprNode* m_child;

public:

    constexpr NotExpr(ExprNode* e) :
        m_child(e) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::not_expr;
    }

    const ExprNode* child() const noexcept {
        return m_child;
    }

    void do_print(std::ostream& out, int level) const override {
        out << "BOOL_NOT\n";
        m_child->print(out, level + 1);
    }

    entity_bits_pair calc_entities() const noexcept override {
        auto e = m_child->calc_entities();
        return std::make_pair(e.second, e.first);
    }

};

enum class attribute_type {
    id        = 0,
    version   = 1,
    visible   = 2,
    changeset = 3,
    uid       = 4,
    user      = 5,
    tags      = 6,
    nodes     = 7,
    members   = 8
};

inline const char* attribute_name(attribute_type attr) noexcept {
    static const char* names[] = {
        "id",
        "version",
        "visible",
        "changeset",
        "uid",
        "user",
        "tags",
        "nodes",
        "members"
    };

    return names[int(attr)];
}

class IntegerAttribute : public ExprNode {

    attribute_type m_attribute;

public:

    IntegerAttribute(const std::string& attr) {
        if (attr == "@id") {
            m_attribute = attribute_type::id;
        } else if (attr == "@version") {
            m_attribute = attribute_type::version;
        } else if (attr == "@changeset") {
            m_attribute = attribute_type::changeset;
        } else if (attr == "@uid") {
            m_attribute = attribute_type::uid;
        } else {
            throw std::runtime_error("not an integer attribute");
        }
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::integer_attribute;
    }

    void do_print(std::ostream& out, int /*level*/) const override {
        out << "INT_ATTR[" << attribute_name(m_attribute) << "]\n";
    }

    entity_bits_pair calc_entities() const noexcept override {
        return std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
    }

    attribute_type attribute() const noexcept {
        return m_attribute;
    }

}; // class IntegerAttribute

class StringAttribute : public ExprNode {

    attribute_type m_attribute;

public:

    StringAttribute(const std::string& attr) {
        if (attr == "@user") {
            m_attribute = attribute_type::user;
        } else {
            throw std::runtime_error("not a string attribute");
        }
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::string_attribute;
    }

    void do_print(std::ostream& out, int /*level*/) const override {
        out << "INT_ATTR[" << attribute_name(m_attribute) << "]\n";
    }

    entity_bits_pair calc_entities() const noexcept override {
        return std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
    }

    attribute_type attribute() const noexcept {
        return m_attribute;
    }

}; // class StringAttribute

enum class integer_op_type {
    equal,
    not_equal,
    less_than,
    less_or_equal,
    greater_than,
    greater_or_equal
};

class BinaryIntOperation : public ExprNode {

    ExprNode* m_lhs;
    ExprNode* m_rhs;
    integer_op_type m_op;

public:

    constexpr BinaryIntOperation(ExprNode* lhs, integer_op_type op, ExprNode* rhs) noexcept :
        m_lhs(lhs),
        m_rhs(rhs),
        m_op(op) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::binary_int_op;
    }

    const char* operator_name() const noexcept {
        static const char* names[] = {
            "equal",
            "not_equal",
            "less_than",
            "less_or_equal",
            "greater",
            "greater_or_equal"
        };

        return names[int(m_op)];
    }

    void do_print(std::ostream& out, int level) const override {
        out << "INT_BIN_OP[" << operator_name() << "]\n";
        lhs()->print(out, level + 1);
        rhs()->print(out, level + 1);
    }

    entity_bits_pair calc_entities() const noexcept override {
        const auto l = m_lhs->calc_entities();
        const auto r = m_rhs->calc_entities();
        return std::make_pair(l.first & r.first, l.second & r.second);
    }

    integer_op_type op() const noexcept {
        return m_op;
    }

    ExprNode* lhs() const noexcept {
        return m_lhs;
    }

    ExprNode* rhs() const noexcept {
        return m_rhs;
    }

}; // class BinaryIntOperation

enum class string_op_type {
    equal,
    not_equal
};

class BinaryStrOperation : public ExprNode {

    ExprNode* m_lhs;
    ExprNode* m_rhs;
    string_op_type m_op;

public:

    constexpr BinaryStrOperation(ExprNode* lhs, string_op_type op, ExprNode* rhs) noexcept :
        m_lhs(lhs),
        m_rhs(rhs),
        m_op(op) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::binary_str_op;
    }

    const char* operator_name() const noexcept {
        static const char* names[] = {
            "equal",
            "not_equal"
        };

        return names[int(m_op)];
    }

    void do_print(std::ostream& out, int level) const override {
        out << "INT_STR_OP[" << operator_name() << "]\n";
        lhs()->print(out, level + 1);
        rhs()->print(out, level + 1);
    }

    entity_bits_pair calc_entities() const noexcept override {
        const auto l = m_lhs->calc_entities();
        const auto r = m_rhs->calc_entities();
        return std::make_pair(l.first & r.first, l.second & r.second);
    }

    string_op_type op() const noexcept {
        return m_op;
    }

    ExprNode* lhs() const noexcept {
        return m_lhs;
    }

    ExprNode* rhs() const noexcept {
        return m_rhs;
    }

}; // class BinaryStrOperation

class IntegerValue : public ExprNode {

    std::int64_t m_value;

public:

    constexpr IntegerValue(std::int64_t value) :
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::integer_value;
    }

    void do_print(std::ostream& out, int /*level*/) const override {
        out << "INT_VALUE[" << m_value << "]\n";
    }

    entity_bits_pair calc_entities() const noexcept override {
        return std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
    }

    std::int64_t value() const noexcept {
        return m_value;
    }

}; // class IntegerValue

class StringValue : public ExprNode {

    std::string m_value;

public:

    StringValue(const std::string& value) :
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::string_value;
    }

    void do_print(std::ostream& out, int /*level*/) const override {
        out << "STR_VALUE[" << m_value << "]\n";
    }

    entity_bits_pair calc_entities() const noexcept override {
        return std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
    }

    const std::string& value() const noexcept {
        return m_value;
    }

}; // class StringValue

class RegexValue : public ExprNode {

    std::string m_str;
    std::regex m_value;

public:

    RegexValue(const std::regex& value) :
        m_str("UNKNOWN"),
        m_value(value) {
    }

    RegexValue(const std::string& value) :
        m_str(value),
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::regex_value;
    }

    void do_print(std::ostream& out, int /*level*/) const override {
        out << "REGEX_VALUE[" << m_str << "]\n";
    }

    entity_bits_pair calc_entities() const noexcept override {
        return std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
    }

    const std::regex& value() const noexcept {
        return m_value;
    }

}; // class RegexValue

class CheckHasKeyExpr : public ExprNode {

    std::string m_key;

public:

    CheckHasKeyExpr(const std::string& str) :
        m_key(str) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::check_has_key;
    }

    void do_print(std::ostream& out, int /*level*/) const override {
        out << "HAS_KEY \"" << m_key << "\"\n";
    }

    const char* key() const noexcept {
        return m_key.c_str();
    }

    entity_bits_pair calc_entities() const noexcept override {
        return std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
    }

};

class CheckTagStrExpr : public ExprNode {

    std::string m_key;
    std::string m_oper;
    std::string m_value;

public:

    CheckTagStrExpr(const std::string& key, const std::string& oper, const std::string& value) :
        m_key(key),
        m_oper(oper),
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::check_tag_str;
    }

    void do_print(std::ostream& out, int /*level*/) const override {
        out << "CHECK_TAG \"" << m_key << "\" " << m_oper << " \"" << m_value << "\"\n";
    }

    const char* key() const noexcept {
        return m_key.c_str();
    }

    const char* oper() const noexcept {
        return m_oper.c_str();
    }

    const char* value() const noexcept {
        return m_value.c_str();
    }

    entity_bits_pair calc_entities() const noexcept override {
        return std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
    }

};

class CheckTagRegexExpr : public ExprNode {

    std::string m_key;
    std::string m_oper;
    std::string m_value;
    std::regex m_value_regex;
    bool m_case_insensitive;

public:

    CheckTagRegexExpr(const std::string& key, const std::string& oper, const std::string& value, boost::optional<char>& ci) :
        m_key(key),
        m_oper(oper),
        m_value(value),
        m_value_regex(),
        m_case_insensitive(ci == 'i') {
        auto options = std::regex::nosubs | std::regex::optimize;
        if (m_case_insensitive) {
            options |= std::regex::icase;
        }
        m_value_regex = std::regex{m_value, options};
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::check_tag_regex;
    }

    void do_print(std::ostream& out, int /*level*/) const override {
        out << "CHECK_TAG \"" << m_key << "\" " << m_oper << " /" << m_value << "/" << (m_case_insensitive ? " (IGNORE CASE)" : "") << "\n";
    }

    entity_bits_pair calc_entities() const noexcept override {
        return std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
    }

    const char* key() const noexcept {
        return m_key.c_str();
    }

    const char* oper() const noexcept {
        return m_oper.c_str();
    }

    const std::regex* value_regex() const noexcept {
        return &m_value_regex;
    }

    bool case_insensitive() const noexcept {
        return m_case_insensitive;
    }

};

class CheckAttrIntExpr : public ExprNode {

    std::string m_attr;
    std::string m_oper;
    std::int64_t m_value;

public:

    CheckAttrIntExpr(const std::string& attr, const std::string& oper, std::int64_t value) :
        m_attr(attr),
        m_oper(oper),
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::check_attr_int;
    }

    void do_print(std::ostream& out, int /*level*/) const override {
        out << "CHECK_ATTR " << m_attr << " " << m_oper << " " << m_value << "\n";
    }

    entity_bits_pair calc_entities() const noexcept override {
        return std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
    }

    const std::string& attr() const noexcept {
        return m_attr;
    }

    const std::string& oper() const noexcept {
        return m_oper;
    }

    std::int64_t value() const noexcept {
        return m_value;
    }

};

class CheckObjectTypeExpr : public ExprNode {

    osmium::item_type m_type;

public:

    CheckObjectTypeExpr(const std::string& type) :
        m_type(osmium::char_to_item_type(type[0])) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::check_has_type;
    }

    osmium::item_type type() const noexcept {
        return m_type;
    }

    void do_print(std::ostream& out, int /*level*/) const override {
        out << "HAS_TYPE[" << osmium::item_type_to_name(m_type) << "]\n";
    }

    entity_bits_pair calc_entities() const noexcept override {
        auto e = osmium::osm_entity_bits::from_item_type(m_type);
        return std::make_pair(e, ~e);
    }

};


class OSMObjectFilter {

    ExprNode* m_root = nullptr;

public:

    OSMObjectFilter(std::string& input);

    const ExprNode* root() const noexcept {
        return m_root;
    }

    void print_tree(std::ostream& out) const {
        m_root->print(out, 0);
    }

    osmium::osm_entity_bits::type entities() const noexcept {
        return m_root->entities();
    }

}; // class OSMObjectFilter

