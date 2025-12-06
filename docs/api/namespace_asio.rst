
.. _namespace_asio:

Namespace asio
==============


ASIO Compatibility Layer. 




.. contents:: Contents
   :local:
   :backlinks: none




Detailed Description
--------------------

Compatibility layer for websocketpp to work with newer standalone ASIO.

Problem:

- websocketpp expects ``io_service`` but newer ASIO (1.18+) uses ``io_context``

- websocketpp hasn't been updated for ASIO 1.18+



Solution:

- Provides aliases and compatibility wrappers to bridge the gap

- Maps ``io_service`` -> ``io_context``

- Maps deprecated APIs to modern equivalents



This is a known compatibility issue in the websocketpp library 







Namespaces
----------


- :ref:`namespace_asio__io_context_work`


Functions
---------


- :ref:`exhale_function_namespaceasio_1a1377d9946fd344206e06079bd118f98e`


Typedefs
--------


- :ref:`exhale_typedef_namespaceasio_1a1366b079e33073fddaccac22a2ff50fa`

- :ref:`exhale_typedef_namespaceasio_1ae53b866dd77b1c232af1be59ef581b72`
