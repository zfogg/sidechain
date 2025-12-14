.. _backend-architecture:

Backend Architecture
====================

This section documents the reactive architecture patterns used in the Sidechain VST Plugin.
The plugin follows a modern C++26 reactive architecture with unidirectional data flow,
eliminating callback hell and providing automatic UI updates.

.. toctree::
   :maxdepth: 2
   :caption: Architecture Components:

   stores
   observables
   reactive-components
   data-flow
   threading
   services

Overview
--------

The Sidechain plugin architecture is built on three core concepts:

1. **Store Pattern** - Centralized state management with reactive subscriptions
2. **Observable Pattern** - Reactive data bindings with automatic updates
3. **Unidirectional Data Flow** - Predictable state changes and UI updates

Key Benefits
~~~~~~~~~~~~

* **Single Source of Truth**: All state lives in stores (FeedStore, ChatStore, UserStore)
* **Automatic UI Updates**: Components automatically repaint when store state changes
* **Type Safety**: Full C++26 type checking with templates
* **Thread Safety**: Proper message thread marshalling with JUCE integration
* **Testability**: Pure functions and dependency injection enable comprehensive testing

Architecture Diagram
~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

    ┌─────────────────────────────────────────────────────────────┐
    │                     User Interface Layer                     │
    │  (PostsFeed, MessageThread, ProfileView - ReactiveBoundComponent) │
    └────────────────────┬────────────────────────────────────────┘
                         │ subscribe()
                         ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                      Store Layer                             │
    │     (FeedStore, ChatStore, UserStore - Store<TState>)        │
    └────────────────────┬────────────────────────────────────────┘
                         │ dispatch()
                         ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                    Service Layer                             │
    │    (FeedService, AudioService, AuthService)                  │
    └────────────────────┬────────────────────────────────────────┘
                         │ API calls
                         ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                   Network Layer                              │
    │   (NetworkClient, WebSocketClient, StreamChatClient)         │
    └─────────────────────────────────────────────────────────────┘

Modern C++ Features Used
~~~~~~~~~~~~~~~~~~~~~~~~~

* **C++26 Standard**: Full use of modern C++ features
* **Smart Pointers**: ``std::unique_ptr``, ``std::shared_ptr`` for RAII
* **Templates**: ``ObservableProperty<T>``, ``Store<TState>``
* **Lambda Expressions**: Reactive subscriptions and callbacks
* **std::function**: Type-safe function objects
* **std::atomic**: Lock-free audio thread communication
* **Move Semantics**: Efficient resource transfer

See Also
~~~~~~~~

* :doc:`stores` - Centralized state management
* :doc:`observables` - Reactive property bindings
* :doc:`reactive-components` - UI component integration
* :doc:`data-flow` - Data flow patterns and examples
* :doc:`threading` - Threading model and safety
* :doc:`services` - Business logic layer

Backend Integration
~~~~~~~~~~~~~~~~~~~

For information about backend support and requirements:
* `Backend Investigation Notes <../BACKEND_INVESTIGATION_NOTES.md>`_ - Assessment of backend support for real-time collaboration (OT, error tracking, rate limiting)
* Known Issues:
  - Operational Transform: Not implemented on backend (required for Task 4.20)
  - Error Tracking API: Not implemented (required for Task 4.19)
  - Rate Limiting: Implemented with minor header bug in Retry-After header
