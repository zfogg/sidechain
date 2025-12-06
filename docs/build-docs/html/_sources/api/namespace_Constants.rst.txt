
.. _namespace_Constants:

Namespace Constants
===================


:ref:`namespace_Constants` - Centralized magic numbers for the Sidechain plugin. 




.. contents:: Contents
   :local:
   :backlinks: none




Detailed Description
--------------------

This header consolidates configuration values that were previously scattered throughout the codebase. Groupings:

- Endpoints: API URLs for different environments

- Api: Network timeouts, retry limits

- Audio: :ref:`exhale_class_classRecording` limits, BPM ranges, cache sizes

- :ref:`namespace_UI`: Component dimensions, corner radii, avatar sizes

- Cache: Image and audio cache limits



Usage: #include "util/Constants.h"

// API URLs juce::URL url(Constants::Endpoints::DEV_BASE_URL + "/api/v1/auth/login");

// API timeouts .withConnectionTimeoutMs(Constants::Api::DEFAULT_TIMEOUT_MS)

// :ref:`namespace_UI` dimensions setSize(width, Constants::Ui::HEADER_HEIGHT) 





Namespaces
----------


- :ref:`namespace_Constants__Api`

- :ref:`namespace_Constants__Audio`

- :ref:`namespace_Constants__Cache`

- :ref:`namespace_Constants__Endpoints`

- :ref:`namespace_Constants__Errors`

- :ref:`namespace_Constants__Limits`

- :ref:`namespace_Constants__Pagination`

- :ref:`namespace_Constants__Timing`

- :ref:`namespace_Constants__Ui`
