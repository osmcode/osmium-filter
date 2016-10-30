
This is a very preliminary description of the expression language. It is only
partially implemented and things will change.

# OSM Filter Language (OFL)

    way and @version > 10


## Boolean composition

    EXPR1 or EXPR2
    EXPR1 and EXPR2
    not EXPR
    (EXPR)


## Type checks

Return a boolean

    @type == node
    @type == way
    @type == relation
    node
    way
    relation


## Integer comparisons

Return a boolean

    INT == INT            - is equal
    INT != INT            - is not equal
    INT <  INT            - is less than
    INT >  INT            - is greater than
    INT <= INT            - is less or equal
    INT >= INT            - is greater or equal
    INT in (INT, ...)     - is in the list

The following are not implemented. Do we need them?

    VALUE not in (INT, ...) - is not in the list
    VALUE any_of (INT, ...)
    VALUE all_of (INT, ...)
    VALUE none_of (INT, ...)

# Time comparisons

NOT YET IMPLEMENTED

Return a boolean

    VALUE == TIMESTAMP            - is equal
    VALUE != TIMESTAMP            - is not equal
    VALUE <  TIMESTAMP            - is less than
    VALUE >  TIMESTAMP            - is greater than
    VALUE <= TIMESTAMP            - is less or equal
    VALUE >= TIMESTAMP            - is greater or equal
    VALUE in (TIMESTAMP, ...)     - is in the list
    VALUE in (@FILENAME)    - is in the file
    VALUE TS-TS ??
    VALUE TS:TS ??

# String comparisons

Return a boolean

    VALUE == "STRING"       - is equal
    VALUE != "STRING"       - is not equal
    VALUE =^ "STRING"       - prefix equal
    VALUE !^ "STRING"       - prefix not equal
    VALUE =~ "STRING"       - matches the regular expression
    VALUE !~ "STRING"       - does not match the regular expression

    VALUE =~ "STRING"i      - matches the regular expression
    VALUE =~ /STRING/       - matches the regular expression

Not yet implemented:
    VALUE in ("STRING", "STRING", ...)

## Boolean attributes

Return a boolean

    @visible
    @deleted

## Integer attributes

Return an int value

    @id


## Unsigned integer attributes

Return an int value

    @version
    @uid
    @changeset


## Time attributes

Not yet implemented.

Return an time value

    @timestamp


## String attributes

Return an string value

    @user


## "Array" attributes

Return an integer (the count), "decay" to boolean in boolean context

    @tags
    @nodes
    @members

    @tags[]
    @nodes[]
    @members[]

    @tags[] = 3
    @nodes[] > 17
    @members[] <= 2

    @tags["highway"] > @tags["oneway"]


## Tag checks

    @tags - has any tags
    @tags[CONDITION] - condition is true for tags

    @tags[@key = "highway"]
    @tags[@key != "highway"]
    @tags[@key ~ "^addr:"] > 3
    @tags[@key !~ "^addr:"]

    @tags[@key = "highway" and @value = "service"]
    @tags[@key = "highway" and @value ~ "_link$"]
    @tags[@key = "highway" and @value !~ "_link$"]


## Nodes checks

Can only be true for ways

    @nodes[@ref=17]
    @nodes[17]
    @nodes[@ref in (17, 18, 20)]
    @nodes[@ref > 100]

Return all ways that have at least one of these nodes in them:

    @nodes[@ref in (17, 18, 20)]

Return all ways that have all of these nodes in them:

    @nodes[17] and @nodes[18] and @nodes[20]


## Members checks

Can only be true for relations

    @members[@type="way"]
    @members[@ref=17]
    @members[@role="inner"]

    @members[way]
    @members[17]
    @members["inner"]


## Lists in external files

Use

    VALUE in (@filename)
    VALUE not in (@filename)

to read list of Ids or strings from external file.


## UTF-8 encoding/escaping

## Unsure

    closed_way?
        @nodes[closed]
        @type=closed_way

    "oneway" in ("yes", "true", "1")
        short form?

    Variables?

        set hw "highway" in ("motorway", "primary", "secondary")
        way and $hw and @version==1

        set ids way and "highway" = motorway { @nodes }
        node and @id in ($ids)

