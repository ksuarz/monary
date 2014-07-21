Monary |release| Documentation
==============================

Overview
--------
**Monary** provides a Python interface for fast column queries from `MongoDB
<http://www.mongodb.org>`_. It is recommended over PyMongo when requiring large
bulk reads from a database into `NumPy arrays <http://www.numpy.org/>`_.

Note that Monary is still in beta. There are no guarantees of API stability;
furthermore, dependencies may change in the future.

Monary is written by `David J. C. Beach <http://djcinnovations.com/>`_.

:doc:`installation`
  Instructions on how to get the distribution.

:doc:`tutorial`
  Getting started quickly with Monary.

:doc:`examples/index`
  Examples of how to perform specific tasks.

:doc:`reference`
  In-depth explanations of how Monary handles BSON types.

:doc:`faq`
  Frequently asked questions about Monary.


Dependencies
------------
Monary depends on `PyMongo <http://api.mongodb.org/python/current/>`_
and `NumPy <http://www.numpy.org/>`_.

Monary uses the `MongoDB C driver <http://github.com/mongodb/mongo-c-driver>`_,
which comes bundled as part of the module.

Issues
------
All issues can be reported by opening up an issue on the Monary `BitBucket
issues page <https://bitbucket.org/djcbeach/monary/issues>`_.

Contributing
------------
Monary is an open-source project and is hosted on `BitBucket
<https://bitbucket.org/djcbeach/monary/wiki/Home>`_. To contribute, fork the
project and send a pull request.

See the :doc:`contributors` page for a list of people who have contributed to
developing Monary.

Changes
-------
See the :doc:`changelog` for a full list of changes to Monary.

About This Documentation
------------------------
This documentation is generated using the `Sphinx
<http://sphinx.pocoo.org/>`_ documentation generator. The source files
for the documentation are located in the *doc/* directory of the
Monary distribution.

Indices and tables
------------------
* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

.. toctree::
   :hidden:
   :maxdepth: 2

   installation
   tutorial
   examples/index
   reference
   faq
   changelog
   contributors
