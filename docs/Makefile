# Minimal Sphinx Makefile
# This Makefile provides the standard Sphinx targets for building documentation
# Automatically uses virtual environment if it exists

SPHINXOPTS    ?=
SOURCEDIR    = .
BUILDDIR     = _build

# Check for virtual environment and use it if available
VENV_PYTHON := $(shell if [ -f .venv/bin/python3 ]; then echo .venv/bin/python3; elif [ -f .venv/bin/python ]; then echo .venv/bin/python; else echo python3; fi)
SPHINXBUILD  ?= $(VENV_PYTHON) -m sphinx

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

