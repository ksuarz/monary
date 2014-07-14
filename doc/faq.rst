Frequently Asked Questions
==========================

.. contents::

Can Monary insert documents into MongoDB?
-----------------------------------------
Though there may be support for bulk inserts from arrays into MongoDB in the
future, for now Monary can only retrieve data. It cannot perform any inserts. In
the meantime, use `PyMongo <http://api.mongodb.org/python/current/>`_.


Why does my array contain masked values?
----------------------------------------
Typically, a value is masked if the data type you specify for a field is
incompatible with the actual type retrieved from the document in MongoDB.

If the entire array is masked, then all of the matching fields in the database
have an incompatible type.

If there are only some masked values in the result array, then some of the
documents have fields with the specified name but not of the specified type.

Consider, for example, inserting the following two documents at the mongo
shell::

    > db.foo.insert({ a : NumberInt(1) });
    WriteResult({ "nInserted" : 1 })

    > db.foo.insert({ a : "hello" })
    WriteResult({ "nInserted" : 1 })

Because there is a type mismatch, some values will be masked depending on what
type the query asks for::

    >>> import monary
    >>> m = monary.Monary("localhost")
    >>> m.query("test", "foo", {}, ["a"], ["int32"], sort="sequence")
    [masked_array(data = [1 --],
                 mask = [False  True],
           fill_value = 99999)
    ]
    >>> m.query("test", "foo", {}, ["a"], ["string:6"], sort="sequence")
    [masked_array(data = [-- 'hello'],
                 mask = [ True False],
           fill_value = N/A)
    ]


What if I don't know what type of data I want from MongoDB?
-----------------------------------------------------------
MongoDB has very flexible schemas; a consequence of this is that documents in
the same collection can have fields of different types. To infer the type of
data for a certain field name, specify the type of "type"::

    >>> m.query("test", "foo", {}, ["a"], ["type"])
    [masked_array(data = [16 2]
                 mask = [False False],
           fill_value = 999999)
    ]

This returns an 8-bit integer containing the BSON type code for the object.

.. seealso::

    The `BSON specification <http://bsonspec.org/spec.html>`_ for the
    BSON type codes.


How do I retrieve strings data using Monary?
--------------------------------------------
Internally, all strings are `C strings
<http://en.wikipedia.org/wiki/C_string#Definitions>`_.  To specify a string
type, you must also indicate the size of the string **including** the
terminating ``NUL`` character.::

    >>> m.query("test", "foo", {}, ["mystr"], ["string:4"])
    [masked_array(data = ['foo' 'bar' 'baz'],
                 mask = [False False False],
           fill_value = N/A)
    ]

Ideally, the size specified should be the least upper bound
of the sizes of strings you are expecting to receive.
