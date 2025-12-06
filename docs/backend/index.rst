Backend API Documentation
==========================

The Sidechain backend is written in Go and provides the server-side functionality for the VST plugin.

.. toctree::
   :maxdepth: 2
   :caption: Backend Contents:

   overview

Go Package Documentation
------------------------

For detailed API documentation generated from Go package comments, see the `Go package documentation <godoc/pkg/github.com/zfogg/sidechain/backend.html>`_.

The backend is organized into the following packages:

* `internal/handlers <godoc/pkg/github.com/zfogg/sidechain/backend/internal/handlers.html>`_ - HTTP request handlers
* `internal/models <godoc/pkg/github.com/zfogg/sidechain/backend/internal/models.html>`_ - Data models
* `internal/database <godoc/pkg/github.com/zfogg/sidechain/backend/internal/database.html>`_ - Database operations
* `internal/auth <godoc/pkg/github.com/zfogg/sidechain/backend/internal/auth.html>`_ - Authentication and authorization
* `internal/stream <godoc/pkg/github.com/zfogg/sidechain/backend/internal/stream.html>`_ - Stream.io integration
* `internal/storage <godoc/pkg/github.com/zfogg/sidechain/backend/internal/storage.html>`_ - File storage (S3)
* `internal/websocket <godoc/pkg/github.com/zfogg/sidechain/backend/internal/websocket.html>`_ - WebSocket server

