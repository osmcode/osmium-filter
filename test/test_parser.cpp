
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
    check("@node",          eb::node,           "BOOL_ATTR[node]");
    check("@way",           eb::way,            "BOOL_ATTR[way]");
    check("@relation",      eb::relation,       "BOOL_ATTR[relation]");
    check("@node or @way",  eb::node | eb::way, "BOOL_OR\n BOOL_ATTR[node]\n BOOL_ATTR[way]");
    check("@node and @way", eb::nothing,        "BOOL_AND\n BOOL_ATTR[node]\n BOOL_ATTR[way]");
}

TEST_CASE("integer comparison") {
    check("@id == 1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id != 1", eb::nwr, "INT_BIN_OP[not_equal]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id <  1", eb::nwr, "INT_BIN_OP[less_than]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id >  1", eb::nwr, "INT_BIN_OP[greater_than]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id <= 1", eb::nwr, "INT_BIN_OP[less_or_equal]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@id >= 1", eb::nwr, "INT_BIN_OP[greater_or_equal]\n INT_ATTR[id]\n INT_VALUE[1]");
}

TEST_CASE("integer list comparison") {
    check("@id in (71, 28)",      eb::nwr, "IN_INT_LIST[in]\n INT_ATTR[id]\n VALUES[71, 28]");
    check("@id not in (71, 28)",  eb::nwr, "IN_INT_LIST[not_in]\n INT_ATTR[id]\n VALUES[71, 28]");
    check("not @id in (71, 28)",  eb::nwr, "BOOL_NOT\n IN_INT_LIST[in]\n  INT_ATTR[id]\n  VALUES[71, 28]");
    check("@id in (<'somefile')", eb::nwr, "IN_INT_LIST[in]\n INT_ATTR[id]\n FROM_FILE[somefile]");
}

TEST_CASE("string comparison") {
    check("@user == 'foo'", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user != 'foo'", eb::nwr, "BIN_STR_OP[not_equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user =^ 'foo'", eb::nwr, "BIN_STR_OP[prefix_equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user !^ 'foo'", eb::nwr, "BIN_STR_OP[prefix_not_equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user =~ 'foo'", eb::nwr, "BIN_STR_OP[match]\n STR_ATTR[user]\n REGEX_VALUE[foo]");
    check("@user !~ 'foo'", eb::nwr, "BIN_STR_OP[not_match]\n STR_ATTR[user]\n REGEX_VALUE[foo]");
}

TEST_CASE("string value") {
    check("@user == 'foo'", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user == \"foo\"", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user == foo", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[foo]");
    check("@user == ' foo'", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[ foo]");
    check("@user == ' foo '", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[ foo ]");
    check("@user == '1 2 3'", eb::nwr, "BIN_STR_OP[equal]\n STR_ATTR[user]\n STR_VALUE[1 2 3]");
}

TEST_CASE("simple integer attributes") {
    check("@id        == 1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[id]\n INT_VALUE[1]");
    check("@version   == 1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[version]\n INT_VALUE[1]");
    check("@uid       == 1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[uid]\n INT_VALUE[1]");
    check("@changeset == 1", eb::nwr, "INT_BIN_OP[equal]\n INT_ATTR[changeset]\n INT_VALUE[1]");
}

TEST_CASE("boolean attributes") {
    check("@visible",     eb::nwr, "BOOL_ATTR[visible]");
    check("not @visible", eb::nwr, "BOOL_NOT\n BOOL_ATTR[visible]");
    check("@closed_way",  eb::way, "BOOL_ATTR[closed_way]");
    check("@closed_way or (@relation and 'type' == 'multipolygon')", eb::way | eb::relation, "BOOL_OR\n BOOL_ATTR[closed_way]\n BOOL_AND\n  BOOL_ATTR[relation]\n  CHECK_TAG[type][equal][multipolygon]");
    check("@open_way",    eb::way, "BOOL_ATTR[open_way]");
}

TEST_CASE("has key") {
    check("'highway'", eb::nwr, "HAS_KEY[highway]");
    check("highway",   eb::nwr, "HAS_KEY[highway]");
    check("'highway' == 'primary'", eb::nwr, "CHECK_TAG[highway][equal][primary]");
    check(" highway  ==  primary ", eb::nwr, "CHECK_TAG[highway][equal][primary]");
    check("'highway' != 'primary'", eb::nwr, "CHECK_TAG[highway][not_equal][primary]");
    check("'highway' =~ 'primary'", eb::nwr, "CHECK_TAG[highway][match][primary][]");
    check("'highway' !~ 'primary'", eb::nwr, "CHECK_TAG[highway][not_match][primary][]");
    check("'highway' =~ 'primary'i", eb::nwr, "CHECK_TAG[highway][match][primary][IGNORE_CASE]");
    check("'highway' !~ 'primary'i", eb::nwr, "CHECK_TAG[highway][not_match][primary][IGNORE_CASE]");
}

TEST_CASE("tags with subexpression") {
    check("@tags[ @key == 'highway' ] >  0", eb::nwr, "INT_BIN_OP[greater_than]\n COUNT_TAGS\n  BIN_STR_OP[equal]\n   STR_ATTR[key]\n   STR_VALUE[highway]\n INT_VALUE[0]");
    check("@tags[ @key == 'highway' ] == 0", eb::nwr, "INT_BIN_OP[equal]\n COUNT_TAGS\n  BIN_STR_OP[equal]\n   STR_ATTR[key]\n   STR_VALUE[highway]\n INT_VALUE[0]");
}

TEST_CASE("tags without subexpression") {
    check("@tags >  0", eb::nwr, "INT_BIN_OP[greater_than]\n COUNT_TAGS\n  TRUE\n INT_VALUE[0]");
    check("@tags == 0", eb::nwr, "INT_BIN_OP[equal]\n COUNT_TAGS\n  TRUE\n INT_VALUE[0]");
}

