
.. _namespace_ImageLoader:

Namespace ImageLoader
=====================


:ref:`namespace_ImageLoader` - :ref:`namespace_Async` image loading with LRU caching. 




.. contents:: Contents
   :local:
   :backlinks: none




Detailed Description
--------------------

Named :ref:`namespace_ImageLoader` instead of ImageCache to avoid conflict with juce::ImageCache.

Features:

- :ref:`namespace_Async` loading from URLs (doesn't block :ref:`namespace_UI` thread)

- LRU eviction when cache is full

- Placeholder support while loading

- Automatic retry on failure

- Thread-safe

- Drawing helpers for avatars



Usage: // Load image asynchronously :ref:`exhale_function_namespaceImageLoader_1a291ac43216d03d28c0b2f61c1b77f8a5`(url, [this](const juce::Image& img) { avatarImage = img; repaint(); }); 





Namespaces
----------


- :ref:`namespace_ImageLoader__@051335212165155142372313021271201065333052227311`


Classes
-------


- :ref:`exhale_struct_structImageLoader_1_1Stats`


Functions
---------


- :ref:`exhale_function_namespaceImageLoader_1a126f00329a0e7278848e887ae523eec9`

- :ref:`exhale_function_namespaceImageLoader_1a4955d214a0bdc90f2ba3c53f679e6654`

- :ref:`exhale_function_namespaceImageLoader_1a8a4de7d82490630aa83cfebc76e19e97`

- :ref:`exhale_function_namespaceImageLoader_1a344c7580a43651af83e7b016b5bcccd4`

- :ref:`exhale_function_namespaceImageLoader_1ae358332ad76eff05c37ba912d97fccc3`

- :ref:`exhale_function_namespaceImageLoader_1a714e995dfcfef8707b272c9d5339f043`

- :ref:`exhale_function_namespaceImageLoader_1ac9eca413d6d85149b7a6f0e39164c939`

- :ref:`exhale_function_namespaceImageLoader_1a360d22cc87db5683192ca1124d6e3963`

- :ref:`exhale_function_namespaceImageLoader_1a291ac43216d03d28c0b2f61c1b77f8a5`

- :ref:`exhale_function_namespaceImageLoader_1a328332d7f5dafe42e5db51ee916fcd43`

- :ref:`exhale_function_namespaceImageLoader_1a94b226c1b09d67333a1ad902597054d5`

- :ref:`exhale_function_namespaceImageLoader_1a231fd4e9a05addef726bd224324a1658`

- :ref:`exhale_function_namespaceImageLoader_1a53e8a46bd62a45b6a1c9555aadf510ce`

- :ref:`exhale_function_namespaceImageLoader_1ab57b7237e9e1743982ce84f9abaeb688`


Typedefs
--------


- :ref:`exhale_typedef_namespaceImageLoader_1a05491f7302c0fcd3c3f5c2df7d7bbb56`
