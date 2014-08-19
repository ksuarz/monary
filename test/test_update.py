# Monary - Copyright 2011-2014 David J. C. Beach
# Please see the included LICENSE.TXT and NOTICE.TXT for licensing information.

import random

import numpy as np
from numpy import ma
import pymongo

from monary import Monary, MonaryParam

NUM_TEST_RECORDS = 1000

# These values are being cached to speed up the tests.
int8_arr = None
float64_arr = None

# This is used for sorting results during queries to ensure the returned values
# are in the same order as the values inserted.
seq = MonaryParam(
    ma.masked_array(np.arange(NUM_TEST_RECORDS, dtype="int64"),
                    np.zeros(NUM_TEST_RECORDS)), "seq")

def NTR():
    return range(NUM_TEST_RECORDS)


def rand_bools():
    return [bool(random.getrandbits(1)) for _ in NTR()]


def make_ma(data, dtype):
    return ma.masked_array(data, rand_bools(), dtype=dtype)


def setup():
    global int8_arr, float64_arr
    with pymongo.MongoClient() as c:
        c.drop_database("monary_test")
    random.seed(1234)  # For reproducibility.

    int8_arr = make_ma([random.randint(0 - 2 ** 4, 2 ** 4 - 1)
                        for _ in NTR()], "int8")
    float64_arr = make_ma([random.uniform(-1e30, 1e30)
                           for _ in NTR()], "float64")


def test_update_all():
    with Monary() as m:
        m.insert("monary_test", "data", [seq])
        # The only value is masked, so it will match anything.
        masked = ma.masked_array([0], [True], dtype="int16")
        seven = ma.masked_array([7], [False], dtype="int16")
        updated = m.update("monary_test", "data",
                           [MonaryParam(masked, "_")],
                           [MonaryParam(seven, "$set.seven")],
                           multi=True)
        assert updated == NUM_TEST_RECORDS
        assert m.count("monary_test", "data", {"seven": 7}) == updated
    teardown()


def test_update_normalize():
    with Monary() as m:
        ids = m.insert("monary_test", "data",
                       [MonaryParam(float64_arr, "value"), seq])
        # Find the id's and values of all unmasked int8 values.
        ids = ids[~float64_arr.mask]
        floats = float64_arr.data[~float64_arr.mask]
        # Normalize the values.
        avg = floats.mean()
        std = floats.std()
        floats -= avg
        floats /= std
        # Make masked arrays for updating.
        floats = ma.masked_array(floats, np.zeros(len(floats)))
        ids = ma.masked_array(ids, np.zeros(len(ids)))
        updated = m.update("monary_test", "data",
                           [MonaryParam(ids, "_id", "id")],
                           [MonaryParam(floats, "$set.value")])
        assert updated == len(ids)
        data, = m.query("monary_test", "data", {"value": {"$exists": True}},
                        ["value"], ["float64"], sort="seq")
        assert data.count() == len(data) == floats.count() == len(floats)
        assert (data == floats).all()
    teardown()


def test_upserts():
    with Monary() as m:
        s = ma.masked_array(seq.array.data, int8_arr.mask)
        m.insert("monary_test", "data", [MonaryParam(s, "seq")])
        (updated, upserted, ids) = m.update(
            "monary_test", "data", [seq],
            [MonaryParam(int8_arr, "$set.ints")], upsert=True)
        assert updated == s.count()
        assert updated + upserted == NUM_TEST_RECORDS
        assert upserted == ids.count()
        assert (ids.mask == ~s.mask).all()
    teardown()


def teardown():
    with pymongo.MongoClient() as c:
        c.drop_database("monary_test")
