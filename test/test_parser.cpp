
#include <sstream>
#include <string>

#include "object_filter.hpp"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

void check(const std::string& s, osmium::osm_entity_bits::type types, const std::string& tree) {
    OSMObjectFilter filter{s};
    REQUIRE(filter.entities() == types);

    std::stringstream t;
    filter.print_tree(t);

    REQUIRE(t.str() == tree + "\n");
}

namespace eb = osmium::osm_entity_bits;

TEST_CASE("spacing and comments") {
    check("true", eb::nwr, "TRUE");
    check("false", eb::nwr, "FALSE");
    check("   false  \n \t", eb::nwr, "FALSE");
    check("# foo\ntrue ", eb::nwr, "TRUE");
    check("true # foo\n", eb::nwr, "TRUE");
    check("true # foo", eb::nwr, "TRUE");
}

TEST_CASE("boolean expressions") {
    check("true and true", eb::nwr, "BOOL_AND\n TRUE\n TRUE");
    check("true and false and true", eb::nwr, "BOOL_AND\n TRUE\n FALSE\n TRUE");
    check("true or false", eb::nwr, "BOOL_OR\n TRUE\n FALSE");
    check("true or (false and false)", eb::nwr, "BOOL_OR\n TRUE\n BOOL_AND\n  FALSE\n  FALSE");
    check("(true or false) and (false or true)", eb::nwr, "BOOL_AND\n BOOL_OR\n  TRUE\n  FALSE\n BOOL_OR\n  FALSE\n  TRUE");
}

TEST_CASE("object types") {
    check("node", eb::node, "HAS_TYPE[node]");
    check("@type = node", eb::node, "HAS_TYPE[node]");
    check("way", eb::way, "HAS_TYPE[way]");
    check("@type = way", eb::way, "HAS_TYPE[way]");
    check("relation", eb::relation, "HAS_TYPE[relation]");
    check("@type = relation", eb::relation, "HAS_TYPE[relation]");
    check("node or way", eb::node | eb::way, "BOOL_OR\n HAS_TYPE[node]\n HAS_TYPE[way]");
    check("node and @type = way", eb::nothing, "BOOL_AND\n HAS_TYPE[node]\n HAS_TYPE[way]");
}

TEST_CASE("integer comparison") {
    check("@uid =  1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[uid]\n INT_VALUE[1]");
    check("@uid != 1", eb::nwr, "INT_BIN_OP[not_equal]\n INT_ATTR[uid]\n INT_VALUE[1]");
    check("@uid <  1", eb::nwr, "INT_BIN_OP[less_than]\n INT_ATTR[uid]\n INT_VALUE[1]");
    check("@uid >  1", eb::nwr, "INT_BIN_OP[greater_than]\n INT_ATTR[uid]\n INT_VALUE[1]");
    check("@uid <= 1", eb::nwr, "INT_BIN_OP[less_or_equal]\n INT_ATTR[uid]\n INT_VALUE[1]");
    check("@uid >= 1", eb::nwr, "INT_BIN_OP[greater_or_equal]\n INT_ATTR[uid]\n INT_VALUE[1]");
}

TEST_CASE("string comparison") {
    check("@user =   'foo'", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user !=  'foo'", eb::nwr, "BIN_STR_OP[not_equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user ^=  'foo'", eb::nwr, "BIN_STR_OP[prefix_equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user !^= 'foo'", eb::nwr, "BIN_STR_OP[prefix_not_equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user ~   'foo'", eb::nwr, "BIN_STR_OP[match]\n STR_ATTR[user]\n REGEX_VALUE[foo]");
    check("@user !~  'foo'", eb::nwr, "BIN_STR_OP[not_match]\n STR_ATTR[user]\n REGEX_VALUE[foo]");
}

