Timestamp Example
=================

This quick example demonstrates how to extract timestamps with Monary.

Setup
-----
We can use PyMongo to populate a collection with some test data containing
random timestamps. First, make a connection::

    >>> from pymongo import MongoClient
    >>> client = MongoClient()

Then, we can insert random timestamps::

    >>> import random
    >>> import bson
    >>> for _ in range(100):
    ...     time = random.randint(0, 1000000)
    ...     inc = random.randint(0, 1000000)
    ...     ts = bson.timestamp.Timestamp(time=time, inc=inc)
    ...     client.test.data.insert({"ts": ts})

Finding Timestamp Data
----------------------

Next we use :doc:`query </examples/query>` to get back our data::

    >>> from monary import Monary
    >>> m = Monary()
    >>> time_stamps, = m.query("test", "data", {},
    ...                        ["ts"], ["timestamp"])

Finally, we use `struct <https://docs.python.org/2/library/struct.html>`_ to
unpack the resulting data::

    >>> import struct
    >>> data = [struct.unpack("<ii", ts) for ts in time_stamps]
    >>> time_stamps = [bson.timestamp.Timestamp(time=time, inc=inc)
    ...                for time, inc in data]
    >>> time_stamps[0]
    Timestamp(237645, 622130)
