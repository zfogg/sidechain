.. _threading:

Threading Model
===============

The Sidechain plugin uses JUCE's threading model with strict rules for audio thread safety.
Understanding the threading model is critical for writing correct, performant code.

Thread Types
------------

Audio Thread
~~~~~~~~~~~~

**Purpose**: Process audio with real-time constraints

**Characteristics**:

* Highest priority thread
* Real-time deadline (typically ~10ms at 512 samples, 48kHz)
* **NO allocations** allowed
* **NO locks** (mutex, etc.) allowed
* **NO system calls** allowed
* **NO file I/O** allowed
* **NO network calls** allowed

**What CAN run on audio thread**:

.. code-block:: cpp

    void PluginProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi) {
        // ✅ Lock-free atomic reads
        float gain = gainParameter.get();  // AtomicObservableProperty

        // ✅ Pre-allocated buffer operations
        buffer.applyGain(gain);

        // ✅ Simple math
        float rms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());

        // ✅ Lock-free FIFO operations
        if (audioFifo.getNumReady() > 0) {
            audioFifo.pull(tempBuffer, numSamples);
        }
    }

**What CANNOT run on audio thread**:

.. code-block:: cpp

    void PluginProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi) {
        // ❌ NEVER allocate
        auto newBuffer = new AudioBuffer<float>(2, 512);  // DEADLOCK RISK!

        // ❌ NEVER lock
        std::lock_guard<std::mutex> lock(mutex);  // REAL-TIME VIOLATION!

        // ❌ NEVER call network
        networkClient->upload(buffer);  // BLOCKS FOR HUNDREDS OF MS!

        // ❌ NEVER use ObservableProperty (has mutex)
        username.set("Alice");  // DEADLOCK RISK!

        // ❌ NEVER file I/O
        file.write(buffer.getReadPointer(0), buffer.getNumSamples());  // BLOCKS!
    }

Message Thread
~~~~~~~~~~~~~~

**Purpose**: Handle UI events and updates

**Characteristics**:

* Medium priority
* Processes JUCE component events (mouse, keyboard, paint)
* Where all UI code runs
* Can allocate, lock, make system calls
* Thread-safe for JUCE components

**Typical operations**:

.. code-block:: cpp

    void MyComponent::mouseUp(const juce::MouseEvent& event) {
        // ✅ UI updates
        repaint();

        // ✅ Store operations
        feedStore->toggleLike(postId);

        // ✅ Component changes
        button.setEnabled(false);

        // ✅ Modal dialogs
        showDialog();
    }

Background Threads
~~~~~~~~~~~~~~~~~~

**Purpose**: Network calls, file I/O, heavy computation

**Characteristics**:

* Low priority
* Used for async operations
* Can block without affecting audio/UI
* Must marshal results back to message thread for UI updates

**Example**:

.. code-block:: cpp

    // Background network call
    networkClient->getFeed(FeedType::Timeline, [](Outcome<FeedResponse> result) {
        // Callback runs on background thread

        // Marshal to message thread for UI update
        juce::MessageManager::callAsync([result]() {
            // Now on message thread - safe for UI
            if (result.isOk()) {
                displayFeed(result.getValue().posts);
            }
        });
    });

Thread Safety Rules
-------------------

Rule 1: No Audio Thread Locks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Never lock mutex on audio thread**:

.. code-block:: cpp

    // ❌ BAD: Mutex on audio thread
    std::mutex dataMutex;

    void processBlock(AudioBuffer<float>& buffer) {
        std::lock_guard<std::mutex> lock(dataMutex);  // REAL-TIME VIOLATION!
        processAudio(buffer);
    }

    // ✅ GOOD: Lock-free atomic
    AtomicObservableProperty<float> gain{1.0f};

    void processBlock(AudioBuffer<float>& buffer) {
        buffer.applyGain(gain.get());  // Lock-free read!
    }

Rule 2: Pre-Allocate Everything
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Allocate in constructor, use in processBlock**:

.. code-block:: cpp

    class AudioProcessor {
    public:
        AudioProcessor() {
            // ✅ Allocate in constructor
            fftBuffer.setSize(2, 2048);
            tempBuffer.setSize(2, 512);
        }

        void processBlock(AudioBuffer<float>& buffer) {
            // ✅ Use pre-allocated buffers
            fftBuffer.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());

            // ❌ NEVER allocate here
            // AudioBuffer<float> temp(2, 512);  // DEADLOCK RISK!
        }

    private:
        AudioBuffer<float> fftBuffer;
        AudioBuffer<float> tempBuffer;
    };

Rule 3: Atomic for Cross-Thread Communication
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Use atomics for audio → UI communication**:

.. code-block:: cpp

    class AudioProcessor {
    public:
        void processBlock(AudioBuffer<float>& buffer) {
            // Audio thread updates atomic
            float level = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
            currentLevel.store(level, std::memory_order_release);
        }

        float getCurrentLevel() const {
            // UI thread reads atomic
            return currentLevel.load(std::memory_order_acquire);
        }

    private:
        std::atomic<float> currentLevel{0.0f};
    };

Rule 4: Message Thread for UI Updates
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Always update UI on message thread**:

.. code-block:: cpp

    // ❌ BAD: UI update from background thread
    networkClient->getFeed([this](const FeedResponse& response) {
        // This callback runs on background thread!
        displayPosts(response.posts);  // CRASH! Component not thread-safe
    });

    // ✅ GOOD: Marshal to message thread
    networkClient->getFeed([this](const FeedResponse& response) {
        juce::MessageManager::callAsync([this, response]() {
            // Now on message thread
            displayPosts(response.posts);  // Safe!
        });
    });

Rule 5: Store Subscriptions Are Message Thread
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Store subscriptions automatically run on message thread**:

.. code-block:: cpp

    feedStore->subscribe([this](const FeedStoreState& state) {
        // This runs on message thread automatically
        repaint();  // Safe!
        button.setEnabled(true);  // Safe!
    });

Common Patterns
---------------

Audio → UI Communication
~~~~~~~~~~~~~~~~~~~~~~~~

Use atomics for lock-free communication:

.. code-block:: cpp

    class LevelMeter : public juce::Component, public juce::Timer {
    public:
        LevelMeter(AudioProcessor& processor) : processor(processor) {
            startTimer(30);  // ~30fps update rate
        }

        void timerCallback() override {
            // Read current level from audio thread (lock-free)
            float newLevel = processor.getCurrentLevel();

            if (std::abs(newLevel - displayLevel) > 0.01f) {
                displayLevel = newLevel;
                repaint();
            }
        }

        void paint(juce::Graphics& g) override {
            drawLevelMeter(g, displayLevel);
        }

    private:
        AudioProcessor& processor;
        float displayLevel = 0.0f;
    };

UI → Audio Communication
~~~~~~~~~~~~~~~~~~~~~~~~~

Use atomics for parameter changes:

.. code-block:: cpp

    class GainKnob : public juce::Slider {
    public:
        GainKnob(AtomicObservableProperty<float>& gainParam) : gainParam(gainParam) {
            onValueChange = [this]() {
                // UI thread writes to atomic
                gainParam.set(static_cast<float>(getValue()));
            };
        }

    private:
        AtomicObservableProperty<float>& gainParam;
    };

    // Audio thread reads lock-free
    void processBlock(AudioBuffer<float>& buffer) {
        buffer.applyGain(gainParam.get());  // Lock-free!
    }

Background Processing
~~~~~~~~~~~~~~~~~~~~~

Use JUCE ThreadPool or custom threads:

.. code-block:: cpp

    void MyComponent::onExportButtonClicked() {
        // Disable button during export
        exportButton.setEnabled(false);

        // Run export on background thread
        juce::Thread::launch([this]() {
            // Background thread - can block
            exportAudioFile();

            // Marshal completion to message thread
            juce::MessageManager::callAsync([this]() {
                exportButton.setEnabled(true);
                showCompletionDialog();
            });
        });
    }

Lock-Free FIFO
~~~~~~~~~~~~~~

Use AbstractFifo for audio thread communication:

.. code-block:: cpp

    class AudioRecorder {
    public:
        AudioRecorder() : fifo(8192) {
            buffer.setSize(2, 8192);
        }

        void processBlock(const AudioBuffer<float>& input) {
            // Audio thread writes to FIFO (lock-free)
            int numSamples = input.getNumSamples();

            if (fifo.getFreeSpace() >= numSamples) {
                int start1, size1, start2, size2;
                fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

                if (size1 > 0)
                    buffer.copyFrom(0, start1, input, 0, 0, size1);
                if (size2 > 0)
                    buffer.copyFrom(0, start2, input, 0, size1, size2);

                fifo.finishedWrite(size1 + size2);
            }
        }

        void pullRecordedAudio(AudioBuffer<float>& destination) {
            // Background thread reads from FIFO (lock-free)
            int numReady = fifo.getNumReady();
            if (numReady > 0) {
                int start1, size1, start2, size2;
                fifo.prepareToRead(numReady, start1, size1, start2, size2);

                if (size1 > 0)
                    destination.copyFrom(0, 0, buffer, 0, start1, size1);
                if (size2 > 0)
                    destination.copyFrom(0, size1, buffer, 0, start2, size2);

                fifo.finishedRead(size1 + size2);
            }
        }

    private:
        juce::AbstractFifo fifo;
        AudioBuffer<float> buffer;
    };

Observable Thread Safety
-------------------------

ObservableProperty<T>
~~~~~~~~~~~~~~~~~~~~~

**Thread-safe but uses mutex**:

.. code-block:: cpp

    ObservableProperty<juce::String> username{"Alice"};

    // ✅ Safe from any thread (but has mutex)
    username.set("Bob");  // Locks mutex

    // ❌ NEVER use on audio thread
    void processBlock(AudioBuffer<float>& buffer) {
        auto name = username.get();  // DEADLOCK RISK!
    }

AtomicObservableProperty<T>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Lock-free, safe for audio thread reads**:

.. code-block:: cpp

    AtomicObservableProperty<float> gain{1.0f};

    // ✅ Safe on audio thread (lock-free)
    void processBlock(AudioBuffer<float>& buffer) {
        buffer.applyGain(gain.get());  // Lock-free!
    }

    // ✅ Safe from UI thread
    void onSliderChange() {
        gain.set(slider.getValue());  // Atomic write
    }

Store Thread Safety
~~~~~~~~~~~~~~~~~~~

**Stores run on message thread**:

.. code-block:: cpp

    // ✅ Safe - stores always on message thread
    feedStore->toggleLike(postId);

    // ✅ Safe - subscriptions on message thread
    feedStore->subscribe([this](const auto& state) {
        repaint();  // Safe!
    });

    // ❌ NEVER access from audio thread
    void processBlock(AudioBuffer<float>& buffer) {
        feedStore->loadFeed(FeedType::Timeline);  // DEADLOCK RISK!
    }

Debugging Thread Issues
-----------------------

Detecting Audio Thread Violations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use JUCE assertions:

.. code-block:: cpp

    void processBlock(AudioBuffer<float>& buffer) {
        // Assert we're on audio thread
        jassert(juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager() == false);

        // Detect allocations in debug builds
        #if JUCE_DEBUG
        juce::ScopedNoDenormals noDenormals;
        #endif
    }

Detecting UI Thread Violations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    void MyComponent::paint(juce::Graphics& g) {
        // Assert we're on message thread
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    }

Trace Deadlocks
~~~~~~~~~~~~~~~

.. code-block:: cpp

    std::mutex dataMutex;

    void processBlock(AudioBuffer<float>& buffer) {
        #if JUCE_DEBUG
        DBG("Audio thread attempting lock...");
        #endif

        std::lock_guard<std::mutex> lock(dataMutex);  // Will deadlock if held

        #if JUCE_DEBUG
        DBG("Audio thread acquired lock");  // Never prints = deadlock!
        #endif
    }

Performance Profiling
---------------------

Measure Audio Thread Performance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    void processBlock(AudioBuffer<float>& buffer) {
        auto startTime = juce::Time::getHighResolutionTicks();

        // Process audio
        processAudio(buffer);

        auto endTime = juce::Time::getHighResolutionTicks();
        auto elapsedMs = juce::Time::highResolutionTicksToSeconds(endTime - startTime) * 1000.0;

        // Log if processing took > 50% of buffer duration
        double bufferDurationMs = buffer.getNumSamples() * 1000.0 / getSampleRate();
        if (elapsedMs > bufferDurationMs * 0.5) {
            DBG("WARNING: Audio processing took " + juce::String(elapsedMs, 2) + "ms (buffer: " + juce::String(bufferDurationMs, 2) + "ms)");
        }
    }

Monitor CPU Usage
~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    class PerformanceMonitor {
    public:
        void recordProcessingTime(double timeMs) {
            totalTime += timeMs;
            callCount++;

            if (callCount % 100 == 0) {
                double avgTime = totalTime / callCount;
                DBG("Average processing time: " + juce::String(avgTime, 3) + "ms");
                totalTime = 0.0;
                callCount = 0;
            }
        }

    private:
        double totalTime = 0.0;
        int callCount = 0;
    };

Best Practices Summary
----------------------

Audio Thread
~~~~~~~~~~~~

* ✅ Use ``AtomicObservableProperty`` for parameters
* ✅ Pre-allocate all buffers in constructor
* ✅ Use lock-free data structures (``AbstractFifo``, ``std::atomic``)
* ✅ Keep processing < 50% of buffer duration
* ❌ Never allocate memory
* ❌ Never use mutex/locks
* ❌ Never use ``ObservableProperty`` (has mutex)
* ❌ Never do file I/O or network calls

Message Thread
~~~~~~~~~~~~~~

* ✅ All UI updates here
* ✅ Store operations here
* ✅ Component events here
* ✅ Can use ``ObservableProperty``
* ✅ Can allocate, lock, block
* ❌ Don't do heavy computation (use background thread)

Background Thread
~~~~~~~~~~~~~~~~~

* ✅ Network calls
* ✅ File I/O
* ✅ Heavy computation
* ✅ Can block
* ❌ Don't update UI directly (marshal to message thread)

See Also
--------

* :doc:`observables` - Observable thread safety
* :doc:`stores` - Store thread safety
* :doc:`data-flow` - Complete data flow with threading
* JUCE Threading Documentation: https://docs.juce.com/master/classThread.html
