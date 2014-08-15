Tutorial
========

Prerequisites
-------------
To start, you need to have **Monary** :doc:`installed <installation>`. In a
Python shell, the following should run without raising an exception::

    >>> import monary

You'll also need a MongoDB instance running on the default host and port
(``localhost:27017``). If you have `downloaded and installed
<http://www.mongodb/org/display/DOCS/Getting+Started>`_ MongoDB, you can start
the mongo daemon in your system shell:

.. code-block:: bash

    $ mongod

Making a Connection with Monary
-------------------------------
To use **Monary**, we need to create a connection to a running **mongod**
instance. We can make a new Monary object::

    >>> from monary import Monary
    >>> client = Monary()

This connects to the default host and port; it can also be specified
explicitly::

    >>> client = Monary("localhost", 27017)

Monary can also accept MongoDB URI strings::

    >>> client = Monary("mongodb://me@password:test.example.net:2500/database?replicaSet=test&connectTimeoutMS=300000")

.. seealso::

    The `MongoDB connection string format
    <http://docs.mongodb.org/manual/reference/connection-string/>`_ for more
    information about how these URI's are formatted.

Performing Finds
----------------
Specifying the name of a MongoDB document field and its type tells Monary to
query for the desired data and load it into a NumPy array.

To start, we first need a data set for use. We can insert some sample data using
the mongo shell:

.. code-block:: bash

    $ mongo

Then, at the shell prompt, insert some sample documents:

.. code-block:: javascript

    > use test
    switched to db test
    > for (var i = 0; i < 5000; i++) {
    ... db.collection.insert({ a : Math.random(), b : NumberInt(i) })
    ... }

Each document looks something like this:

.. code-block:: javascript

    {
        a : 0.34534613435643535,
        b : 1
    }

To retrieve all of the ``b`` values with Monary, we use the ``query()`` function
and specify the field name we want and its type.::

    >>> with Monary() as m:
    ...     arrays = m.query("test", "collection", {}, ["b"], ["int32"])

``arrays`` is now a list containing a NumPy `masked array
<http://docs.scipy.org/doc/numpy/reference/maskedarray.generic.html>`_ with 5000
values::

    >>> arrays
    [masked_array(data = [0 1 2 ..., 4997 4998 4999],
                 mask = [False False False ..., False False False],
           fill_value = 999999)
    ]

We can also query for both fields together::

    >>> with Monary("localhost") as m:
    ...     arrays = m.query("test", "collection", {}, ["a", "b"], ["float64", "int32"])
    ...
    >>> arrays
    [masked_array(data = [0.7288538725115359 0.4277338122483343 0.5252409593667835 ...,
     0.36620052182115614 0.2733050910755992 0.16910275584086776],
                 mask = [False False False ..., False False False],
           fill_value = 1e+20)
    , masked_array(data = [0 1 2 ..., 4997 4998 4999],
                 mask = [False False False ..., False False False],
           fill_value = 999999)
    ]
