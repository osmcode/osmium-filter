
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
// XXX    check("# foo", eb::nwr, "TRUE");
}

TEST_CASE("boolean expressions") {
    check("true and true", eb::nwr, "BOOL_AND\n TRUE\n TRUE");
    check("true and false and true", eb::nwr, "BOOL_AND\n TRUE\n FALSE\n TRUE");
    check("true or false", eb::nwr, "BOOL_OR\n TRUE\n FALSE");
    check("true or (false and false)", eb::nwr, "BOOL_OR\n TRUE\n BOOL_AND\n  FALSE\n  FALSE");
    check("(true or false) and (false or true)", eb::nwr, "BOOL_AND\n BOOL_OR\n  TRUE\n  FALSE\n BOOL_OR\n  FALSE\n  TRUE");
    check("true or not true", eb::nwr, "BOOL_OR\n TRUE\n BOOL_NOT\n  TRUE");
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
    check("@id =  1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id != 1", eb::nwr, "INT_BIN_OP[not_equal]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id <  1", eb::nwr, "INT_BIN_OP[less_than]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id >  1", eb::nwr, "INT_BIN_OP[greater_than]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id <= 1", eb::nwr, "INT_BIN_OP[less_or_equal]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id >= 1", eb::nwr, "INT_BIN_OP[greater_or_equal]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id in (71, 28)", eb::nwr, "IN_INT_LIST\n INT_ATTR[id]\n VALUES[...]");
// XXX    check("@id in 'filename'", eb::nwr, "IN_INT_LIST\n INT_ATTR[id]\n FROM_FILE[filename]");
}

TEST_CASE("string comparison") {
    check("@user =   'foo'", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user !=  'foo'", eb::nwr, "BIN_STR_OP[not_equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user ^=  'foo'", eb::nwr, "BIN_STR_OP[prefix_equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user !^= 'foo'", eb::nwr, "BIN_STR_OP[prefix_not_equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user ~   'foo'", eb::nwr, "BIN_STR_OP[match]\n STR_ATTR[user]\n REGEX_VALUE[foo]");
    check("@user !~  'foo'", eb::nwr, "BIN_STR_OP[not_match]\n STR_ATTR[user]\n REGEX_VALUE[foo]");
}

TEST_CASE("string value") {
    check("@user = 'foo'", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user = \"foo\"", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user = foo", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
}

TEST_CASE("simple integer attributes") {
    check("@id        = 1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@version   = 1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[version]\n INT_VALUE[1]");
    check("@uid       = 1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[uid]\n INT_VALUE[1]");
    check("@changeset = 1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[changeset]\n INT_VALUE[1]");
}

TEST_CASE("has key") {
    check("'highway'", eb::nwr, "HAS_KEY[highway]");
    check("'highway' =  'primary'", eb::nwr, "CHECK_TAG[highway][equal][primary]");
    check("'highway' != 'primary'", eb::nwr, "CHECK_TAG[highway][not_equal][primary]");
    check("'highway' ~  'primary'", eb::nwr, "CHECK_TAG[highway][match][primary][]");
    check("'highway' !~ 'primary'", eb::nwr, "CHECK_TAG[highway][not_match][primary][]");
    check("'highway' ~  'primary'i", eb::nwr, "CHECK_TAG[highway][match][primary][IGNORE_CASE]");
    check("'highway' !~ 'primary'i", eb::nwr, "CHECK_TAG[highway][not_match][primary][IGNORE_CASE]");
}

TEST_CASE("closed way") {
    check("closed_way", eb::way, "CLOSED_WAY");
    check("closed_way or (relation and 'type'='multipolygon')", eb::way | eb::relation, "BOOL_OR\n CLOSED_WAY\n BOOL_AND\n  HAS_TYPE[relation]\n  CHECK_TAG[type][equal][multipolygon]");
}

