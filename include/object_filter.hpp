#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <osmium/index/id_set.hpp>
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

enum class boolean_attribute_type {
    node,
    way,
    relation,
    visible,
    closed_way,
    open_way
};

inline const char* attribute_name(boolean_attribute_type attr) noexcept {
    static const char* names[] = {
        "node",
        "way",
        "relation",
        "visible",
        "closed_way",
        "open_way"
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
        "greater_than",
        "greater_or_equal"
    };

    return names[int(op)];
}

enum class string_op_type {
    equal,
    not_equal,
    prefix_equal,
    prefix_not_equal,
    match,
    not_match
};

inline const char* operator_name(string_op_type op) noexcept {
    static const char* names[] = {
        "equal",
        "not_equal",
        "prefix_equal",
        "prefix_not_equal",
        "match",
        "not_match"
    };

    return names[int(op)];
}

enum class list_op_type {
    in,
    not_in
};

inline const char* operator_name(list_op_type op) noexcept {
    static const char* names[] = {
        "in",
        "not_in"
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
    boolean_attribute,
    binary_int_op,
    binary_str_op,
    string_comp,
    tags_expr,
    nodes_expr,
    members_expr,
    closed_way,
    in_integer_list,
    check_has_type,
    check_has_key,
    check_tag_str,
    check_tag_regex
};

using entity_bits_pair = std::pair<osmium::osm_entity_bits::type,
                                   osmium::osm_entity_bits::type>;

class unused {};

class ExprNode;
class WithSubExpr;

// This is a horrible hack to make boost::spirit work with unique_ptr.
// I could not find a cleaner way than this. Basically we wrap the unique_ptr
// in this class to make it copyable (by doing a move inside the copy of the
// wrapper class).
template <typename T>
struct expr_node {

    mutable std::unique_ptr<ExprNode> m_data;

    using value_type = expr_node<ExprNode>;

    expr_node() :
        m_data(nullptr) {
    }

    expr_node(unused) :
        m_data(new T) {
    }

    // Used for AndExpr and OrExpr
    expr_node(const std::vector<expr_node<ExprNode>>& expressions) :
        m_data(nullptr) {
        if (expressions.size() == 1) {
            m_data.reset(expressions[0].release());
        } else {
            m_data.reset(new T(expressions));
        }
    }

    template <typename... Args>
    expr_node(Args&&... args) :
        m_data(new T(std::forward<Args>(args)...)) {
    }

    expr_node(const expr_node& other) :
        m_data(std::move(other.m_data)) {
    }

    template <typename D, typename std::enable_if<std::is_base_of<T, D>{}, int>::type = 0>
    expr_node(const expr_node<D>& other) {
        m_data.reset(other.m_data.release());
    }

    expr_node(expr_node&& other) = default;

    template <typename D, typename std::enable_if<std::is_base_of<T, D>{}, int>::type = 0>
    expr_node(expr_node<D>&& other) {
        m_data.reset(other.m_data.release());
    }

    expr_node& operator=(const expr_node& other) {
        m_data = std::move(other.m_data);
        return *this;
    }

    template <typename D, typename std::enable_if<std::is_base_of<T, D>{}, int>::type = 0>
    expr_node& operator=(const expr_node<D>& other) {
        m_data = std::move(other.m_data);
        return *this;
    }

    expr_node& operator=(expr_node&& other) = default;

    template <typename D, typename std::enable_if<std::is_base_of<T, D>{}, int>::type = 0>
    expr_node& operator=(expr_node<D>&& other) {
        m_data = std::move(other.m_data);
        return *this;
    }

    ExprNode* release() const {
        return m_data.release();
    }

    ExprNode* get() const {
        return m_data.get();
    }

}; // class expr_node

class ExprNode {

protected:

    virtual void do_print(std::ostream& out, int level) const = 0;

public:

    ExprNode() = default;

    virtual ~ExprNode() {
    }

    virtual expr_node_type expression_type() const noexcept = 0;

    virtual entity_bits_pair calc_entities() const noexcept {
        return std::make_pair(osmium::osm_entity_bits::nwr,
                              osmium::osm_entity_bits::nwr);
    }

    void indent(std::ostream& out, int level) const {
        while (level > 0) {
            out << ' ';
            --level;
        }
    }

    void print(std::ostream& out, int level) const {
        indent(out, level);
        do_print(out, level);
    }

    virtual void prepare() {
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

class BooleanValue : public BoolExpression {

    bool m_value;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << (m_value ? "TRUE" : "FALSE") << "\n";
    }

public:

    explicit BooleanValue(bool value = true) :
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

}; // class BooleanValue

class WithSubExpr : public BoolExpression {

    std::vector<std::unique_ptr<ExprNode>> m_children;

public:

    explicit WithSubExpr(std::vector<std::unique_ptr<ExprNode>>&& children) :
        m_children(std::move(children)) {
#ifndef NDEBUG
        for (const auto& child : m_children) {
            assert(child);
        }
#endif
    }

    explicit WithSubExpr(const std::vector<expr_node<ExprNode>>& children) {
        for (auto& child : children) {
            assert(child.get());
            m_children.emplace_back(child.release());
        }
    }

    const std::vector<std::unique_ptr<ExprNode>>& children() const noexcept {
        return m_children;
    }

    void prepare() override final {
        for (auto& child : m_children) {
            child->prepare();
        }
    }

}; // class WithSubExpr

class AndExpr : public WithSubExpr {

    template <typename T>
    bool eval_bool_impl(const T& t) const {
        const auto it = std::find_if(children().cbegin(), children().cend(), [&t](const std::unique_ptr<ExprNode>& e){
            return !e->eval_bool(t);
        });

        return it == children().cend();
    }

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "BOOL_AND\n";
        for (const auto& child : children()) {
            child->print(out, level + 1);
        }
    }

public:

    explicit AndExpr(std::vector<std::unique_ptr<ExprNode>>&& children) :
        WithSubExpr(std::move(children)) {
    }

    explicit AndExpr(const std::vector<expr_node<ExprNode>>& children) :
        WithSubExpr(children) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::and_expr;
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto bits = std::make_pair(osmium::osm_entity_bits::nwr, osmium::osm_entity_bits::nwr);
        return std::accumulate(children().begin(), children().end(), bits, [](entity_bits_pair b, const std::unique_ptr<ExprNode>& e) {
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
        const auto it = std::find_if(children().cbegin(), children().cend(), [&t](const std::unique_ptr<ExprNode>& e){
            return e->eval_bool(t);
        });

        return it != children().cend();
    }

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "BOOL_OR\n";
        for (const auto& child : children()) {
            child->print(out, level + 1);
        }
    }

public:

    explicit OrExpr(std::vector<std::unique_ptr<ExprNode>>&& children) :
        WithSubExpr(std::move(children)) {
    }

    explicit OrExpr(const std::vector<expr_node<ExprNode>>& children) :
        WithSubExpr(children) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::or_expr;
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto bits = std::make_pair(osmium::osm_entity_bits::nothing, osmium::osm_entity_bits::nothing);
        return std::accumulate(children().begin(), children().end(), bits, [](entity_bits_pair b, const std::unique_ptr<ExprNode>& e) {
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

    std::unique_ptr<ExprNode> m_expr;

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "BOOL_NOT\n";
        expr()->print(out, level + 1);
    }


public:

    explicit NotExpr(std::unique_ptr<ExprNode>&& expr) :
        m_expr(std::move(expr)) {
        assert(m_expr);
    }

    explicit NotExpr(expr_node<ExprNode> expr) :
        m_expr(expr.release()) {
        assert(m_expr);
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::not_expr;
    }

    const ExprNode* expr() const noexcept {
        return m_expr.get();
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto e = expr()->calc_entities();
        return std::make_pair(e.second, e.first);
    }

    void prepare() override final {
        m_expr->prepare();
    }

    bool eval_bool(const osmium::OSMObject& object) const override final {
        return !expr()->eval_bool(object);
    }

    bool eval_bool(const osmium::Tag& tag) const override final {
        return !expr()->eval_bool(tag);
    }

    bool eval_bool(const osmium::NodeRef& nr) const override final {
        return !expr()->eval_bool(nr);
    }

    bool eval_bool(const osmium::RelationMember& member) const override final {
        return !expr()->eval_bool(member);
    }

}; // class NotExpr

class IntegerValue : public IntegerExpression {

    std::int64_t m_value;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "INT_VALUE[" << m_value << "]\n";
    }

public:

    explicit IntegerValue(std::int64_t value) :
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

    explicit StringValue(const std::string& value) :
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

    explicit RegexValue(const std::regex& value) :
        m_str("UNKNOWN"),
        m_value(value) {
    }

    explicit RegexValue(const std::string& value) :
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

class IntegerAttribute : public IntegerExpression {

    integer_attribute_type m_attribute;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "INT_ATTR[" << attribute_name(m_attribute) << "]\n";
    }

public:

    explicit IntegerAttribute(integer_attribute_type attr) noexcept :
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

    explicit StringAttribute(string_attribute_type attr) noexcept :
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

class BooleanAttribute : public BoolExpression {

    boolean_attribute_type m_attribute;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "BOOL_ATTR[" << attribute_name(m_attribute) << "]\n";
    }

public:

    explicit BooleanAttribute(boolean_attribute_type attr) noexcept :
        m_attribute(attr) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::boolean_attribute;
    }

    boolean_attribute_type attribute() const noexcept {
        return m_attribute;
    }

    entity_bits_pair calc_entities() const noexcept override final {
        switch (m_attribute) {
            case boolean_attribute_type::node:
                return std::make_pair(osmium::osm_entity_bits::node, ~osmium::osm_entity_bits::node);
            case boolean_attribute_type::way:
                return std::make_pair(osmium::osm_entity_bits::way, ~osmium::osm_entity_bits::way);
            case boolean_attribute_type::relation:
                return std::make_pair(osmium::osm_entity_bits::relation, ~osmium::osm_entity_bits::relation);
            case boolean_attribute_type::visible:
                return std::make_pair(osmium::osm_entity_bits::nwr, osmium::osm_entity_bits::nwr);
            case boolean_attribute_type::closed_way:
                return std::make_pair(osmium::osm_entity_bits::way, ~osmium::osm_entity_bits::way);
            case boolean_attribute_type::open_way:
                return std::make_pair(osmium::osm_entity_bits::way, ~osmium::osm_entity_bits::way);
        }

        assert(false);
    }

    bool eval_bool(const osmium::OSMObject& object) const override final {
        switch (m_attribute) {
            case boolean_attribute_type::node:
                return object.type() == osmium::item_type::node;
            case boolean_attribute_type::way:
                return object.type() == osmium::item_type::way;
            case boolean_attribute_type::relation:
                return object.type() == osmium::item_type::relation;
            case boolean_attribute_type::visible:
                return object.visible();
            case boolean_attribute_type::closed_way:
                return object.type() == osmium::item_type::way && static_cast<const osmium::Way&>(object).is_closed();
            case boolean_attribute_type::open_way:
                return object.type() == osmium::item_type::way && !static_cast<const osmium::Way&>(object).is_closed();
        }

        assert(false);
    }

}; // class BooleanAttribute

class BinaryIntOperation : public BoolExpression {

    std::unique_ptr<ExprNode> m_lhs;
    std::unique_ptr<ExprNode> m_rhs;
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

    explicit BinaryIntOperation(std::unique_ptr<ExprNode>&& lhs,
                                integer_op_type op,
                                std::unique_ptr<ExprNode>&& rhs) noexcept :
        m_lhs(std::move(lhs)),
        m_rhs(std::move(rhs)),
        m_op(op) {
        assert(lhs);
        assert(rhs);
    }

    explicit BinaryIntOperation(const std::tuple<expr_node<ExprNode>, integer_op_type, expr_node<ExprNode>>& params) noexcept :
        m_lhs(std::get<0>(params).release()),
        m_rhs(std::get<2>(params).release()),
        m_op(std::get<1>(params)) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::binary_int_op;
    }

    ExprNode* lhs() const noexcept {
        return m_lhs.get();
    }

    ExprNode* rhs() const noexcept {
        return m_rhs.get();
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto l = lhs()->calc_entities();
        const auto r = rhs()->calc_entities();
        return std::make_pair(l.first & r.first, l.second & r.second);
    }

    integer_op_type op() const noexcept {
        return m_op;
    }

    void prepare() override final {
        lhs()->prepare();
        rhs()->prepare();
    }

    bool eval_bool(const osmium::OSMObject& object) const override final {
        return compare(lhs()->eval_int(object),
                       rhs()->eval_int(object));
    }

    bool eval_bool(const osmium::NodeRef& nr) const override final {
        return compare(lhs()->eval_int(nr),
                       rhs()->eval_int(nr));
    }

    bool eval_bool(const osmium::RelationMember& member) const override final {
        return compare(lhs()->eval_int(member),
                       rhs()->eval_int(member));
    }

}; // class BinaryIntOperation

class BinaryStrOperation : public BoolExpression {

    std::unique_ptr<ExprNode> m_lhs;
    std::unique_ptr<ExprNode> m_rhs;
    string_op_type m_op;

    template <typename T>
    bool eval_bool_impl(const T& t) const {
        const char* value = lhs()->eval_string(t);

        switch (m_op) {
            case string_op_type::equal:
                return !std::strcmp(value, m_rhs->eval_string(t));
            case string_op_type::not_equal:
                return std::strcmp(value, m_rhs->eval_string(t));
            case string_op_type::prefix_equal:
                return !std::strncmp(value, m_rhs->eval_string(t), std::strlen(m_rhs->eval_string(t)));
            case string_op_type::prefix_not_equal:
                return std::strncmp(value, m_rhs->eval_string(t), std::strlen(m_rhs->eval_string(t)));
            case string_op_type::match:
                return std::regex_search(value, *(dynamic_cast<RegexValue*>(rhs())->value()));
            case string_op_type::not_match:
                return !std::regex_search(value, *(dynamic_cast<RegexValue*>(rhs())->value()));
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

    explicit BinaryStrOperation(std::unique_ptr<ExprNode>&& lhs,
                                string_op_type op,
                                std::unique_ptr<ExprNode>&& rhs) noexcept :
        m_lhs(std::move(lhs)),
        m_rhs(std::move(rhs)),
        m_op(op) {
        assert(lhs);
        assert(rhs);
    }

    explicit BinaryStrOperation(const std::tuple<expr_node<ExprNode>, string_op_type, expr_node<ExprNode>>& params) noexcept :
        m_lhs(std::get<0>(params).release()),
        m_rhs(std::get<2>(params).release()),
        m_op(std::get<1>(params)) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::binary_str_op;
    }

    ExprNode* lhs() const noexcept {
        return m_lhs.get();
    }

    ExprNode* rhs() const noexcept {
        return m_rhs.get();
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto l = lhs()->calc_entities();
        const auto r = rhs()->calc_entities();
        return std::make_pair(l.first & r.first, l.second & r.second);
    }

    string_op_type op() const noexcept {
        return m_op;
    }

    void prepare() override final {
        lhs()->prepare();
        rhs()->prepare();
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

class TagsExpr : public IntegerExpression {

    std::unique_ptr<ExprNode> m_expr;

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "COUNT_TAGS\n";
        expr()->print(out, level + 1);
    }

public:

    TagsExpr() :
        m_expr(new BooleanValue) {
    }

    explicit TagsExpr(std::unique_ptr<ExprNode>&& expr) :
        m_expr(std::move(expr)) {
        assert(m_expr);
    }

    explicit TagsExpr(expr_node<ExprNode> expr) :
        m_expr(expr.release()) {
        assert(m_expr);
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::tags_expr;
    }

    ExprNode* expr() const noexcept {
        return m_expr.get();
    }

    void prepare() override final {
        m_expr->prepare();
    }

    std::int64_t eval_int(const osmium::OSMObject& object) const override final {
        return std::count_if(object.tags().cbegin(), object.tags().cend(), [this](const osmium::Tag& tag){
            return expr()->eval_bool(tag);
        });
    }

}; // class TagsExpr

class NodesExpr : public IntegerExpression {

    std::unique_ptr<ExprNode> m_expr;

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "COUNT_NODES\n";
        expr()->print(out, level + 1);
    }

public:

    NodesExpr() :
        m_expr(new BooleanValue) {
    }

    explicit NodesExpr(std::unique_ptr<ExprNode>&& expr) :
        m_expr(std::move(expr)) {
        assert(m_expr);
    }

    explicit NodesExpr(expr_node<ExprNode> expr) :
        m_expr(expr.release()) {
        assert(m_expr);
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::nodes_expr;
    }

    ExprNode* expr() const noexcept {
        return m_expr.get();
    }

    void prepare() override final {
        m_expr->prepare();
    }

    std::int64_t eval_int(const osmium::OSMObject& object) const override final {
        if (object.type() != osmium::item_type::way) {
            return 0;
        }

        const auto& nodes = static_cast<const osmium::Way&>(object).nodes();
        return std::count_if(nodes.cbegin(), nodes.cend(), [this](const osmium::NodeRef& nr){
            return expr()->eval_bool(nr);
        });
    }

    entity_bits_pair calc_entities() const noexcept override final {
        const auto e = osmium::osm_entity_bits::way;
        return std::make_pair(e, ~e);
    }

}; // class NodesExpr

class MembersExpr : public IntegerExpression {

    std::unique_ptr<ExprNode> m_expr;

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "COUNT_MEMBERS\n";
        expr()->print(out, level + 1);
    }

public:

    MembersExpr() :
        m_expr(new BooleanValue) {
    }

    explicit MembersExpr(std::unique_ptr<ExprNode>&& expr) :
        m_expr(std::move(expr)) {
        assert(m_expr);
    }

    explicit MembersExpr(expr_node<ExprNode> expr) :
        m_expr(expr.release()) {
        assert(m_expr);
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::members_expr;
    }

    ExprNode* expr() const noexcept {
        return m_expr.get();
    }

    void prepare() override final {
        m_expr->prepare();
    }

    std::int64_t eval_int(const osmium::OSMObject& object) const override final {
        if (object.type() != osmium::item_type::relation) {
            return 0;
        }

        const auto& members = static_cast<const osmium::Relation&>(object).members();
        return std::count_if(members.cbegin(), members.cend(), [this](const osmium::RelationMember& member){
            return expr()->eval_bool(member);
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
        out << "HAS_KEY[" << m_key << "]\n";
    }

public:

    explicit CheckHasKeyExpr(const std::string& str) :
        m_key(str) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_has_key;
    }

    const char* key() const noexcept {
        return m_key.c_str();
    }

    bool eval_bool(const osmium::OSMObject& object) const noexcept override final {
        return object.tags().has_key(m_key.c_str());
    }

}; // class CheckHasKeyExpr

class CheckTagStrExpr : public BoolExpression {

    std::string m_key;
    std::string m_value;
    string_op_type m_op;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "CHECK_TAG[" << m_key << "][" << operator_name(m_op) << "][" << m_value << "]\n";
    }

public:

    explicit CheckTagStrExpr(const std::string& key,
                             string_op_type op,
                             const std::string& value) :
        m_key(key),
        m_value(value),
        m_op(op) {
    }

    explicit CheckTagStrExpr(const std::tuple<std::string, string_op_type, std::string>& params) :
        CheckTagStrExpr(std::get<0>(params), std::get<1>(params), std::get<2>(params)) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_tag_str;
    }

    const char* key() const noexcept {
        return m_key.c_str();
    }

    string_op_type op() const noexcept {
        return m_op;
    }

    const char* value() const noexcept {
        return m_value.c_str();
    }

    bool eval_bool(const osmium::OSMObject& object) const noexcept override final {
        const char* tag_value = object.tags().get_value_by_key(m_key.c_str());
        if (!tag_value) {
            return false;
        }
        const bool has_tag = !std::strcmp(tag_value, value());
        return m_op == string_op_type::equal ? has_tag : !has_tag;
    }

}; // class CheckTagStrExpr

class CheckTagRegexExpr : public BoolExpression {

    std::string m_key;
    std::string m_value;
    std::regex m_value_regex;
    string_op_type m_op;
    bool m_case_insensitive;

protected:

    void do_print(std::ostream& out, int /*level*/) const override final {
        out << "CHECK_TAG[" << m_key << "][" << operator_name(m_op) << "][" << m_value << "][" << (m_case_insensitive ? "IGNORE_CASE" : "") << "]\n";
    }

public:

    explicit CheckTagRegexExpr(const std::string& key,
                               string_op_type op,
                               const std::string& value,
                               const boost::optional<char>& ci) :
        m_key(key),
        m_value(value),
        m_value_regex(),
        m_op(op),
        m_case_insensitive(ci == 'i') {
        auto options = std::regex::nosubs | std::regex::optimize;
        if (m_case_insensitive) {
            options |= std::regex::icase;
        }
        m_value_regex = std::regex{m_value, options};
    }

    explicit CheckTagRegexExpr(const std::tuple<std::string, string_op_type, std::string, boost::optional<char>>& params) :
        CheckTagRegexExpr(std::get<0>(params), std::get<1>(params), std::get<2>(params), std::get<3>(params)) {
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::check_tag_regex;
    }

    const char* key() const noexcept {
        return m_key.c_str();
    }

    string_op_type op() const noexcept {
        return m_op;
    }

    const std::regex* value_regex() const noexcept {
        return &m_value_regex;
    }

    bool case_insensitive() const noexcept {
        return m_case_insensitive;
    }

    bool eval_bool(const osmium::OSMObject& object) const noexcept override final {
        const char* tag_value = object.tags().get_value_by_key(m_key.c_str());
        if (!tag_value) {
            return false;
        }
        const bool has_tag = std::regex_search(tag_value, m_value_regex);
        return m_op == string_op_type::match ? has_tag : !has_tag;
    }

}; // class CheckTagRegexExpr

class InIntegerList : public BoolExpression {

    std::unique_ptr<ExprNode> m_attr;
    std::unique_ptr<osmium::index::IdSet<std::uint64_t>> m_values;
    std::string m_filename;
    list_op_type m_op;

protected:

    void do_print(std::ostream& out, int level) const override final {
        out << "IN_INT_LIST[" << operator_name(m_op) << "]\n";
        m_attr->print(out, level + 1);
        indent(out, level + 1);
        if (m_filename.empty()) {
            out << "VALUES[";
            auto* ids = dynamic_cast<osmium::index::IdSetSmall<std::uint64_t>*>(m_values.get());
            if (ids) {
                auto it = ids->cbegin();
                if (it != ids->cend()) {
                    out << *it;
                    ++it;
                }
                for (int i = 4; i > 0 && it != ids->cend(); ++it, --i) {
                    out << ", " << *it;
                }
                if (it != ids->cend()) {
                    out << ", ...";
                }
            } else {
                out << "...";
            }
            out << "]\n";
        } else {
            out << "FROM_FILE[" << m_filename << "]\n";
        }
    }

    void load_file() {
        std::uint64_t value;
        std::ifstream input{m_filename};
        while (input >> value) {
            m_values->set(value);
        }
    }

public:

    explicit InIntegerList(std::unique_ptr<ExprNode>& attr, list_op_type op, const std::vector<std::int64_t>& values) :
        m_attr(std::move(attr)),
        m_values(new osmium::index::IdSetSmall<std::uint64_t>),
        m_filename(),
        m_op(op) {
        assert(m_attr);
        for (auto value : values) {
            m_values->set(std::uint64_t(value));
        }
    }

    explicit InIntegerList(const std::tuple<expr_node<ExprNode>, list_op_type, std::vector<std::int64_t>>& params) :
        m_attr(std::get<0>(params).release()),
        m_values(new osmium::index::IdSetSmall<std::uint64_t>),
        m_filename(),
        m_op(std::get<1>(params)) {
        assert(m_attr);
        for (auto value : std::get<2>(params)) {
            m_values->set(std::uint64_t(value));
        }
    }

    explicit InIntegerList(const std::tuple<expr_node<ExprNode>, list_op_type, std::string>& params) :
        m_attr(std::get<0>(params).release()),
        m_values(),
        m_filename(std::get<2>(params)),
        m_op(std::get<1>(params)) {
        assert(m_attr);
    }

    expr_node_type expression_type() const noexcept override final {
        return expr_node_type::in_integer_list;
    }

    void prepare() override final {
        m_attr->prepare();
        if (!m_filename.empty()) {
            if (!m_values) {
                m_values.reset(new osmium::index::IdSetDense<std::uint64_t>);
            } else {
                m_values->clear();
            }
            load_file();
        }
    }

    bool eval_bool(const osmium::OSMObject& object) const noexcept override final {
        assert(m_values);
        const std::int64_t value = m_attr->eval_int(object);
        const bool comp = m_values->get(std::uint64_t(value));
        return comp == (m_op == list_op_type::in);
    }

}; // class InIntegerList


class OSMObjectFilter {

    std::unique_ptr<ExprNode> m_root = std::unique_ptr<ExprNode>(new BooleanValue);

public:

    explicit OSMObjectFilter(const std::string& input);

    const ExprNode* root() const noexcept {
        return m_root.get();
    }

    void print_tree(std::ostream& out) const {
        m_root->print(out, 0);
    }

    osmium::osm_entity_bits::type entities() const noexcept {
        return m_root->calc_entities().first;
    }

    void prepare() {
        return m_root->prepare();
    }

    bool match(const osmium::OSMObject& object) const {
        return m_root->eval_bool(object);
    }

}; // class OSMObjectFilter

