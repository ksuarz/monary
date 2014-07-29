# Monary - Copyright 2011-2014 David J. C. Beach
# Please see the included LICENSE.TXT and NOTICE.TXT for licensing information.

import datetime
import random
import string
import sys
import time

import bson
import numpy as np
import numpy.ma as ma
import pymongo

import monary
from monary.monary import mvoid_to_bson_id, validate_insert_fields

NUM_TEST_RECORDS = 14000
ARRAYS = None


def NTR():
    return range(NUM_TEST_RECORDS)


def rand_bools():
    return [bool(random.getrandbits(1)) for _ in NTR()]


def make_ma(data, dtype):
    return ma.masked_array(data, rand_bools(), dtype=dtype)


def random_timestamp():
    ts = bson.timestamp.Timestamp(
        time=random.randint(0, 2147483647),
        inc=random.randint(0, 2147483647))
    # return int.from_bytes(struct.pack("<II", ts.time, ts.inc), "little")
    return (ts.time << 32) | (ts.inc)


def random_date():
    t = datetime.datetime(1970, 1, 1) + \
        (1 - 2 * random.randint(0, 1)) * \
        datetime.timedelta(days=random.randint(0, 60 * 365),
                           seconds=random.randint(0, 24 * 3600),
                           milliseconds=random.randint(0, 1000))
    return time.mktime(t.timetuple())


def random_string(str_len):
    return "".join(random.choice(string.ascii_letters)
                   for _ in range(str_len))


def setup():
    global ARRAYS
    with pymongo.MongoClient() as c:
        c.drop_database("monary_test")  # ensure that database does not exist
    random.seed(1234)  # for reproducibility
    ARRAYS = []
    # bool
    bool_arr = make_ma(rand_bools(), "bool")
    ARRAYS.append(bool_arr)
    # int8
    int8_arr = make_ma([random.randint(0 - 2 ** 4, 2 ** 4 - 1)
                        for _ in NTR()], "int8")
    ARRAYS.append(int8_arr)
    # int16
    int16_arr = make_ma([random.randint(0 - 2 ** 8, 2 ** 8 - 1)
                         for _ in NTR()], "int16")
    ARRAYS.append(int16_arr)
    # int32
    int32_arr = make_ma([random.randint(0 - 2 ** 16, 2 ** 16 - 1)
                         for _ in NTR()], "int32")
    ARRAYS.append(int32_arr)
    # int64
    int64_arr = make_ma([random.randint(0 - 2 ** 32, 2 ** 32 - 1)
                         for _ in NTR()], "int64")
    ARRAYS.append(int64_arr)
    # uint8
    uint8_arr = make_ma([random.randint(0, 2 ** 8 - 1)
                         for _ in NTR()], "uint8")
    ARRAYS.append(uint8_arr)
    # uint16
    uint16_arr = make_ma([random.randint(0, 2 ** 16 - 1)
                          for _ in NTR()], "uint16")
    ARRAYS.append(uint16_arr)
    # uint32
    uint32_arr = make_ma([random.randint(0, 2 ** 32 - 1)
                          for _ in NTR()], "uint32")
    ARRAYS.append(uint32_arr)
    # uint64
    uint64_arr = make_ma([random.randint(0, 2 ** 64 - 1)
                          for _ in NTR()], "uint64")
    ARRAYS.append(uint64_arr)
    # float32
    float32_arr = make_ma([random.uniform(-1e30, 1e30)
                           for _ in NTR()], "float32")
    ARRAYS.append(float32_arr)
    # float64
    float64_arr = make_ma([random.uniform(-1e30, 1e30)
                           for _ in NTR()], "float64")
    ARRAYS.append(float64_arr)
    # timestamp
    timestamp_arr = make_ma([random_timestamp() for _ in NTR()], "uint64")
    ARRAYS.append(timestamp_arr)
    # date
    date_arr = make_ma([random_date() for _ in NTR()], "int64")
    ARRAYS.append(date_arr)
    # string
    string_arr = make_ma([random_string(10) for _ in NTR()], "S10")
    ARRAYS.append(string_arr)
    # sequence for sorting
    seq = ma.masked_array(list(NTR()), np.zeros(NUM_TEST_RECORDS),
                          dtype="int64")
    ARRAYS.append(seq)


def test_insert_no_types():
    with monary.Monary() as m:
        # Dates and timestamps can't be defaulted properly
        inserted = m.insert("monary_test", "data", ARRAYS[:-4],
                            ["bool", "int8", "int16", "int32", "int64",
                             "uint8", "uint16", "uint32", "uint64", "float32",
                             "float64"])
        assert inserted == NUM_TEST_RECORDS
        teardown()


def test_insert():
    with monary.Monary() as m:
        inserted = m.insert("monary_test", "data", ARRAYS[:-1],
                            ["bool", "int8", "int16", "int32", "int64",
                             "uint8", "uint16", "uint32", "uint64", "float32",
                             "float64", "timestamp", "date", "string"],
                            ["bool", "int8", "int16", "int32", "int64",
                             "uint8", "uint16", "uint32", "uint64", "float32",
                             "float64", "timestamp", "date", "string:10"])
        assert inserted == NUM_TEST_RECORDS
        teardown()


def test_retrieve_no_types():
    with monary.Monary() as m:
        m.insert("monary_test", "data", ARRAYS[:-4] + [ARRAYS[-1]],
                 ["x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
                  "x9", "x10", "x11", "sequence"])
    with monary.Monary() as m:
        retrieved = m.query("monary_test", "data", {},
                            ["x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
                             "x9", "x10", "x11"],
                            ["bool", "int8", "int16", "int32", "int64",
                             "uint8", "uint16", "uint32", "uint64", "float32",
                             "float64"], sort="sequence")
        for data, expected in zip(retrieved, ARRAYS):
            assert data.count() == expected.count()
            assert (data == expected).all()
    teardown()


def test_retrieve():
    with monary.Monary() as m:
        m.insert("monary_test", "data", ARRAYS,
                 ["x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
                  "x9", "x10", "x11", "x12", "x13", "x14", "sequence"],
                 ["bool", "int8", "int16", "int32", "int64",
                  "uint8", "uint16", "uint32", "uint64", "float32",
                  "float64", "timestamp", "date", "string:10", "int64"])
    with monary.Monary() as m:
        retrieved = m.query("monary_test", "data", {},
                            ["x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
                             "x9", "x10", "x11", "x12", "x13", "x14",
                             "sequence"],
                            ["bool", "int8", "int16", "int32", "int64",
                             "uint8", "uint16", "uint32", "uint64", "float32",
                             "float64", "timestamp", "date", "string:10",
                             "int64"], sort="sequence")
        for data, expected in zip(retrieved, ARRAYS):
            assert data.count() == expected.count()
            assert (data == expected).all()
    teardown()


def test_oid():
    # Insert documents to generate oid's
    with monary.Monary() as m:
        m.insert("monary_test", "data", [ARRAYS[0], ARRAYS[-1]],
                 ["dummy", "sequence"])
    # Get back the id's and test inserting them
    with monary.Monary() as m:
        retrieved = m.query("monary_test", "data", {},
                            ["_id", "sequence"],
                            ["id", "int64"], sort="sequence")
        retrieved[1] += NUM_TEST_RECORDS
        inserted = m.insert("monary_test", "data", retrieved,
                            ["oid", "sequence"], ["id", "int64"])
        assert inserted == NUM_TEST_RECORDS
    with monary.Monary() as m:
        retrieved = m.query("monary_test", "data", {},
                            ["_id", "oid"], ["id", "id"], sort="sequence")
        data = retrieved[0][:NUM_TEST_RECORDS]
        expected = retrieved[1][NUM_TEST_RECORDS:]
        for d, e in zip(data, expected):
            assert mvoid_to_bson_id(d) == mvoid_to_bson_id(e)
    teardown()


def test_insert_field_validation():
    good = [
        ['a', 'b', 'c'],
        ['a', 'b.a', 'b.b', 'c'],
        ['a.a', 'a.b', 'a.c', 'b', 'c.a', 'c.b', 'c.c'],
        ['a.a.a', 'a.b', 'a.a.b', 'a.c.a', 'b', 'a.c.b'],
        ['a.b.c.d.e.f', 'g.h.i.j.k', 'l.m.n.o.p.q.r.s.t.u', 'b.c.d'],
    ]
    bad = [
        ['a', 'b', 'a'],  # 'a' occurs twice
        ['a', 'b.c', 'b.c.d'],  # 'b.c' is duplicated
        ['a.a', 'a.b', 'b', 'c.a', 'c.b', 'a.b.c'],  # 'a.b' is duplicated
        ['a.a.a', 'a.a.b', 'a.b', 'a.c.a', 'a.c.b', 'b', 'c.a', 'c.',
         'd.a.a', 'd.a.b', 'd.b', 'd.c.a', 'd.c.b'],  # 'c.' ends in '.'
        ['a.b.c.d.e.f', 'g.h.i.j.k', 'l.m.n.o.p.q.r.s.t.u', 'v.w.x',
         'y.z', 'a']  # 'a' is duplicated
    ]
    for g in good:
        try:
            validate_insert_fields(g)
        except ValueError:
            assert False, "%r should have been valid" % g
    for b in bad:
        try:
            validate_insert_fields(b)
            assert False, "%r should not have been valid" % b
        except ValueError:
            pass


def test_nested_insert():
    seq = ARRAYS[-1]
    squares = np.arange(NUM_TEST_RECORDS) ** 2
    squares = ma.masked_array(squares, np.zeros(NUM_TEST_RECORDS),
                              dtype="float64")
    random = np.random.uniform(0, 5, NUM_TEST_RECORDS)
    random = ma.masked_array(random, np.zeros(NUM_TEST_RECORDS),
                             dtype="float64")
    unmasked = ma.masked_array(rand_bools(), np.zeros(NUM_TEST_RECORDS),
                               dtype="bool")
    masked = ma.masked_array(rand_bools(), np.ones(NUM_TEST_RECORDS),
                             dtype="bool")
    with monary.Monary() as m:
        m.insert("monary_test", "data", [squares, random, seq, unmasked,
                 masked], ["data.sqr", "data.rand", "sequence", "x.y.real",
                 "x.y.fake"])
    with pymongo.MongoClient() as c:
        col = c.monary_test.data
        for i, doc in enumerate(col.find().sort(
                [("sequence", pymongo.ASCENDING)])):
            assert doc["sequence"] == i
            assert random[i] == doc["data"]["rand"]
            assert squares[i] == doc["data"]["sqr"]
            assert "fake" not in doc["x"]["y"]
            assert unmasked[i] == doc["x"]["y"]["real"]
    teardown()


def test_retrieve_nested():
    arrays = ARRAYS[:5] + ARRAYS[9:11] + ARRAYS[-2:]
    with monary.Monary() as m:
        m.insert("monary_test", "data", arrays,
                 ["a.b.c.d.e.f.g.h.x1", "a.b.c.d.e.f.g.h.x2",
                  "a.b.c.d.e.f.g.h.x3", "a.b.c.d.e.f.g.h.x4",
                  "a.b.c.d.e.f.g.h.x5", "a.b.c.d.e.f.g.h.x6",
                  "a.b.c.d.e.f.g.h.x7", "a.b.c.d.e.f.g.h.x8",
                  "sequence"],
                 ["bool", "int8", "int16", "int32", "int64",
                  "float32", "float64", "string:10", "int64"])
    with pymongo.MongoClient() as c:
        col = c.monary_test.data
        for i, doc in enumerate(col.find().sort(
                [("sequence", pymongo.ASCENDING)])):
            assert doc["sequence"] == i
            for j in range(8):
                if not arrays[j].mask[i]:
                    data = arrays[j][i]
                    exp = doc["a"]["b"]["c"]["d"]["e"]["f"]["g"]["h"]
                    exp = exp["x" + str(j + 1)]
                    if sys.version_info[0] >= 3 and isinstance(data, bytes):
                        data = data.decode("ascii")
    teardown()


def teardown():
    with pymongo.MongoClient() as c:
        c.drop_database("monary_test")  # ensure that database does not exist
