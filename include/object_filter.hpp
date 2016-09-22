
#include <regex>
#include <stack>
#include <string>
#include <tuple>
#include <vector>

#include <NativeJIT/CodeGen/ExecutionBuffer.h>
#include <NativeJIT/CodeGen/FunctionBuffer.h>
#include <NativeJIT/Function.h>
#include <Temporary/Allocator.h>

namespace boost {
    template <class T>
    class optional;
}

namespace osmium {
    class OSMObject;
}

struct Context {
    std::vector<std::string> strings;
    std::vector<std::regex> regexes;
};

class OSMObjectFilter {

    NativeJIT::ExecutionBuffer m_code_allocator{8192};
    NativeJIT::Allocator m_allocator{8192};
    NativeJIT::FunctionBuffer m_code{m_code_allocator, 8192};
    NativeJIT::Function<bool, const osmium::OSMObject&> m_expression{m_allocator, m_code};

    std::stack<NativeJIT::Node<bool>*> m_expression_stack;

    Context m_context;

    const Context* context() const noexcept {
        return &m_context;
    }

    NativeJIT::Function<bool, const osmium::OSMObject&>::FunctionType m_function;

    bool m_verbose;

    void boolean_and();
    void boolean_or();
    void boolean_not();

    void check_has_key(const std::string& str);
    void check_tag_str(const std::tuple<std::string, std::string, std::string>& value);
    void check_tag_regex(const std::tuple<std::string, std::string, std::string, boost::optional<char>>& value);
    void check_attr_int(const std::tuple<std::string, std::string, int64_t>& value);
    void check_object_type(const std::string& type);
    void check_id(int64_t value);

public:

    OSMObjectFilter(std::string& input, bool verbose);

    bool match(osmium::OSMObject& object) {
        return m_function(object);
    }

}; // class OSMObjectFilter

