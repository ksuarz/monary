Query Example
=============

This example shows you how to use Monary's ``query`` command.

Setup
-----
For this example, let's use PyMongo to insert documents with numerical data
into MongoDB. First, we can set up a connection to the local MongoDB database::

    >>> from pymongo import MongoClient
    >>> client = MongoClient()

Next, we generate some documents. In this example, these documents will
represent financial assets::

    >>> import random
    >>> documents = []
    >>> for i in range(5000):
    ...     doc = {"sold": True}
    ...     doc["buy_price"] = random.uniform(50, 300)
    ...     doc["sell_price"] = doc["buy_price"] + random.uniform(-10, 30)
    ...     documents.append(doc)

Finally, we use PyMongo to insert the data into MongoDB::

    >>> client.finance.assets.insert(documents)


Using Query
-----------
To perform a query, first create a Monary connection::

    >>> from monary import Monary
    >>> m = Monary()

Now we query the database, specifying the keys we want from the MongoDB
documents and what type we want the returned data to be::

    >>> buy_price, sell_price = m.query("finance", "assets", {"sold": True},
    ...                                 ["buy_price", "sell_price"],
    ...                                 ["float64", "float64"])
    >>> assets_count = sell_price.count()
    >>> gain = sell_price - buy_price   # vector subtraction
    >>> cumulative_gain = gain.sum()

Finally, we can review our financial data::

    >>> cumulative_gain
    49272.963745278699
    >>> assets_count
    5000
