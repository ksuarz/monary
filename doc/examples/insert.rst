Insert Example
==============

This example shows you how to use Monary's ``insert`` command to send documents
to MongoDB.

Any value that can be queried can also be inserted except for ``type``,
``length``, and ``size``. Values returned from any of those queries, however,
can themselves be inserted. Both nested field insertion (via fields containing
".") and BSON value insertion are supported as well.

Purpose of Insert
-----------------
Though MongoDB has aggregation functionality, Numpy can perform more operations
than aggregation supports. Because of this, it can be useful to insert data
into MongoDB with Monary after processing it with Numpy.

Monary inserts can also be used to store intermediate data in the middle of
intense computations. This can be useful when doing operations on blocks of
data with :doc:`block query </examples/block-query>`.

Setup
-----
For this example, let's use PyMongo to put some unprocessed documents
representing students' test scores into MongoDB. First, we can set up a
connection to the local MongoDB database::

    >>> from pymongo import MongoClient
    >>> client = MongoClient()

Next, we generate the documents::

    >>> import random
    >>> al_num = '0123456789abcdefghijklmnopqrstuvwxyz'
    >>> client.drop_database("monary_students")
    >>> for _ in range(1000):
    ...     student_id = "".join(al_num[random.randint(0, len(al_num)-1)]
    ...                          for _ in range(14))
    ...     student = {"student_id": student_id,
    ...                "test_scores":
    ...                    {"midterm": random.randint(0, 1000) / 10,
    ...                     "final_exam": random.randint(0, 1000) / 10},
    ...                "name": "..."}
    ...     client.monary_students.raw.insert(student)


Using Monary Insert
-------------------
Let's first get all the raw test data into Numpy arrays with Monary::

    >>> import numpy as np
    >>> from monary import Monary
    >>> m = Monary()
    >>> ids, midterm, final = \
    ...     m.query("monary_students", "raw", {},
    ...             ["student_id",
    ...              "test_scores.midterm",
    ...              "test_scores.midterm"],
    ...             ["string:14", "float64",
    ...              "float64"])

Now we process the scores and assign grades to each student::

    >>> grades = [None, None]
    >>> for i, arr in enumerate([midterm, final]):
    ...     # curve to average of 2.3333
    ...     mean, stdev = arr.mean(), arr.std()
    ...     grades[i] = (arr - mean) / stdev
    ...     grades[i] += 2.3333
    ...     # bound grades within [0.0, 4.0]
    ...     fours = np.argwhere(grades[i] > 4.0)
    ...     zeros = np.argwhere(grades[i] < 0.0)
    ...     grades[i][fours] = 4.0
    ...     grades[i][zeros] = 0.0

Now weight both tests and assign overall grades:

    >>> overall_grades = (grades[0] * 0.4 + grades[1] * 0.6).round(3)

Finally, we insert the results to the database::

    >>> ids = m.insert("monary_students", "graded",
    ...                [ids, overall_grades] + grades,
    ...                ["student_id", "grades.overall", "grades.midterm",
    ...                 "grades.final_exam"],
    ...                ["string:14"] + ["float64"] * 3)
    >>> from monary.monary import mvoid_to_bson_id
    >>> oids = list(map(mvoid_to_bson_id, ids))
    >>> oids[0]
    ObjectId('53dba51e61155374af671dc1')

We can see that insert returns a Numpy array containing the ObjectId of the
inserted documents.
