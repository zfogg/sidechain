Backend API Documentation
==========================

The Sidechain backend is written in Go and provides the server-side functionality for the VST plugin.

.. toctree::
   :maxdepth: 2
   :caption: Backend Contents:

   overview

Go Package Documentation
------------------------

For detailed API documentation generated from Go package comments, see the `Go package documentation <godoc/zfogg/sidechain/backend/index.html>`_.

The backend is organized into the following packages:

* `internal/handlers <godoc/zfogg/sidechain/backend/internal/handlers/index.html>`_ - HTTP request handlers
* `internal/models <godoc/zfogg/sidechain/backend/internal/models/index.html>`_ - Data models
* `internal/database <godoc/zfogg/sidechain/backend/internal/database/index.html>`_ - Database operations
* `internal/auth <godoc/zfogg/sidechain/backend/internal/auth/index.html>`_ - Authentication and authorization
* `internal/stream <godoc/zfogg/sidechain/backend/internal/stream/index.html>`_ - Stream.io integration
* `internal/storage <godoc/zfogg/sidechain/backend/internal/storage/index.html>`_ - File storage (S3)
* `internal/websocket <godoc/zfogg/sidechain/backend/internal/websocket/index.html>`_ - WebSocket server

