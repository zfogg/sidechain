Architecture Overview
=====================

This document provides a high-level overview of the Sidechain plugin architecture.
For a comprehensive guide covering modern C++26 patterns, reactive state management,
and implementation patterns, see :doc:`plugin-architecture/index`.

System Overview
---------------

The Sidechain plugin is built using the `JUCE framework <https://juce.com>`_ and follows a modular architecture:

.. code-block:: text

   ┌─────────────────────────────────────────────────────────────┐
   │                  Plugin Editor (UI Layer)                   │
   │  ┌─────────────────────────────────────────────────────────┐
   │  │         UI Components & Components Management           │
   │  │  (Feed, Messages, Profile, Recording, Stories)          │
   │  └─────────────────────────────────────────────────────────┘
   └──┬──────────────────────────────────────────────────────────┘
      │ dispatch actions
      ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                    Store Layer (State)                       │
   │  (PostsStore, ChatStore, UserStore, etc.)                  │
   └──┬──────────────────────────────────────────────────────────┘
      │ network calls
      ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                  Network Layer                              │
   │  ┌──────────────────┐  ┌──────────────────┐                │
   │  │  Network Client  │  │ WebSocket Client │                │
   │  │  (HTTP API)      │  │ (Real-time)      │                │
   │  └──────────────────┘  └──────────────────┘                │
   └──┬──────────────────────────────────────────────────────────┘
      │
      ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                   Backend Services                          │
   │  (Go Server + GetStream.io + Audio CDN)                    │
   └─────────────────────────────────────────────────────────────┘

   ┌─────────────────────────────────────────────────────────────┐
   │              Plugin Processor (Audio Layer)                 │
   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
   │  │ Audio Capture│  │ MIDI Capture │  │ Audio Player │     │
   │  └──────────────┘  └──────────────┘  └──────────────┘     │
   └─────────────────────────────────────────────────────────────┘

Core Components
---------------

Plugin Editor
~~~~~~~~~~~~~

The ``SidechainAudioProcessorEditor`` class is the main UI component that manages:

* View navigation and state
* User authentication
* Component lifecycle
* WebSocket connections
* Notification handling

See :doc:`plugin-architecture/reactive-components` for detailed UI component patterns.

Plugin Processor
~~~~~~~~~~~~~~~~

The ``SidechainAudioProcessor`` class handles:

* Audio processing pipeline
* Audio capture from the DAW
* MIDI capture
* Audio playback

Network Layer
~~~~~~~~~~~~~

The network layer consists of:

* **NetworkClient**: HTTP API client for REST endpoints
* **WebSocketClient**: Real-time WebSocket connections
* **StreamChatClient**: GetStream.io chat integration
* **FeedDataManager**: Feed data management and caching

Audio System
~~~~~~~~~~~

The audio system includes:

* **AudioCapture**: Captures audio from the DAW
* **MIDICapture**: Captures MIDI events
* **HttpAudioPlayer**: Streams audio from URLs
* **BufferAudioPlayer**: Plays audio from memory buffers
* **KeyDetector**: Detects musical key from audio

UI Components
~~~~~~~~~~~~~

The UI is organized into functional components:

* **AuthComponent**: Login and signup
* **PostsFeedComponent**: Social feed display
* **MessageThreadComponent**: Chat interface
* **ProfileComponent**: User profiles
* **RecordingComponent**: Audio recording interface
* **StoryRecordingComponent**: Story creation interface

Data Models
-----------

The plugin uses several data models:

* **FeedPost**: Represents a post in the feed
* **Story**: Represents a story (24-hour expiring content)
* **User**: User profile data
* **Message**: Chat message data

Storage
-------

User data is stored locally using:

* **UserDataStore**: Centralized user data management
* JUCE's ``PropertiesFile`` for persistent storage

Build System
------------

The plugin uses CMake for building:

* Cross-platform support (macOS, Windows, Linux)
* JUCE integration
* Dependency management (ASIO, websocketpp, libkeyfinder)
* Test framework integration (Catch2)

For more details, see the :doc:`development` guide.

See Also
--------

For comprehensive architecture documentation:

* :doc:`plugin-architecture/index` - Complete architecture guide with reactive patterns
* :doc:`plugin-architecture/stores` - Store pattern and state management
* :doc:`plugin-architecture/observables` - Observable collections reference
* :doc:`plugin-architecture/threading` - Threading model and safety constraints
* :doc:`plugin-architecture/data-flow` - Data flow diagrams and patterns
