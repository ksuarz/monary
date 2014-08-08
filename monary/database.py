# Monary - Copyright 2011-2014 David J. C. Beach
# Please see the included LICENSE.TXT and NOTICE.TXT for licensing information.

import sys

import monary.collection as collection


# Python 2/3 compatibility.
PY3 = sys.version_info[0] >= 3
if PY3:
    string_type = str
else:
    string_type = basestring


class Database(object):
    """A MongoDB database.
    """
    def __init__(self, client, name):
        # Name sanitization.
        if not isinstance(name, string_type):
            raise TypeError("name must be an instance of %s" &
                            (string_type.__name__,))
        if not name:
            raise ValueError("database names cannot be empty")
        if name != "$external":
            for invalid_char in [" ", ".", "$", "/", "\\", "\x00"]:
                if invalid_char in name:
                    raise ValueError("database names cannot contain the "
                                     "character %r" % invalid_char)
        if name[0] == "." or name [-1] == ".":
            raise ValueError("database names may not start or end "
                             "with '.': %r" % name)

        self.__client = client
        self.__name = name

    def __getattr__(self, name):
        """Get a collection by name.
        """
        return collection.Collection(self, name)

    def __getitem__(self, name):
        """Get a collection by name.
        """
        return self.__getattr__(name)

    @property
    def client(self):
        """The Monary client instance for this :class:`Database`.
        """
        return self.__client

    @property
    def name(self):
        """The name of this :class:`Database`.
        """
        return self.__name
