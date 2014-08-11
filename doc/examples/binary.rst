Binary Data Example
===================

This example shows you how to obtain blocks of binary data from MongoDB with
Monary.

Setup
-----
For this example, let's use PyMongo to insert raw binary image data into
MongoDB. First, we can set up a connection to the local MongoDB database::

    >>> import bson
    >>> from pymongo import MongoClient
    >>> client = MongoClient()

Next, we open some random image files. Assume we have image files named
``img0.jpg`` through ``img99.jpg``::

    >>> images = []
    >>> for i in range(0, 100):
    ...     with open("img%d.jpg" % i, "rb") as img:
    ...         f = img.read()
    ...         images.append(f)

Finally, we use PyMongo's ``Binary`` class and insert the data into MongoDB::

    >>> for img in images:
    ...     data = bson.binary.Binary(img)
    ...     doc = {"bindata" : data}
    ...     client.test.data.insert(doc)

If you don't happen to have random files laying around, you can issue this
command at a Unix shell:

.. code-block:: bash

    $ for ((i = 0; i < 100; i=i+1)); do
    > head -c $SIZE < /dev/urandom > "file${i}.dat"
    > done

This creates 100 files, each containing ``$SIZE`` bytes of random data.

Finding Binary Data
-------------------
To query binary data, Monary requires the size of the binary to load in. Since
the data in different documents can be of different sizes, we need to use the
size of the biggest binary blob to avoid a loss of data.

To find the size of the data in bytes, we use the ``size`` type::

    >>> from monary import Monary
    >>> m = Monary()
    >>> sizes, = m.query("test", "data", {}, ["bindata"], ["size"])
    >>> sizes
    masked_array(data = [128L 64L 100L ..., 255L 255L 255L],
                 mask = [False False False ..., False False False],
           fill_value = 999999)

Note that these sizes are unsigned 32-bit integers::

    >>> sizes[0]
    128
    >>> type(sizes[0])
    <type 'numpy.uint32'>

We can get the maximum image size with ``numpy.amax``::

    >>> import numpy
    >>> max_size = int(numpy.amax(sizes))

Finally, we can issue a query command to get pointers to the binary data::

    >>> data, = m.query("test", "data", {},
    ...                 ["bindata"], ["binary:%d" % max_size])
    >>> data
    masked_array(data = [<read-write buffer ptr 0x7f8a58421b50, size 255 at 0x105b6deb0> ...],
                 mask = [False ...]
           fill_value = ???)

Each buffer pointer is of type ``numpy.ma.core.mvoid``. From here, you can use
NumPy to manipulate the data or export it to another location.
