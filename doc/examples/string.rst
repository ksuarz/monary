Strings Example
===============

This example demonstrates how to extract strings with Monary.

Setup
-----
We can use PyMongo to populate a collection with some test data containing
random strings. First, make a connection::

    >>> from pymongo import MongoClient
    >>> client = MongoClient()

Then, we can insert random strings of your choosing::

    >>> import random
    >>> import string
    >>> def rand_str(length):
    ...     return "".join(random.choice(string.lowercase)
    ...                    for c in range(0, length))
    ...
    >>> for i in range(0, 100):
    ...     s = rand_str(random.choice(range(1, 10)))
    ...     doc = {"stringdata" : s}
    ...     client.test.data.insert(doc)

MongoDB encodes strings in UTF-8, so you can use non-ASCII characters. Here is a
sample script that inserts non-ASCII strings:

.. code-block:: python

    # -*- coding: utf-8 -*-
    from pymongo import MongoClient

    client = MongoClient()
    client.test.utf8.insert({"stringdata" : "liberté"})
    client.test.utf8.insert({"stringdata" : "自由"})
    client.test.utf8.insert({"stringdata" : "ελευθερία"})

Finding String Data
-------------------

String Sizing
.............

Monary can obtain strings from MongoDB if you specify the size of the strings in
bytes.

.. note::

    The size of a string in bytes **includes** the terminating ``NUL``
    character. For example, the size of "hello" is 6.
    
.. note:: 

    Characters encoded in UTF-8 can be larger than one byte in size. For
    example, the size of "99¢" is 5 - one byte for each '9', two bytes for '¢',
    and one more byte for the terminating ``NUL`` character.

It is safe to specify larger sizes: if the length of a string is smaller than
the specified size, the extra space is padded with ``NUL`` characters.

If the length of a string is greater than the specified size, **it will be
truncated**. However, the terminating ``NUL`` character is always included. For
example, storing "hello" in a block of size 4 results in "hel" (with a byte
representation of ``hel\0``). Multi-byte UTF-8-encoded characters may also be
truncated, which will result in an invalid string.

To determine the length of a UTF-8 encoded string in Python, encode the desired
string or Unicode object, take its length and add one for the terminating
``NUL``::

    >>> def strlen_in_bytes(string):
    ...     return len(string.encode("utf-8")) + 1

Monary provides a way to query the database directly for the byte sizes of
strings.

Performing Queries
..................
If we don't know the maximum size of the strings in advance, we can
:doc:`query </examples/query>` for their size, which returns the size of the
strings in bytes::

    >>> from monary import Monary
    >>> m = Monary()
    >>> sizes, = m.query("test", "data", {}, ["stringdata"], ["size"])
    >>> sizes
    masked_array(data = [10L 8L 4L ..., 7L 6L 10L],
                 mask = [False False False ... False False False],
           fill_value = 999999)

Now that we have the sizes of all the strings, we can find the maximum string
size::

    >>> import numpy
    >>> max_size = numpy.amax(sizes)

Finally, we can use this size to obtain the actual strings from MongoDB::

    >>> data, = m.query("test", "data", {}, ["stringdata"],
    ...                 ["string:%d" % max_size])
    >>> data
    masked_array(data = ['nbuvggamk' 'bkhwkwl' 'tvb' ..., 'rsdefd' 'lpasx' 'wpdlxierd'],
                 mask = [False False False ..., False False False],
           fill_value = N/A)

Each of these values is a ``numpy.string_`` instance. You can convert it to a
regular Python string if you'd like::

    >>> mystr = str(data[0])

If you have non-ASCII UTF-8 characters in this data, create a Unicode object
instead with the proper encoding::

    >>> sizes, = m.query("test", "utf8", {}, ["stringdata"], ["size"])
    >>> data, = m.query("test", "utf8", {}, ["stringdata"],
    ...                 ["string:%d" % numpy.amax(sizes)])
    >>> mystr = unicode(data[0], "utf-8")
    >>> mystr
    u'libert\xe9'
    >>> print mystr
    liberté
