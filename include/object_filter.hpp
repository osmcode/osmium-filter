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
#include <boost/variant.hpp>

#include <osmium/osm/item_type.hpp>
#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/object.hpp>

enum class expr_node_type : int {
    and_expr,
    or_expr,
    not_expr,
    bool_value,
    integer_value,
    string_value,
    regex_value,
    integer_attribute,
    string_attribute,
    binary_int_op,
    binary_str_op,
    string_comp,
    tags_expr,
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

    virtual entity_bits_pair calc_entities() const noexcept {
        return std::make_pair(osmium::osm_entity_bits::all,
                              osmium::osm_entity_bits::all);
    }

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

    virtual bool eval_bool(const osmium::OSMObject& /*object*/) const {
        throw std::runtime_error{"Expected a bool expression"};
    }

    virtual int64_t eval_int(const osmium::OSMObject& /*object*/) const {
        throw std::runtime_error{"Expected an integer expression"};
    }

    virtual const char* eval_string(const osmium::OSMObject& /*object*/) const {
        throw std::runtime_error{"Expected a string expression"};
    }

}; // class ExprNode

class BoolExpression : public ExprNode {

public:

    int64_t eval_int(const osmium::OSMObject& object) const override final {
        return eval_bool(object) ? 1 : 0;
    }

}; // class BoolExpression

class IntegerExpression : public ExprNode {

public:

    bool eval_bool(const osmium::OSMObject& object) const override final {
        return eval_int(object) > 0;
    }

}; // class IntegerExpression

class StringExpression : public ExprNode {

public:

    bool eval_bool(const osmium::OSMObject& object) const override final {
        const char* str = eval_string(object);
        return str && str[0] != '\0';
    }

    int64_t eval_int(const osmium::OSMObject& object) const override final {
        return std::atoll(eval_string(object));
    }

}; // class StringExpression

class BoolValue : public BoolExpression {

    bool m_value;

public:

    constexpr BoolValue(bool value = true) :
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::bool_value;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << (m_value ? "TRUE" : "FALSE") << "\n";
    }

    bool eval_bool(const osmium::OSMObject& /*object*/) const override final {
        return m_value;
    }

}; // class BoolValue

class WithSubExpr : public BoolExpression {

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

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::and_expr;
    }

    void do_print(std::ostream& out, int level) const override final {
        out << "BOOL_AND\n";
        for (const auto* child : m_children) {
            assert(child);
            child->print(out, level + 1);
        }
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto bits = std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
        return std::accumulate(children().begin(), children().end(), bits, [](entity_bits_pair b, const ExprNode* e) {
            const auto x = e->calc_entities();
            return std::make_pair(b.first & x.first, b.second & x.second);
        });
    }

    bool eval_bool(const osmium::OSMObject& object) const override final {
        return std::accumulate(children().begin(), children().end(), true, [&object](bool b, const ExprNode* e) {
            return b & e->eval_bool(object);
        });
    }

}; // class AndExpr

class OrExpr : public WithSubExpr {

public:

    OrExpr(std::vector<ExprNode*> children) :
        WithSubExpr(children) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::or_expr;
    }

    void do_print(std::ostream& out, int level) const override final {
        out << "BOOL_OR\n";
        for (const auto* child : m_children) {
            assert(child);
            child->print(out, level + 1);
        }
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto bits = std::make_pair(osmium::osm_entity_bits::nothing, osmium::osm_entity_bits::nothing);
        return std::accumulate(children().begin(), children().end(), bits, [](entity_bits_pair b, const ExprNode* e) {
            assert(e);
            const auto x = e->calc_entities();
            return std::make_pair(b.first | x.first, b.second | x.second);
        });
    }

    bool eval_bool(const osmium::OSMObject& object) const override final {
        return std::accumulate(children().begin(), children().end(), false, [&object](bool b, const ExprNode* e) {
            return b | e->eval_bool(object);
        });
    }

}; // class OrExpr

class NotExpr : public BoolExpression {

    ExprNode* m_child;

public:

    constexpr NotExpr(ExprNode* e) :
        m_child(e) {
        assert(e);
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::not_expr;
    }

    const ExprNode* child() const noexcept {
        return m_child;
    }

    void do_print(std::ostream& out, int level) const override final {
        out << "BOOL_NOT\n";
        m_child->print(out, level + 1);
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto e = m_child->calc_entities();
        return std::make_pair(e.second, e.first);
    }

    bool eval_bool(const osmium::OSMObject& object) const override final {
        return !m_child->eval_bool(object);
    }

}; // class NotExpr

class IntegerValue : public IntegerExpression {

    std::int64_t m_value;

public:

    constexpr IntegerValue(std::int64_t value) :
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::integer_value;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "INT_VALUE[" << m_value << "]\n";
    }

    std::int64_t value() const noexcept {
        return m_value;
    }

    int64_t eval_int(const osmium::OSMObject& /*object*/) const override final {
        return m_value;
    }

}; // class IntegerValue

class StringValue : public StringExpression {

    std::string m_value;

public:

    StringValue(const std::string& value) :
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::string_value;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "STR_VALUE[" << m_value << "]\n";
    }

    const std::string& value() const noexcept {
        return m_value;
    }

    const char* eval_string(const osmium::OSMObject& /*object*/) const override final {
        return m_value.c_str();
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

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::regex_value;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "REGEX_VALUE[" << m_str << "]\n";
    }

    const std::regex* value() const noexcept {
        return &m_value;
    }

}; // class RegexValue

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

enum class integer_op_type {
    equal,
    not_equal,
    less_than,
    less_or_equal,
    greater_than,
    greater_or_equal
};

inline const char* operator_name(integer_op_type op) noexcept {
    static const char* names[] = {
        "equal",
        "not_equal",
        "less_than",
        "less_or_equal",
        "greater",
        "greater_or_equal"
    };

    return names[int(op)];
}

enum class string_op_type {
    equal,
    not_equal,
    match,
    not_match
};

inline const char* operator_name(string_op_type op) noexcept {
    static const char* names[] = {
        "equal",
        "not_equal",
        "match",
        "not_match"
    };

    return names[int(op)];
}

class IntegerAttribute : public IntegerExpression {

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
            throw std::runtime_error{"not an integer attribute"};
        }
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::integer_attribute;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "INT_ATTR[" << attribute_name(m_attribute) << "]\n";
    }

    attribute_type attribute() const noexcept {
        return m_attribute;
    }

    int64_t eval_int(const osmium::OSMObject& object) const override final {
        switch (m_attribute) {
            case attribute_type::id:
                return object.id();
            case attribute_type::version:
                return object.version();
            case attribute_type::changeset:
                return object.changeset();
            case attribute_type::uid:
                return object.uid();
            default:
                break;
        }
        throw std::runtime_error{"not an int"};
    }

}; // class IntegerAttribute

class StringAttribute : public StringExpression {

    attribute_type m_attribute;

public:

    StringAttribute(const std::string& attr) {
        if (attr == "@user") {
            m_attribute = attribute_type::user;
        } else {
            throw std::runtime_error{"not a string attribute"};
        }
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::string_attribute;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "STR_ATTR[" << attribute_name(m_attribute) << "]\n";
    }

    attribute_type attribute() const noexcept {
        return m_attribute;
    }

    const char* eval_string(const osmium::OSMObject& object) const override final {
        return object.user();
    }

}; // class StringAttribute

class BinaryIntOperation : public BoolExpression {

    ExprNode* m_lhs;
    ExprNode* m_rhs;
    integer_op_type m_op;

public:

    constexpr BinaryIntOperation(ExprNode* lhs, integer_op_type op, ExprNode* rhs) noexcept :
        m_lhs(lhs),
        m_rhs(rhs),
        m_op(op) {
        assert(lhs);
        assert(rhs);
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::binary_int_op;
    }

    void do_print(std::ostream& out, int level) const override final {
        out << "INT_BIN_OP[" << operator_name(m_op) << "]\n";
        lhs()->print(out, level + 1);
        rhs()->print(out, level + 1);
    }

    entity_bits_pair calc_entities() const noexcept override final {
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

    bool eval_bool(const osmium::OSMObject& object) const override final {
        const int64_t lhs = m_lhs->eval_int(object);
        const int64_t rhs = m_rhs->eval_int(object);

        switch (m_op) {
            case integer_op_type::equal:
                return lhs == rhs;
            case integer_op_type::not_equal:
                return lhs != rhs;
            case integer_op_type::less_than:
                return lhs < rhs;
            case integer_op_type::less_or_equal:
                return lhs <= rhs;
            case integer_op_type::greater_than:
                return lhs > rhs;
            case integer_op_type::greater_or_equal:
                return lhs >= rhs;
            default:
                break;
        }

        throw std::runtime_error{"unknown op"};
    }

}; // class BinaryIntOperation

class BinaryStrOperation : public BoolExpression {

    ExprNode* m_lhs;
    ExprNode* m_rhs;
    string_op_type m_op;

public:

    constexpr BinaryStrOperation(ExprNode* lhs, string_op_type op, ExprNode* rhs) noexcept :
        m_lhs(lhs),
        m_rhs(rhs),
        m_op(op) {
        assert(lhs);
        assert(rhs);
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::binary_str_op;
    }

    void do_print(std::ostream& out, int level) const override final {
        out << "BIN_STR_OP[" << operator_name(m_op) << "]\n";
        lhs()->print(out, level + 1);
        rhs()->print(out, level + 1);
    }

    entity_bits_pair calc_entities() const noexcept override final {
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

    bool eval_bool(const osmium::OSMObject& object) const override final {
        const char* value = m_lhs->eval_string(object);

        switch (m_op) {
            case string_op_type::equal:
                return !std::strcmp(value, m_rhs->eval_string(object));
            case string_op_type::not_equal:
                return std::strcmp(value, m_rhs->eval_string(object));
            case string_op_type::match:
                return std::regex_search(value, *(dynamic_cast<RegexValue*>(m_rhs)->value()));
            case string_op_type::not_match:
                return !std::regex_search(value, *(dynamic_cast<RegexValue*>(m_rhs)->value()));
            default:
                break;
        }

        throw std::runtime_error{"unknown op"};
    }

}; // class BinaryStrOperation

class StringComp : public ExprNode {

    string_op_type m_op;
    ExprNode*      m_value;

public:

    StringComp(string_op_type op, ExprNode* value) :
        m_op(op),
        m_value(value) {
        assert(value);
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::string_comp;
    }

    void do_print(std::ostream& out, int level) const override final {
        out << "STRING_COMP[" << operator_name(m_op) << "]\n";
        m_value->print(out, level + 1);
    }

    string_op_type op() const noexcept {
        return m_op;
    }

    ExprNode* value() const noexcept {
        return m_value;
    }

}; // class StringComp


class TagsExpr : public BoolExpression {

    ExprNode* m_key_expr;
    ExprNode* m_val_expr;

public:

    TagsExpr(ExprNode* key_expr, ExprNode* val_expr) :
        m_key_expr(key_expr ? key_expr : new BoolValue(true)),
        m_val_expr(val_expr ? val_expr : new BoolValue(true)) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::tags_expr;
    }

    void do_print(std::ostream& out, int level) const override final {
        out << "TAGS_ATTR\n";
        m_key_expr->print(out, level + 1);
        m_val_expr->print(out, level + 1);
    }

    ExprNode* key_expr() const noexcept {
        return m_key_expr;
    }

    ExprNode* val_expr() const noexcept {
        return m_val_expr;
    }

}; // class TagsExpr

class CheckHasKeyExpr : public BoolExpression {

    std::string m_key;

public:

    CheckHasKeyExpr(const std::string& str) :
        m_key(str) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_has_key;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "HAS_KEY \"" << m_key << "\"\n";
    }

    const char* key() const noexcept {
        return m_key.c_str();
    }

};

class CheckTagStrExpr : public BoolExpression {

    std::string m_key;
    std::string m_oper;
    std::string m_value;

public:

    CheckTagStrExpr(const std::string& key, const std::string& oper, const std::string& value) :
        m_key(key),
        m_oper(oper),
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_tag_str;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
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

};

class CheckTagRegexExpr : public BoolExpression {

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

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_tag_regex;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "CHECK_TAG \"" << m_key << "\" " << m_oper << " /" << m_value << "/" << (m_case_insensitive ? " (IGNORE CASE)" : "") << "\n";
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

class CheckAttrIntExpr : public BoolExpression {

    std::string m_attr;
    std::string m_oper;
    std::int64_t m_value;

public:

    CheckAttrIntExpr(const std::string& attr, const std::string& oper, std::int64_t value) :
        m_attr(attr),
        m_oper(oper),
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_attr_int;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "CHECK_ATTR " << m_attr << " " << m_oper << " " << m_value << "\n";
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

class CheckObjectTypeExpr : public BoolExpression {

    osmium::item_type m_type;

public:

    CheckObjectTypeExpr(const std::string& type) :
        m_type(osmium::char_to_item_type(type[0])) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_has_type;
    }

    osmium::item_type type() const noexcept {
        return m_type;
    }

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "HAS_TYPE[" << osmium::item_type_to_name(m_type) << "]\n";
    }

    entity_bits_pair calc_entities() const noexcept override final {
        auto e = osmium::osm_entity_bits::from_item_type(m_type);
        return std::make_pair(e, ~e);
    }

    bool eval_bool(const osmium::OSMObject& object) const override final {
        return object.type() == m_type;
    }

}; // class CheckObjecTypeExpr


class OSMObjectFilter {

    ExprNode* m_root = nullptr;

public:

    OSMObjectFilter(std::string& input);

    const ExprNode* root() const noexcept {
        return m_root;
    }

    void print_tree(std::ostream& out) const {
        assert(m_root);
        m_root->print(out, 0);
    }

    osmium::osm_entity_bits::type entities() const noexcept {
        assert(m_root);
        return m_root->entities();
    }

    bool match(const osmium::OSMObject& object) const {
        assert(m_root);
        return m_root->eval_bool(object);
    }

}; // class OSMObjectFilter

