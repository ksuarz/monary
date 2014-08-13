# Monary - Copyright 2011-2014 David J. C. Beach
# Please see the included LICENSE.TXT and NOTICE.TXT for licensing information.

import sys


# Python 2/3 compatibility.
PY3 = sys.version_info[0] >= 3
if PY3:
    string_type = str
else:
    string_type = basestring


class Collection(object):
    """A MongoDB collection.
    """
    def __init__(self, database, name):
        # Name sanitization.
        if not isinstance(name, string_type):
            raise TypeError("name must be an instance of %s" %
                            (string_type.__name__,))
        if not name:
            raise ValueError("collection names cannot be empty")
        if not (name.startswith("oplog.$main") or name.startswith("$cmd")):
            for invalid_char in [" ", ".", "$", "/", "\\", "\x00"]:
                if invalid_char in name:
                    raise ValueError("collection names cannot contain the "
                                     "character %r" % invalid_char)
        if name[0] == "." or name [-1] == ".":
            raise ValueError("collection names may not start or end "
                             "with '.': %r" % name)

        self.__database = database
        self.__name = name
        self.__full_name = "%s.%s" % (self.__database.name, self.__name)

    def __getattr__(self, name):
        """Get a sub-collection of this collection by name.
        """
        return Collection(self.__database, u"%s.%s" % (self.__name, name))

    def __getitem__(self, name):
        """Get a sub-collection of this collection by name.
        """
        return self.__getattr__(name)

    @property
    def database(self):
        """The Monary database instance for this :class:`Collection`.
        """
        return self.__database

    @property
    def name(self):
        """The name of this :class:`Collection`.
        """
        return self.__name

    @property
    def full_name(self):
        """The full name of this :class:`Collection`.
        """
        return self.__full_name

    # TODO: Docstrings to come later. (Copy originals)
    def count(self, query):
        return self.database.client.count(self.database.name, self.name, query)

    def find(self, query, fields, types, **kwargs):
        return self.database.client.query(self.database.name, self.name, query,
                                          fields, types, **kwargs)

    def block_find(self, query, fields, types, **kwargs):
        return self.database.client.block_query(self.database.name, self.name,
                                                query, fields, types, **kwargs)

    def aggregate(self, pipeline, fields, types, **kwargs):
        return self.database.client.aggregate(self.database.name, self.name,
                                              pipeline, fields, types,
                                              **kwargs)

    def block_aggregate(self, pipeline, fields, types, **kwargs):
        return self.database.client.block_aggregate(self.database.name,
                                                    self.name, pipeline,
                                                    fields, types, **kwargs)

    def insert(self, data, fields, types=None):
        return self.data.client.insert(self.database.name, self.name,
                                       data, fields, types)

    def remove(self, data, fields, types=None, just_one=False):
        return self.data.client.remove(self.database.name, self.name,
                                       data, fields, types, just_one)