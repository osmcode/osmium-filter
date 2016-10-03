#pragma once

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
    NativeJIT::Node<bool>& check_object_type(const CheckObjectTypeExpr* e);
    NativeJIT::Node<bool>& check_has_key(const CheckHasKeyExpr* e);
    NativeJIT::Node<bool>& check_tag_str(const CheckTagStrExpr* e);
    NativeJIT::Node<bool>& check_tag_regex(const CheckTagRegexExpr* e);
    NativeJIT::Node<bool>& check_attr_int(const CheckAttrIntExpr* e);

    NativeJIT::Node<bool>& compile(const ExprNode* node);

public:

    CompiledFilter(const OSMObjectFilter& filter) {
        m_function = m_expression.Compile(compile(filter.root()));
    }

    bool match(osmium::OSMObject& object) {
        return m_function(object);
    }

}; // CompiledFilter

