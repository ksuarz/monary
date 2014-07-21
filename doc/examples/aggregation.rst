Aggregation Pipeline
====================

This example demonstrates how to use MongoDB's `aggregation pipeline
<http://docs.mongodb.org/manual/core/aggregation-introduction/>`_ with
Monary. It assumes that you know the basics of aggregation pipelines. For a
complete list of the possible pipeline operators, refer to the `aggregation
framework operators
<http://docs.mongodb.org/manual/reference/operator/aggregation/>`_.

Setup
-----
This example assumes that the MongoDB daemon is running on the default host and
port. You can use any test data you want to aggregate on; below is a sample
script to populate the database with test data.

.. code-block:: python

    import random

    from pymongo import MongoClient

    # The number of sample documents to use
    TEST_RECORDS = 5000

    client = MongoClient()
    db = client.test

    # Fake some sales data
    products = ["apple", "banana", "orange"]
    buyers = ["alice", "bob"]

    # Insert some test documents
    for i in range(TEST_RECORDS):
        doc = {
            "_id" : i,
            "product" : random.choice(products),
            "buyer" : random.choice(buyers),
            "quantity" : random.randint(1, 4)
        }
        db.data.insert(doc)


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

For example, we can create a pipeline that matches all documents where Alice has
purchased something, then load a list containing all of the products she has
bought::

    >>> pipeline = {"$match" : {"buyer" : "Alice"}}
    >>> result, = monary.aggregate("test", "data", pipeline, ["product"], ["string:7])
    >>> result
    masked_array(data = ['orange' 'orange' 'banana' ..., 'apple' 'banana' 'orange'],
                 mask = [False False False ..., False False False],
                        fill_value = N/A)]])
