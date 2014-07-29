# Monary - Copyright 2011-2014 David J. C. Beach
# Please see the included LICENSE.TXT and NOTICE.TXT for licensing information.

import numpy as np
import numpy.ma as ma
import numpy.random as nprand

import monary
from profile import profile

try:
    xrange
except NameError:
    xrange = range

NUM_BATCHES = 4500
BATCH_SIZE = 1000
# 4500 batches * 1000 per batch = 4.5 million records


def do_insert():
    m = monary.Monary()
    num_docs = NUM_BATCHES * BATCH_SIZE
    arrays = [ma.masked_array(nprand.uniform(0, i + 1, num_docs),
                              np.zeros(num_docs)) for i in xrange(5)]
    with profile("monary insert"):
        m.insert("monary_test", "collection", arrays,
                 ["x1", "x2", "x3", "x4", "x5"],
                 ["float64"] * 5)

if __name__ == "__main__":
    do_insert()
    print("Inserted %d records." % (NUM_BATCHES * BATCH_SIZE))
