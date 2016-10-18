#pragma once

#include <regex>
#include <string>

#include <NativeJIT/CodeGen/ExecutionBuffer.h>
#include <NativeJIT/CodeGen/FunctionBuffer.h>
#include <NativeJIT/Function.h>
#include <Temporary/Allocator.h>

#include <osmium/osm.hpp>

#include "object_filter.hpp"

class CompiledFilter {

    NativeJIT::ExecutionBuffer m_code_allocator{8192};
    NativeJIT::Allocator m_allocator{8192};
    NativeJIT::FunctionBuffer m_code{m_code_allocator, 8192};
    NativeJIT::Function<bool, const osmium::OSMObject&> m_expression{m_allocator, m_code};

    NativeJIT::Function<bool, const osmium::OSMObject&>::FunctionType m_function;

    NativeJIT::Node<bool>& compile_and(const AndExpr* e);
    NativeJIT::Node<bool>& compile_or(const OrExpr* e);
    NativeJIT::Node<bool>& compile_not(const NotExpr* e);

    NativeJIT::Node<std::int64_t>& compile_integer_attribute(const ExprNode* e);
    NativeJIT::Node<bool>& compile_binary_int_op(const ExprNode* e);
    NativeJIT::Node<std::int64_t>& compile_integer_value(const ExprNode* e);

    NativeJIT::Node<const char*>& compile_string_attribute(const ExprNode* e);
    NativeJIT::Node<bool>& compile_binary_str_op(const ExprNode* e);
    NativeJIT::Node<const char*>& compile_string_value(const ExprNode* e);
    NativeJIT::Node<const std::regex*>& compile_regex_value(const ExprNode* e);

    NativeJIT::Node<bool>& tags_expr(const TagsExpr* e);

    NativeJIT::Node<bool>& check_object_type(const CheckObjectTypeExpr* e);
    NativeJIT::Node<bool>& check_has_key(const CheckHasKeyExpr* e);
    NativeJIT::Node<bool>& check_tag_str(const CheckTagStrExpr* e);
    NativeJIT::Node<bool>& check_tag_regex(const CheckTagRegexExpr* e);

    NativeJIT::Node<bool>& compile_bool(const ExprNode* node);
    NativeJIT::Node<std::int64_t>& compile_int(const ExprNode* node);
    NativeJIT::Node<const char*>& compile_str(const ExprNode* node);
    NativeJIT::Node<const std::regex*>& compile_regex(const ExprNode* node);

public:

    CompiledFilter(const OSMObjectFilter& filter) {
        m_function = m_expression.Compile(compile_bool(filter.root()));
    }

    bool match(osmium::OSMObject& object) {
        return m_function(object);
    }

}; // CompiledFilter

