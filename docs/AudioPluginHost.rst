rstHow to build
============

Building JUCE extras (like AudioPluginHost):

- Configure JUCE with ``-DJUCE_BUILD_EXTRAS=ON`` to enable the extras targets
- Then build the specific target: ``--target AudioPluginHost``
- On Linux, you needed ladspa for LADSPA plugin hosting support

.. code-block:: bash

   sudo pacman -S ladspa
   cmake -S deps/JUCE -B deps/JUCE/build -DJUCE_BUILD_EXTRAS=ON -DCMAKE_BUILD_TYPE=Release
   cmake --build deps/JUCE/build --target AudioPluginHost --config Release
   ls -la deps/JUCE/build/extras/AudioPluginHost/AudioPluginHost_artefacts/Release/
   ./deps/JUCE/build/extras/AudioPluginHost/AudioPluginHost_artefacts/Release/AudioPluginHost
