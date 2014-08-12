Remove Example
==============

This example shows you how to use Monary's ``remove`` command to remove
documents from MongoDB.

Any value that can be inserted can also be used to form a "selector" for
remove. Selectors are simply BSON documents. Depending on the value of remove's
``just_one`` command, a single matching document or all matching documents will
be removed for each selector created during monary's ``remove``.

.. seealso::

    :doc:`Monary's Insert Example </examples/insert>` and
    `the MongoDB remove reference
    <http://docs.mongodb.org/manual/reference/method/db.collection.remove/>`_

Purpose of Remove
-----------------
Monary allows bulk finds and bulk inserts, so it makes sense to extend support
to bulk removes. Monary's "bulk" removes are different than regular removes in
that they perform many regular removes at once. This provides the user a fast
way to remove large amounts of data with one command, while still having a very
fine grained control over what is removed.

Setup
-----
For this example, we will use Monary's insert to put ten thousand random values
representing scores out of 100 into our data base. First, we set up a Monary
connection to the local MongoDB database::

    >>> from monary import Monary
    >>> client = Monary()

Next, we generate and insert the documents::

    >>> import numpy as np
    >>> scores = np.random.uniform(0, 100, 10000)  # 10,000 numbers from 0 to 100
    >>> mask = np.zeros(len(scores), dtype="bool")
    >>> scores = np.ma.masked_array(scores, mask)
    >>> ids = client.insert("scores", "data", [scores], ["score"])


Using Monary Remove
-------------------
Suppose we have done our processing and now want to remove the data we inserted
above from the database. We must first make a masked array from the ids::

    >>> mask = np.zeros(len(ids), dtype="bool")
    >>> ids = np.ma.masked_array(ids, mask)

Now we can perform the removal::

    >>> num_removed = client.remove("scores", "data", [ids], ["_id"], ["id"])
    >>> num_removed
    10000

Suppose instead of the last two commands above, we had wanted to remove all of
the data we just inserted where the score was less than 80. Suppose also that
we don't want to accidentally remove data that had been in our database before
we started with this example. Then, we can do this with Monary like so::

    >>> arr = np.zeros(len(ids), dtype="float64")
    >>> arr += 80.0
    >>> mask = np.zeros(len(arr), dtype="bool")
    >>> arr = np.ma.masked_array(arr, mask)
    >>> num_removed = client.remove("scores", "data",
    ...                             [ids, arr],
    ...                             ["_id", "score.$lt"],
    ...                             ["id", "float64"])
    >>> num_removed
    7974
