
.. _namespace_Log:

Namespace Log
=============


:ref:`namespace_Log` - Simple logging utility for the Sidechain plugin. 




.. contents:: Contents
   :local:
   :backlinks: none




Detailed Description
--------------------

Output behavior:

- debug/info: stdout

- warn/error: stderr

- All levels: written to log file



:ref:`namespace_Log` file location (determined by NDEBUG macro):

- Development builds (NDEBUG not defined): ./plugin.log (current working directory)

- Release builds (NDEBUG defined): Platform-specific standard log directory

   - macOS: ~/Library/Logs/Sidechain/plugin.log

   - Linux: ~/.local/share/Sidechain/logs/plugin.log

   - Windows: LOCALAPPDATA%/Sidechain/logs/plugin.log





Thread-safe and gracefully handles missing directories or inaccessible files. 





Namespaces
----------


- :ref:`namespace_Log__@100115140125046133325374270265002270121145204375`


Enums
-----


- :ref:`exhale_enum_namespaceLog_1afb5d4c945d835d194a295461d752531e`


Functions
---------


- :ref:`exhale_function_namespaceLog_1ad889cf1c1a37892eb664cc43f075fc3b`

- :ref:`exhale_function_namespaceLog_1a694fcab0122ee1c358a8b78d0707e02c`

- :ref:`exhale_function_namespaceLog_1aad1f659720fea4e35a300892a64499c8`

- :ref:`exhale_function_namespaceLog_1a42615492d78bb6c3fb6c9949e9d70da1`

- :ref:`exhale_function_namespaceLog_1a48b8faa1f9d2590c6fb650f1004e73c9`

- :ref:`exhale_function_namespaceLog_1a7cf397325784b1e4ed5c523021d7eade`

- :ref:`exhale_function_namespaceLog_1ad911cf62f36ebd6ec98c5545d87c6b1a`

- :ref:`exhale_function_namespaceLog_1a2cde6a6aa92bfccc7943252c6dce9135`

- :ref:`exhale_function_namespaceLog_1aa0fdb288a9fc63e58d56da2c50e3cf42`

- :ref:`exhale_function_namespaceLog_1a0557924d09d5f6102309dba9e8697663`

- :ref:`exhale_function_namespaceLog_1a751e27e65afb19e3dc748e25f2941c27`

- :ref:`exhale_function_namespaceLog_1a76233755459a6a5faccf15f50ee01866`

- :ref:`exhale_function_namespaceLog_1ac56b65add740b565f60ed5b4314c473a`

- :ref:`exhale_function_namespaceLog_1a062ba0d49d2dc5f03044ea83a659c872`

- :ref:`exhale_function_namespaceLog_1a8790e053258a8c5b31685c85c99ec308`

- :ref:`exhale_function_namespaceLog_1a622144850d882bc44c29d119f564f15c`

- :ref:`exhale_function_namespaceLog_1ab250ad8802c143c2960e42940be43518`

- :ref:`exhale_function_namespaceLog_1a534985e980ff6c0068bc8d61a42d3ccd`
