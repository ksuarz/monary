Aggregation Pipeline
====================

This example demonstrates how to use MongoDB's `aggregation pipeline
<http://docs.mongodb.org/manual/core/aggregation-introduction/>`_ with
Monary. It assumes that you know the basics of aggregation pipelines. For a
complete list of the possible pipeline operators, refer to the `aggregation
framework operators
<http://docs.mongodb.org/manual/reference/operator/aggregation/>`_.

Note that you must have MongoDB 2.2 or later to use the aggregation pipeline.

Setup
-----
This example assumes that **mongod** is running on the default host and port.
You can use any test data you want to aggregate on; this example will use the
MongoDB `zipcode data set <http://media.mongodb.org/zips.json>`_. Simply use
**mongoimport** to import the collection into MongoDB.

.. code-block:: bash

    $ mongoimport zips.json

By default, it will be loaded to the database ``test`` under the collection name
``zips``:

.. code-block:: javascript

    > use test
    switched to db test
    > db.zips.find()


Performing Aggregations
-----------------------
In Monary, a pipeline must be a list of Python dicts. Each dict must contain
exactly one aggregation pipeline operator, each representing one stage of the
pipeline.

For convenience, you may also pass a dict containing a single aggregation
operation if your pipeline contains only one stage.

$match
......
The `match operator
<http://docs.mongodb.org/manual/reference/operator/aggregation/match/>`_ is an
equality test: it selects all documents where the specified key has the
specified value.

For example, we can create a pipeline that matches all zip codes in New York
state::

    >>> pipeline = {"$match" : {"buyer" : "Alice"}}
    >>> result, = monary.aggregate("test", "data", pipeline, ["product"], ["string:7])
    >>> result
    masked_array(data = ['orange' 'orange' 'banana' ..., 'apple' 'banana' 'orange'],
                 mask = [False False False ..., False False False],
                        fill_value = N/A)]])
