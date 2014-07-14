Strings
=======

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
    ...     return "".join(random.choice(string.lowercase) for c in range(0, length))
    ...
    >>> for i in range(0, 100):
    ...     chrs = [chrdd]
    ...     s = rand_str(random.choice())
