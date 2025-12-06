Backend Overview
================

The Sidechain backend is a Go-based REST API server that provides the infrastructure for the VST plugin's social features.

Architecture
------------

The backend is organized into the following packages:

**Handlers** (``internal/handlers/``)
    HTTP request handlers for API endpoints

**Models** (``internal/models/``)
    Data models and database entities

**Database** (``internal/database/``)
    Database connection and migration utilities

**Auth** (``internal/auth/``)
    OAuth authentication and JWT token management

**Stream** (``internal/stream/``)
    Integration with GetStream.io for social features

**Storage** (``internal/storage/``)
    AWS S3 integration for audio file storage

**WebSocket** (``internal/websocket/``)
    Real-time WebSocket server for live updates

Technology Stack
----------------

* **Language**: Go 1.24+
* **Framework**: Gin (HTTP router)
* **Database**: PostgreSQL with GORM ORM
* **Social**: GetStream.io
* **Storage**: AWS S3
* **Authentication**: OAuth2, JWT
* **Real-time**: WebSocket

