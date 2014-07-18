Block Queries
=============

This quick example shows you how to use Monary's ``block_query`` command, and
discusses what it does.

What is Block Query
-------------------
``block_query`` functions very similaryly to ``query``. The main difference is
that ``block_query`` returns a generator. Furthermore, all but the last numpy
masked arrays that block_query returns will be overwritten as the user iterates
through the results. ``block_query`` is desirable because it allows the user
to specify the number of documents to be read at a time.

Setup
-----
For this example, let's use PyMongo to insert documents with numberical data
into MongoDB. First, we can set up a connection to the local MongoDB database::

    >>> import bson
    >>> from pymongo import MongoClient
    >>> client = MongoClient()

Next, we generate some documents. In this example, these documents will
represent financial assets::

    >>> import random
    >>> documents = []
    >>> for i in xrange(5000):
    ...     doc = {"sold": True}
    ...     doc["buy_price"] = random.uniform(50, 300)
    ...     doc["sell_price"] = doc["buy_price"] + random.uniform(-10, 30)
    ...     documents.append(doc)

Finally, we use PyMongo to insert the data into MongoDB::

    >>> client.finance.assets.insert(documents)


Using Block Query
-----------------
To perform a block query, first create a monary connection::

    >>> import monary
    >>> m = monary.Monary()

Now we query the database, specifying also how many results we want per block::

    >>> cumulative_gain = 0.0
    >>> assets_count = 0
    >>> for buy_price_block, sell_price_block in (
    ...     m.block_query("finance", "assets", {"sold": True},
    ...                        ["buy_price", "sell_price"],
    ...                        ["float64", "float64"],
    ...                        block_size=1024)):
    ...     assets_count += sell_price_block.count()
    ...     gain = sell_price_block - buy_price_block   # vector subtraction
    ...     cumulative_gain += numpy.sum(gain)

Finally, we can review our financial data::

    >>> cumulative_gain
    49272.963745278699
    >>> assets_count
    5000
