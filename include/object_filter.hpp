#pragma once

#include <algorithm>
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

#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/item_type.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/node_ref.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/way.hpp>

enum class integer_attribute_type {
    id,
    version,
    changeset,
    uid,
    ref
};

inline const char* attribute_name(integer_attribute_type attr) noexcept {
    static const char* names[] = {
        "id",
        "version",
        "changeset",
        "uid",
        "ref"
    };

    return names[int(attr)];
}

enum class string_attribute_type {
    user,
    key,
    value,
    role
};

inline const char* attribute_name(string_attribute_type attr) noexcept {
    static const char* names[] = {
        "user",
        "key",
        "value",
        "role"
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
    nodes_expr,
    members_expr,
    check_has_type,
    check_has_key,
    check_tag_str,
    check_tag_regex
};

using entity_bits_pair = std::pair<osmium::osm_entity_bits::type,
                                   osmium::osm_entity_bits::type>;

class ExprNode {

protected:

    virtual void do_print(std::ostream& out, int level) const = 0;

public:

    constexpr ExprNode() = default;

    virtual ~ExprNode() {
    }

    virtual expr_node_type expression_type() const noexcept = 0;

    virtual entity_bits_pair calc_entities() const noexcept {
        return std::make_pair(osmium::osm_entity_bits::all,
                              osmium::osm_entity_bits::all);
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

    virtual std::int64_t eval_int(const osmium::OSMObject& /*object*/) const {
        throw std::runtime_error{"Expected an integer expression"};
    }

    virtual const char* eval_string(const osmium::OSMObject& /*object*/) const {
        throw std::runtime_error{"Expected a string expression"};
    }

    virtual bool eval_bool(const osmium::Tag& /*tag*/) const {
        throw std::runtime_error{"Expected a bool expression for tags"};
    }

    virtual std::int64_t eval_int(const osmium::Tag& /*tag*/) const {
        throw std::runtime_error{"Expected an integer expression for tags"};
    }

    virtual const char* eval_string(const osmium::Tag& /*tag*/) const {
        throw std::runtime_error{"Expected a string expression for tags"};
    }

    virtual bool eval_bool(const osmium::NodeRef& /*nr*/) const {
        throw std::runtime_error{"Expected a bool expression for a node ref"};
    }

    virtual std::int64_t eval_int(const osmium::NodeRef& /*nr*/) const {
        throw std::runtime_error{"Expected an integer expression for node refs"};
    }

    virtual const char* eval_string(const osmium::NodeRef& /*nr*/) const {
        throw std::runtime_error{"Expected a string expression for node refs"};
    }

    virtual bool eval_bool(const osmium::RelationMember& /*member*/) const {
        throw std::runtime_error{"Expected a bool expression for members"};
    }

    virtual std::int64_t eval_int(const osmium::RelationMember& /*member*/) const {
        throw std::runtime_error{"Expected an integer expression for members"};
    }

    virtual const char* eval_string(const osmium::RelationMember& /*member*/) const {
        throw std::runtime_error{"Expected a string expression for member"};
    }

}; // class ExprNode

class BoolExpression : public ExprNode {

public:

    std::int64_t eval_int(const osmium::OSMObject& object) const override final {
        return eval_bool(object) ? 1 : 0;
    }

    std::int64_t eval_int(const osmium::Tag& tag) const override final {
        return eval_bool(tag) ? 1 : 0;
    }

    std::int64_t eval_int(const osmium::NodeRef& nr) const override final {
        return eval_bool(nr) ? 1 : 0;
    }

    std::int64_t eval_int(const osmium::RelationMember& member) const override final {
        return eval_bool(member) ? 1 : 0;
    }

}; // class BoolExpression

class IntegerExpression : public ExprNode {

public:

    bool eval_bool(const osmium::OSMObject& object) const override final {
        return eval_int(object) > 0;
    }

    bool eval_bool(const osmium::Tag& tag) const override final {
        return eval_int(tag) > 0;
    }

    bool eval_bool(const osmium::NodeRef& nr) const override final {
        return eval_int(nr) > 0;
    }

    bool eval_bool(const osmium::RelationMember& member) const override final {
        return eval_int(member) > 0;
    }

}; // class IntegerExpression

class StringExpression : public ExprNode {

public:

    bool eval_bool(const osmium::OSMObject& object) const override final {
        const char* str = eval_string(object);
        return str && str[0] != '\0';
    }

    std::int64_t eval_int(const osmium::OSMObject& object) const override final {
        return std::atoll(eval_string(object));
    }

    bool eval_bool(const osmium::Tag& tag) const override final {
        const char* str = eval_string(tag);
        return str && str[0] != '\0';
    }

    std::int64_t eval_int(const osmium::Tag& tag) const override final {
        return std::atoll(eval_string(tag));
    }

    bool eval_bool(const osmium::NodeRef& nr) const override final {
        const char* str = eval_string(nr);
        return str && str[0] != '\0';
    }

    std::int64_t eval_int(const osmium::NodeRef& nr) const override final {
        return std::atoll(eval_string(nr));
    }

    bool eval_bool(const osmium::RelationMember& member) const override final {
        const char* str = eval_string(member);
        return str && str[0] != '\0';
    }

    std::int64_t eval_int(const osmium::RelationMember& member) const override final {
        return std::atoll(eval_string(member));
    }

}; // class StringExpression

class BoolValue : public BoolExpression {

    bool m_value;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << (m_value ? "TRUE" : "FALSE") << "\n";
    }

public:

    constexpr BoolValue(bool value = true) :
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::bool_value;
    }

    bool eval_bool(const osmium::OSMObject& /*object*/) const noexcept override final {
        return m_value;
    }

    bool eval_bool(const osmium::Tag& /*tag*/) const noexcept override final {
        return m_value;
    }

    bool eval_bool(const osmium::NodeRef& /*nr*/) const noexcept override final {
        return m_value;
    }

    bool eval_bool(const osmium::RelationMember& /*member*/) const noexcept override final {
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

    template <typename T>
    bool eval_bool_impl(const T& t) const {
        const auto it = std::find_if(children().cbegin(), children().cend(), [&t](const ExprNode* e){
            return !e->eval_bool(t);
        });

        return it == children().cend();
    }

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "BOOL_AND\n";
        for (const auto* child : m_children) {
            assert(child);
            child->print(out, level + 1);
        }
    }

public:

    AndExpr(std::vector<ExprNode*> children) :
        WithSubExpr(children) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::and_expr;
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto bits = std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
        return std::accumulate(children().begin(), children().end(), bits, [](entity_bits_pair b, const ExprNode* e) {
            const auto x = e->calc_entities();
            return std::make_pair(b.first & x.first, b.second & x.second);
        });
    }

    bool eval_bool(const osmium::OSMObject& object) const override final {
        return eval_bool_impl(object);
    }

    bool eval_bool(const osmium::Tag& tag) const override final {
        return eval_bool_impl(tag);
    }

    bool eval_bool(const osmium::NodeRef& nr) const override final {
        return eval_bool_impl(nr);
    }

    bool eval_bool(const osmium::RelationMember& member) const override final {
        return eval_bool_impl(member);
    }

}; // class AndExpr

class OrExpr : public WithSubExpr {

    template <typename T>
    bool eval_bool_impl(const T& t) const {
        const auto it = std::find_if(children().cbegin(), children().cend(), [&t](const ExprNode* e){
            return e->eval_bool(t);
        });

        return it != children().cend();
    }

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "BOOL_OR\n";
        for (const auto* child : m_children) {
            assert(child);
            child->print(out, level + 1);
        }
    }

public:

    OrExpr(std::vector<ExprNode*> children) :
        WithSubExpr(children) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::or_expr;
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
        return eval_bool_impl(object);
    }

    bool eval_bool(const osmium::Tag& tag) const override final {
        return eval_bool_impl(tag);
    }

    bool eval_bool(const osmium::NodeRef& nr) const override final {
        return eval_bool_impl(nr);
    }

    bool eval_bool(const osmium::RelationMember& member) const override final {
        return eval_bool_impl(member);
    }

}; // class OrExpr

class NotExpr : public BoolExpression {

    ExprNode* m_child;

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "BOOL_NOT\n";
        m_child->print(out, level + 1);
    }


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

    entity_bits_pair calc_entities() const noexcept override final {
        const auto e = m_child->calc_entities();
        return std::make_pair(e.second, e.first);
    }

    bool eval_bool(const osmium::OSMObject& object) const override final {
        return !m_child->eval_bool(object);
    }

    bool eval_bool(const osmium::Tag& tag) const override final {
        return !m_child->eval_bool(tag);
    }

    bool eval_bool(const osmium::NodeRef& nr) const override final {
        return !m_child->eval_bool(nr);
    }

    bool eval_bool(const osmium::RelationMember& member) const override final {
        return !m_child->eval_bool(member);
    }

}; // class NotExpr

class IntegerValue : public IntegerExpression {

    std::int64_t m_value;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "INT_VALUE[" << m_value << "]\n";
    }

public:

    constexpr IntegerValue(std::int64_t value) :
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::integer_value;
    }

    std::int64_t value() const noexcept {
        return m_value;
    }

    std::int64_t eval_int(const osmium::OSMObject& /*object*/) const noexcept override final {
        return m_value;
    }

    std::int64_t eval_int(const osmium::Tag& /*tag*/) const noexcept override final {
        return m_value;
    }

    std::int64_t eval_int(const osmium::NodeRef& /*nr*/) const noexcept override final {
        return m_value;
    }

    std::int64_t eval_int(const osmium::RelationMember& /*member*/) const noexcept override final {
        return m_value;
    }

}; // class IntegerValue

class StringValue : public StringExpression {

    std::string m_value;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "STR_VALUE[" << m_value << "]\n";
    }

public:

    StringValue(const std::string& value) :
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::string_value;
    }

    const std::string& value() const noexcept {
        return m_value;
    }

    const char* eval_string(const osmium::OSMObject& /*object*/) const noexcept override final {
        return m_value.c_str();
    }

    const char* eval_string(const osmium::Tag& /*tag*/) const noexcept override final {
        return m_value.c_str();
    }

    const char* eval_string(const osmium::NodeRef& /*nr*/) const noexcept override final {
        return m_value.c_str();
    }

    const char* eval_string(const osmium::RelationMember& /*member*/) const noexcept override final {
        return m_value.c_str();
    }

}; // class StringValue

class RegexValue : public ExprNode {

    std::string m_str;
    std::regex m_value;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "REGEX_VALUE[" << m_str << "]\n";
    }

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

    const std::regex* value() const noexcept {
        return &m_value;
    }

}; // class RegexValue

class CheckObjectTypeExpr : public BoolExpression {

    osmium::item_type m_type;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "HAS_TYPE[" << osmium::item_type_to_name(m_type) << "]\n";
    }

public:

    CheckObjectTypeExpr(osmium::item_type type) :
        m_type(type) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_has_type;
    }

    osmium::item_type type() const noexcept {
        return m_type;
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto e = osmium::osm_entity_bits::from_item_type(m_type);
        return std::make_pair(e, ~e);
    }

    bool eval_bool(const osmium::OSMObject& object) const noexcept override final {
        return object.type() == m_type;
    }

    bool eval_bool(const osmium::RelationMember& member) const noexcept override final {
        return member.type() == m_type;
    }

}; // class CheckObjectTypeExpr

class IntegerAttribute : public IntegerExpression {

    integer_attribute_type m_attribute;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "INT_ATTR[" << attribute_name(m_attribute) << "]\n";
    }

public:

    IntegerAttribute(integer_attribute_type attr) noexcept :
        m_attribute(attr) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::integer_attribute;
    }

    integer_attribute_type attribute() const noexcept {
        return m_attribute;
    }

    std::int64_t eval_int(const osmium::OSMObject& object) const override final {
        switch (m_attribute) {
            case integer_attribute_type::id:
                return object.id();
            case integer_attribute_type::version:
                return object.version();
            case integer_attribute_type::changeset:
                return object.changeset();
            case integer_attribute_type::uid:
                return object.uid();
            default:
                break;
        }

        assert(false);
    }

    std::int64_t eval_int(const osmium::NodeRef& nr) const override final {
        assert(m_attribute == integer_attribute_type::ref);
        return nr.ref();
    }

    std::int64_t eval_int(const osmium::RelationMember& member) const override final {
        assert(m_attribute == integer_attribute_type::ref);
        return member.ref();
    }

}; // class IntegerAttribute

class StringAttribute : public StringExpression {

    string_attribute_type m_attribute;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "STR_ATTR[" << attribute_name(m_attribute) << "]\n";
    }

public:

    StringAttribute(string_attribute_type attr) noexcept :
        m_attribute(attr) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::string_attribute;
    }

    string_attribute_type attribute() const noexcept {
        return m_attribute;
    }

    const char* eval_string(const osmium::OSMObject& object) const override final {
        assert(m_attribute == string_attribute_type::user);

        return object.user();
    }

    const char* eval_string(const osmium::Tag& tag) const override final {
        if (m_attribute == string_attribute_type::key) {
            return tag.key();
        } else if (m_attribute == string_attribute_type::value) {
            return tag.value();
        }

        assert(false);
    }

    const char* eval_string(const osmium::RelationMember& member) const override final {
        assert(m_attribute == string_attribute_type::role);

        return member.role();
    }

}; // class StringAttribute

class BinaryIntOperation : public BoolExpression {

    ExprNode* m_lhs;
    ExprNode* m_rhs;
    integer_op_type m_op;

    bool compare(std::int64_t lhs, std::int64_t rhs) const {
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

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "INT_BIN_OP[" << operator_name(m_op) << "]\n";
        lhs()->print(out, level + 1);
        rhs()->print(out, level + 1);
    }

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
        return compare(m_lhs->eval_int(object),
                       m_rhs->eval_int(object));
    }

    bool eval_bool(const osmium::NodeRef& nr) const override final {
        return compare(m_lhs->eval_int(nr),
                       m_rhs->eval_int(nr));
    }

    bool eval_bool(const osmium::RelationMember& member) const override final {
        return compare(m_lhs->eval_int(member),
                       m_rhs->eval_int(member));
    }

}; // class BinaryIntOperation

class BinaryStrOperation : public BoolExpression {

    ExprNode* m_lhs;
    ExprNode* m_rhs;
    string_op_type m_op;

    template <typename T>
    bool eval_bool_impl(const T& t) const {
        const char* value = m_lhs->eval_string(t);

        switch (m_op) {
            case string_op_type::equal:
                return !std::strcmp(value, m_rhs->eval_string(t));
            case string_op_type::not_equal:
                return std::strcmp(value, m_rhs->eval_string(t));
            case string_op_type::match:
                return std::regex_search(value, *(dynamic_cast<RegexValue*>(m_rhs)->value()));
            case string_op_type::not_match:
                return !std::regex_search(value, *(dynamic_cast<RegexValue*>(m_rhs)->value()));
            default:
                break;
        }

        throw std::runtime_error{"unknown op"};
    }

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "BIN_STR_OP[" << operator_name(m_op) << "]\n";
        lhs()->print(out, level + 1);
        rhs()->print(out, level + 1);
    }

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
        return eval_bool_impl(object);
    }

    bool eval_bool(const osmium::Tag& tag) const override final {
        return eval_bool_impl(tag);
    }

    bool eval_bool(const osmium::RelationMember& member) const override final {
        return eval_bool_impl(member);
    }

}; // class BinaryStrOperation

class StringComp : public ExprNode {

    string_op_type m_op;
    ExprNode*      m_value;

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "STRING_COMP[" << operator_name(m_op) << "]\n";
        m_value->print(out, level + 1);
    }

public:

    StringComp(string_op_type op, ExprNode* value) :
        m_op(op),
        m_value(value) {
        assert(value);
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::string_comp;
    }

    string_op_type op() const noexcept {
        return m_op;
    }

    ExprNode* value() const noexcept {
        return m_value;
    }

}; // class StringComp

class TagsExpr : public IntegerExpression {

    ExprNode* m_expr;

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "CHECK_TAGS\n";
        m_expr->print(out, level + 1);
    }

public:

    TagsExpr(ExprNode* expr = new BoolValue(true)) :
        m_expr(expr) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::tags_expr;
    }

    ExprNode* expr() const noexcept {
        return m_expr;
    }

    std::int64_t eval_int(const osmium::OSMObject& object) const override final {
        return std::count_if(object.tags().cbegin(), object.tags().cend(), [this](const osmium::Tag& tag){
            return m_expr->eval_bool(tag);
        });
    }

}; // class TagsExpr

class NodesExpr : public IntegerExpression {

    ExprNode* m_expr;

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "CHECK_NODES\n";
        m_expr->print(out, level + 1);
    }

public:

    NodesExpr(ExprNode* expr = new BoolValue(true)) :
        m_expr(expr) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::nodes_expr;
    }

    ExprNode* expr() const noexcept {
        return m_expr;
    }

    std::int64_t eval_int(const osmium::OSMObject& object) const override final {
        if (object.type() != osmium::item_type::way) {
            return 0;
        }

        const auto& nodes = static_cast<const osmium::Way&>(object).nodes();
        return std::count_if(nodes.cbegin(), nodes.cend(), [this](const osmium::NodeRef& nr){
            return m_expr->eval_bool(nr);
        });
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto e = osmium::osm_entity_bits::way;
        return std::make_pair(e, ~e);
    }

}; // class NodesExpr

class MembersExpr : public IntegerExpression {

    ExprNode* m_expr;

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "CHECK_MEMBERS\n";
        m_expr->print(out, level + 1);
    }

public:

    MembersExpr(ExprNode* expr = new BoolValue(true)) :
        m_expr(expr) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::members_expr;
    }

    ExprNode* expr() const noexcept {
        return m_expr;
    }

    std::int64_t eval_int(const osmium::OSMObject& object) const override final {
        if (object.type() != osmium::item_type::relation) {
            return 0;
        }

        const auto& members = static_cast<const osmium::Relation&>(object).members();
        return std::count_if(members.cbegin(), members.cend(), [this](const osmium::RelationMember& member){
            return m_expr->eval_bool(member);
        });
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto e = osmium::osm_entity_bits::relation;
        return std::make_pair(e, ~e);
    }

}; // class MembersExpr

class CheckHasKeyExpr : public BoolExpression {

    std::string m_key;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "HAS_KEY \"" << m_key << "\"\n";
    }

public:

    CheckHasKeyExpr(const std::string& str) :
        m_key(str) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_has_key;
    }

    const char* key() const noexcept {
        return m_key.c_str();
    }

};

class CheckTagStrExpr : public BoolExpression {

    std::string m_key;
    std::string m_oper;
    std::string m_value;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "CHECK_TAG \"" << m_key << "\" " << m_oper << " \"" << m_value << "\"\n";
    }

public:

    CheckTagStrExpr(const std::string& key, const std::string& oper, const std::string& value) :
        m_key(key),
        m_oper(oper),
        m_value(value) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_tag_str;
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

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "CHECK_TAG \"" << m_key << "\" " << m_oper << " /" << m_value << "/" << (m_case_insensitive ? " (IGNORE CASE)" : "") << "\n";
    }

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


class OSMObjectFilter {

    ExprNode* m_root = new BoolValue{true};

public:

    OSMObjectFilter(std::string& input);

    const ExprNode* root() const noexcept {
        return m_root;
    }

    void print_tree(std::ostream& out) const {
        m_root->print(out, 0);
    }

    osmium::osm_entity_bits::type entities() const noexcept {
        return m_root->calc_entities().first;
    }

    bool match(const osmium::OSMObject& object) const {
        return m_root->eval_bool(object);
    }

}; // class OSMObjectFilter

