Installing / Upgrading
======================
.. highlight:: bash

**Monary** is in the `Python Package Index
<http://pypi.python.org/pypi/Monary>`_.

Installing with pip
-------------------

You can use `pip <http://pypi.python.org/pypi/pip>`_ to install monary in
platforms other than Windows::

    $ pip install monary

To get a specific version of monary::

    $ pip install monary==0.2.3

To upgrade using pip::

    $ pip install --upgrade monary

.. note::
    Although Monary provides a Python package in .egg format, pip does not
    support installing from Python eggs. If you would like to install Monary
    with a .egg provided on PyPI, use easy_install instead.

Installing with easy_install
----------------------------

To use ``easy_install`` from `setuptools
<http://pypi.python.org/pypi/setuptools>`_ do:

.. code-block:: bash

    $ easy_install monary

To upgrade:

.. code-block:: bash

    $ easy_install -U monary

Installing on Windows
---------------------
Monary provides Python eggs pre-compiled for Windows distributions. First you
must `install easy_install
<http://simpledeveloper.com/how-to-install-easy_install/>`_. To install Monary,
download `the latest monary egg
<https://testpypi.python.org/packages/2.7/M/Monary/Monary-0.3.0-py2.7.egg>`_.
Then from the command line, ``cd`` to where the egg file was downloaded and
type:

.. code-block:: bash

    $ easy_install Monary-0.3.0-py2.7.egg


Installing on OSX
-----------------
Monary provides Python wheels that can be installed directly on OSX.

Installing on Other Unix Distributions
--------------------------------------
Monary uses the `MongoDB C driver <https://github.com/mongodb/mongo-c-driver>`_.
If you install Monary on Linux, BSD and Solaris, you'll need to be able to
compile the C driver with the GNU C compiler. Depending on your system, you may
also need a Python development package that provides the necessary header files
for building C extensions for Python. If you don't have the appropriate
packages, you can install them from a repository with your system package
manager.

On Debian and Ubuntu:

.. code-block:: bash

    $ sudo apt-get install build-essential python-dev

On Red Hat-based distributions (RHEL, CentOS, Fedora, etc.):

.. code-block:: bash

    $ sudo yum install gcc python-devel

On Arch Linux-based distributions, development headers should already be
included in the regular Python distribution:

.. code-block:: bash

    $ sudo pacman -S base-devel gcc python2

Installing from Source
----------------------
You can also install Monary from source, which provides the latest features (but
may be unstable). Simply clone the repository and execute the installation
command::

    $ hg clone https://bitbucket.org/djcbeach/monary
    $ cd monary
    $ python setup.py install
