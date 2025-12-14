# Sidechain VST Plugin Documentation

This directory contains the architecture documentation for the Sidechain VST Plugin, written in reStructuredText (RST) format for Sphinx.

## Building the Documentation

### Prerequisites

Install Sphinx and the Read the Docs theme:

```bash
pip install sphinx sphinx_rtd_theme
```

### Build HTML Documentation

```bash
cd plugin/docs
make html
```

The generated HTML will be in `_build/html/`. Open `_build/html/index.html` in your browser.

### Other Formats

```bash
# Build PDF (requires LaTeX)
make latexpdf

# Build EPUB
make epub

# Clean build artifacts
make clean
```

## Documentation Structure

```
docs/
├── index.rst                    # Main entry point
├── architecture/
│   ├── index.rst               # Backend Architecture overview
│   ├── stores.rst              # Store Pattern documentation
│   ├── observables.rst         # Observable Pattern documentation
│   ├── reactive-components.rst # ReactiveBoundComponent documentation
│   ├── data-flow.rst           # Data Flow patterns and examples
│   ├── threading.rst           # Threading Model and safety
│   └── services.rst            # Service Layer documentation
├── conf.py                     # Sphinx configuration
├── Makefile                    # Build automation
└── README.md                   # This file
```

## Documentation Topics

### Backend Architecture

The architecture section covers the reactive patterns used throughout the plugin:

- **Store Pattern**: Centralized state management (FeedStore, ChatStore, UserStore)
- **Observable Pattern**: Reactive property bindings and collections
- **Reactive Components**: Automatic UI updates via ReactiveBoundComponent
- **Data Flow**: Complete examples of unidirectional data flow
- **Threading Model**: Audio thread safety and message thread marshalling
- **Service Layer**: Business logic and domain operations

## Writing Documentation

### RST Syntax

- Headers: Use `=`, `-`, `~`, `^` underlines
- Code blocks: Use `.. code-block:: cpp` directive
- Links: Use `:doc:` for internal refs, `:ref:` for labels
- Lists: Use `*` or `1.` for bullet/numbered lists

Example:

```rst
Section Header
==============

Subsection
----------

.. code-block:: cpp

    // C++ code example
    void example() {
        std::cout << "Hello" << std::endl;
    }

See :doc:`stores` for more information.
```

### Adding New Pages

1. Create a new `.rst` file in the appropriate subdirectory
2. Add it to the `toctree` in `index.rst` or the parent page
3. Build and verify: `make html`

## Viewing Online

The documentation can be hosted on:

- **Read the Docs**: https://readthedocs.org/
- **GitHub Pages**: Build HTML and push to `gh-pages` branch
- **Local Server**: `python -m http.server -d _build/html`

## Contributing

When updating the codebase, please update the corresponding documentation:

- New stores → Update `stores.rst`
- New observable patterns → Update `observables.rst`
- New data flows → Update `data-flow.rst`
- Threading changes → Update `threading.rst`

## License

Copyright © 2024 Sidechain. All rights reserved.
