
# Osmium Filter

Work-in-progress superfast filter for OSM data.

## Dependencies

* [libosmium](https://github.com/osmcode/libosmium)
* [NativeJIT](https://github.com/bitfunnel/nativejit/)


## Building

Get and build NativeJIT first.

Then:

    mkdir build
    cd build
    cmake ..
    make

You'll probably have to set the include paths libs for NativeJIT on the `cmake`
command line or using `ccmake` for this to work.

The `osmium-filter` binary is created in the `src` directory.


## Run

Usage:

    osmium-filter INPUT-FILE -o OUTPUT-FILE -e FILTER-EXPRESSION

or

    osmium-filter INPUT-FILE -o OUTPUT-FILE -E FILTER-EXPRESSION-FILE

Call with `--help` to get usage info.


## Language


    A | B       A or B

    A & B       A and B

    A B         Same as A & B

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

