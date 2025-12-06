Architecture
===========

This document provides an overview of the Sidechain plugin architecture.

System Overview
---------------

The Sidechain plugin is built using the `JUCE framework <https://juce.com>`_ and follows a modular architecture:

.. graphviz::

   digraph architecture {
       rankdir=TB;
       node [shape=box];

       "Plugin Editor" -> "UI Components";
       "Plugin Editor" -> "Network Client";
       "Plugin Editor" -> "WebSocket Client";
       "Plugin Editor" -> "User Data Store";

       "Plugin Processor" -> "Audio Capture";
       "Plugin Processor" -> "MIDI Capture";
       "Plugin Processor" -> "Audio Players";

       "Network Client" -> "Backend API";
       "WebSocket Client" -> "Backend WebSocket";

       "UI Components" -> "Feed";
       "UI Components" -> "Messages";
       "UI Components" -> "Profile";
       "UI Components" -> "Recording";
       "UI Components" -> "Stories";
   }

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

See :doc:`api/library_root` for detailed API documentation.

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
