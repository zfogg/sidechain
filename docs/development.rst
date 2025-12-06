Development Guide
=================

This guide covers development setup, building, testing, and contributing to the Sidechain plugin.

Prerequisites
-------------

* **CMake** 3.22 or later
* **C++23** compatible compiler:
  - Clang 15+ (macOS, Linux)
  - GCC 13+ (Linux)
  - MSVC 2022+ (Windows)
* **Python 3.8+** (for documentation)
* **Git** with submodules support

Dependencies
------------

The plugin depends on:

* **JUCE** 8.0.11+ (audio framework)
* **ASIO** (standalone, for networking)
* **websocketpp** (WebSocket client)
* **libkeyfinder** (optional, for key detection, requires FFTW3)

Building
--------

Clone the repository with submodules:

.. code-block:: bash

   git clone --recursive https://github.com/zfogg/sidechain.git
   cd sidechain

Configure and build:

.. code-block:: bash

   cd plugin
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release --parallel

Build options:

* ``-DSIDECHAIN_BUILD_TESTS=ON``: Enable unit tests
* ``-DSIDECHAIN_ENABLE_COVERAGE=ON``: Enable code coverage (requires tests)
* ``-DSIDECHAIN_BUILD_PLUGIN_HOST=ON``: Build AudioPluginHost for testing
* ``-DSIDECHAIN_BUILD_STANDALONE=ON``: Build standalone application
* ``-DSIDECHAIN_BUILD_DOCS=ON``: Build documentation

Running Tests
-------------

Build and run tests:

.. code-block:: bash

   cmake -S . -B build -DSIDECHAIN_BUILD_TESTS=ON
   cmake --build build --target SidechainTests
   cd build
   ctest --output-on-failure

Generate coverage report:

.. code-block:: bash

   cmake -S . -B build -DSIDECHAIN_BUILD_TESTS=ON -DSIDECHAIN_ENABLE_COVERAGE=ON
   cmake --build build --target coverage
   # Open build/coverage/html/index.html

Building Documentation
---------------------

Install Python dependencies:

.. code-block:: bash

   pip install -r docs/requirements.txt

Build documentation:

.. code-block:: bash

   cmake -S plugin -B plugin/build -DSIDECHAIN_BUILD_DOCS=ON
   cmake --build plugin/build --target docs
   # Documentation will be in plugin/build/docs/html/

Or build manually:

.. code-block:: bash

   cd docs
   doxygen Doxyfile
   sphinx-build -b html . _build/html

Code Style
----------

* Follow JUCE coding conventions
* Use C++23 features where appropriate
* Document public APIs with Doxygen comments
* Keep functions focused and small
* Use ``const`` and ``constexpr`` where possible

Documentation Style
------------------

Use Doxygen-style comments for API documentation:

.. code-block:: cpp

   /**
    * Brief description of the class/function.
    *
    * Detailed description can span multiple paragraphs.
    *
    * @param paramName Description of parameter
    * @return Description of return value
    */
   void exampleFunction(int paramName);

For narrative documentation, use reStructuredText (`.rst`) or Markdown (`.md`).

Debugging
---------

**macOS:**
* Use Xcode for debugging
* Attach to the plugin host process
* Set breakpoints in plugin code

**Windows:**
* Use Visual Studio debugger
* Attach to your DAW process
* Load the plugin in the DAW

**Linux:**
* Use GDB or LLDB
* Attach to the plugin host
* Use ``gdb --pid=<pid>``

Troubleshooting
---------------

**Plugin not loading:**
* Check plugin format (VST3/AU) matches your DAW
* Verify installation path
* Check console/logs for errors

**Build errors:**
* Ensure all submodules are initialized: ``git submodule update --init --recursive``
* Check CMake version (3.22+)
* Verify compiler supports C++23

**Documentation build errors:**
* Ensure Doxygen is installed: ``brew install doxygen`` (macOS) or ``apt install doxygen`` (Linux)
* Install Python dependencies: ``pip install -r docs/requirements.txt``
* Run Doxygen first, then Sphinx

Contributing
------------

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Update documentation
6. Submit a pull request

For more information, see the main repository README.
