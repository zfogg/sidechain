
.. _namespace_Async:

Namespace Async
===============


:ref:`namespace_Async` - Threading utilities for background work and :ref:`namespace_UI` callbacks. 




.. contents:: Contents
   :local:
   :backlinks: none




Detailed Description
--------------------

Provides clean abstractions over common threading patterns:

- :ref:`exhale_function_namespaceAsync_1a14dc5c8132b8d48281d206c951bfce1b`: Execute work on background thread, callback on message thread

- :ref:`exhale_function_namespaceAsync_1a7450afdc3e5d5e95d1611d6a79bb683a`: Fire-and-forget background work with :ref:`namespace_UI` callback

- :ref:`exhale_function_namespaceAsync_1a362eef472457225b32790d2cbde4de68`: Execute a callback after a delay on message thread

- :ref:`exhale_function_namespaceAsync_1a91f0bf70da9746ccc426a1d621ca0035`: Coalesce rapid calls (e.g., search input)



Usage: // Background work with result :ref:`exhale_function_namespaceAsync_1a14dc5c8132b8d48281d206c951bfce1b`( []() { return downloadImage(); }, [this](const juce::Image& img) { avatarImage = img; repaint(); } );

// Fire-and-forget with completion callback :ref:`exhale_function_namespaceAsync_1a7450afdc3e5d5e95d1611d6a79bb683a`( []() { doExpensiveWork(); }, [this]() { isLoading = false; repaint(); } );

// Delayed execution :ref:`exhale_function_namespaceAsync_1a362eef472457225b32790d2cbde4de68`(500, [this]() { showTooltip(); });

// Debounced search (only fires after 300ms of inactivity) :ref:`exhale_function_namespaceAsync_1a91f0bf70da9746ccc426a1d621ca0035`("search", 300, [this, query]() { performSearch(query); }); 





Namespaces
----------


- :ref:`namespace_Async__@104056163216123175022316213030114364035151265273`


Functions
---------


- :ref:`exhale_function_namespaceAsync_1a5e51f7f4adcb378de8939e261d429c55`

- :ref:`exhale_function_namespaceAsync_1a1df86a47d26fddac495e9327ab7fbe2c`

- :ref:`exhale_function_namespaceAsync_1a8c50ed88ee57c1de25f8526d8b8651e1`

- :ref:`exhale_function_namespaceAsync_1a43e086fb7256a45c84fbfe5c9d5b76df`

- :ref:`exhale_function_namespaceAsync_1a91f0bf70da9746ccc426a1d621ca0035`

- :ref:`exhale_function_namespaceAsync_1a362eef472457225b32790d2cbde4de68`

- :ref:`exhale_function_namespaceAsync_1a14dc5c8132b8d48281d206c951bfce1b`

- :ref:`exhale_function_namespaceAsync_1a7450afdc3e5d5e95d1611d6a79bb683a`

- :ref:`exhale_function_namespaceAsync_1a2e6d168c110d2272a75be469ecee7a80`
