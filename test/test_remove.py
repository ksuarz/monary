# Monary - Copyright 2011-2014 David J. C. Beach
# Please see the included LICENSE.TXT and NOTICE.TXT for licensing information.

import random

import numpy as np
import numpy.ma as ma
import pymongo

import monary

NUM_TEST_RECORDS = 14000

# These values are being cached to speed up the tests.
bool_arr = None
int8_arr = None


def NTR():
    return range(NUM_TEST_RECORDS)


def rand_bools():
    return [bool(random.getrandbits(1)) for _ in NTR()]


def make_ma(data, dtype):
    return ma.masked_array(data, rand_bools(), dtype=dtype)


def setup():
    global bool_arr, int8_arr
    with pymongo.MongoClient() as c:
        c.drop_database("monary_test")
    random.seed(1234)  # For reproducibility.

    bool_arr = make_ma(rand_bools(), "bool")
    int8_arr = make_ma([random.randint(0 - 2 ** 4, 2 ** 4 - 1)
                        for _ in NTR()], "int8")


def test_remove_all():
    global bool_arr, int8_arr
    with monary.Monary() as m:
        m.insert("monary_test", "data", [bool_arr, int8_arr], ["b", "i"])
        assert m.count("monary_test", "data") == NUM_TEST_RECORDS
        # Remove just one of anything.
        removed = m.remove("monary_test", "data", [], [], just_one=True)
        assert removed == 1
        assert m.count("monary_test", "data") == NUM_TEST_RECORDS - 1
        # Remove everything.
        removed = m.remove("monary_test", "data", [], [])
        assert removed == NUM_TEST_RECORDS - 1
        assert m.count("monary_test", "data") == 0


def test_remove_specific():
    global bool_arr, int8_arr
    with monary.Monary() as m:
        ids = m.insert("monary_test", "data", [bool_arr, int8_arr], ["b", "i"])
        # Remove the first half of the data.
        length = int(NUM_TEST_RECORDS / 2)
        ids = ma.masked_array(ids[:length], np.zeros(length))
        assert m.count("monary_test", "data") == NUM_TEST_RECORDS
        removed = m.remove("monary_test", "data", [ids], ["_id"], ["id"])
        assert removed == length
        assert m.count("monary_test", "data") == NUM_TEST_RECORDS - length
        # Now remove all values left with unmasked bool values.
        bools = ma.masked_array([True, False], [False, False], dtype="bool")
        removed = m.remove("monary_test", "data", [bools], ["b"])
        assert removed == bool_arr[length:].count()
        numleft = NUM_TEST_RECORDS - length - removed
        assert m.count("monary_test", "data") == numleft


def teardown():
    with pymongo.MongoClient() as c:
        c.drop_database("monary_test")