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
    check_has_type,
    check_has_key,
    check_tag_str,
    check_tag_regex,
    check_attr_int
};

using entity_bits_pair = std::pair<osmium::osm_entity_bits::type, osmium::osm_entity_bits::type>;

class ExprNode {

public:

    ExprNode() = default;

    virtual ~ExprNode() {
    }

    virtual expr_node_type expression_type() const noexcept = 0;

    virtual void do_print(int level) const = 0;

    virtual entity_bits_pair calc_entities() const noexcept = 0;

    osmium::osm_entity_bits::type entities() const noexcept {
        return calc_entities().first;
    }

    void print(int level) const {
        const int this_level = level;
        while (level > 0) {
            std::cerr << ' ';
            --level;
        }
        do_print(this_level);
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

};

class AndExpr : public WithSubExpr {

public:

    AndExpr(std::vector<ExprNode*> children) :
        WithSubExpr(children) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::and_expr;
    }

    void do_print(int level) const override {
        std::cerr << "AND\n";
        for (const auto& e : m_children) {
            e->print(level + 1);
        }
    }

    entity_bits_pair calc_entities() const noexcept override {
        const auto bits = std::make_pair(osmium::osm_entity_bits::all, osmium::osm_entity_bits::all);
        return std::accumulate(children().begin(), children().end(), bits, [](entity_bits_pair b, const ExprNode* e) {
            const auto x = e->calc_entities();
            return std::make_pair(b.first & x.first, b.second & x.second);
        });
    }

};

class OrExpr : public WithSubExpr {

public:

    OrExpr(std::vector<ExprNode*> children) :
        WithSubExpr(children) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::or_expr;
    }

    void do_print(int level) const override {
        std::cerr << "OR\n";
        for (const auto& e : m_children) {
            e->print(level + 1);
        }
    }

    entity_bits_pair calc_entities() const noexcept override {
        const auto bits = std::make_pair(osmium::osm_entity_bits::nothing, osmium::osm_entity_bits::nothing);
        return std::accumulate(children().begin(), children().end(), bits, [](entity_bits_pair b, const ExprNode* e) {
            const auto x = e->calc_entities();
            return std::make_pair(b.first | x.first, b.second | x.second);
        });
    }

};

class NotExpr : public ExprNode {

    ExprNode* m_child;

public:

    NotExpr(ExprNode* e) :
        m_child(e) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::not_expr;
    }

    const ExprNode* child() const noexcept {
        return m_child;
    }

    void do_print(int level) const override {
        std::cerr << "NOT\n";
        m_child->print(level + 1);
    }

    entity_bits_pair calc_entities() const noexcept override {
        auto e = m_child->calc_entities();
        return std::make_pair(e.second, e.first);
    }

};

class CheckHasKeyExpr : public ExprNode {

    std::string m_key;

public:

    CheckHasKeyExpr(const std::string& str) :
        m_key(str) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::check_has_key;
    }

    void do_print(int /*level*/) const override {
        std::cerr << "HAS_KEY \"" << m_key << "\"\n";
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

    CheckTagStrExpr(const std::tuple<std::string, std::string, std::string>& args) :
        m_key(std::get<0>(args)),
        m_oper(std::get<1>(args)),
        m_value(std::get<2>(args)) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::check_tag_str;
    }

    void do_print(int /*level*/) const override {
        std::cerr << "CHECK_TAG \"" << m_key << "\" " << m_oper << " \"" << m_value << "\"\n";
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

    CheckTagRegexExpr(const std::tuple<std::string, std::string, std::string, boost::optional<char>>& args) :
        m_key(std::get<0>(args)),
        m_oper(std::get<1>(args)),
        m_value(std::get<2>(args)),
        m_value_regex(),
        m_case_insensitive(std::get<3>(args) == 'i') {
        auto options = std::regex::nosubs | std::regex::optimize;
        if (m_case_insensitive) {
            options |= std::regex::icase;
        }
        m_value_regex = std::regex{m_value, options};
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::check_tag_regex;
    }

    void do_print(int /*level*/) const override {
        std::cerr << "CHECK_TAG \"" << m_key << "\" " << m_oper << " /" << m_value << "/" << (m_case_insensitive ? " (IGNORE CASE)" : "") << "\n";
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

    CheckAttrIntExpr(const std::tuple<std::string, std::string, int>& args) :
        m_attr(std::get<0>(args)),
        m_oper(std::get<1>(args)),
        m_value(std::get<2>(args)) {
    }

    expr_node_type expression_type() const noexcept override {
        return expr_node_type::check_attr_int;
    }

    void do_print(int /*level*/) const override {
        std::cerr << "CHECK_ATTR " << m_attr << " " << m_oper << " " << m_value << "\n";
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

class CheckIdExpr : public CheckAttrIntExpr {

public:

    CheckIdExpr(std::int64_t id) :
        CheckAttrIntExpr(std::make_tuple("@id", "=", id)){
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

    void do_print(int /*level*/) const override {
        std::cerr << "HAS_TYPE " << osmium::item_type_to_name(m_type) << "\n";
    }

    entity_bits_pair calc_entities() const noexcept override {
        auto e = osmium::osm_entity_bits::from_item_type(m_type);
        return std::make_pair(e, ~e);
    }

};


class OSMObjectFilter {

    ExprNode* m_root = nullptr;
    bool m_verbose;

public:

    OSMObjectFilter(std::string& input, bool verbose);

    const ExprNode* root() const noexcept {
        return m_root;
    }

    void print_tree() const {
        m_root->print(0);
    }

    osmium::osm_entity_bits::type entities() const noexcept {
        return m_root->entities();
    }

}; // class OSMObjectFilter

