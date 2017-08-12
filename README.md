
# Osmium Filter

Work-in-progress experimental fast and flexible filter for OSM data.

Do not use this for production work. The expression language (see below) will change!

## Dependencies

* [libosmium](https://github.com/osmcode/libosmium)


## Building

Then in the osmium-filter directory:

    mkdir build
    cd build
    cmake ..
    make

The `osmium-filter` binary is created in the `src` directory.


## Run

Usage:

    osmium-filter INPUT-FILE -o OUTPUT-FILE -e FILTER-EXPRESSION

or

    osmium-filter INPUT-FILE -o OUTPUT-FILE -E FILTER-EXPRESSION-FILE

Will filter out only the OSM objects matching the expressions. Call with `-w`
to add all nodes referenced by any matching ways. (This will read the input
twice.)

Call with `--help` to get usage info.


## Expression Language

No real documentation yet, but this will give you some ideas:

    A or B
    A and B
    not A

    highway     Tag key exists

    "highway"   Tag key exists

    highway = residential

    @id=        Compare object ID
    @id!=
    @id<
    @id>
    @id<=
    @id>=

    @version    Same for version
    ...

    @uid        User ID

    @changeset  Changeset ID

    @nodes      Number of nodes

    @members    Number of members

    @tags       Number of tags

    123         ID (same as @id=)

    @type=node
    node

    @type=way
    way

    @type=relation
    relation

