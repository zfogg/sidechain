
.. _program_listing_file_plugin_source_util_Async.cpp:

Program Listing for File Async.cpp
==================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_Async.cpp>` (``plugin/source/util/Async.cpp``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #include "Async.h"
   
   namespace Async
   {
       //==========================================================================
       // Internal Timer Management
       //
       // We use a custom timer class to manage delayed execution. Each timer
       // has a unique ID and stores its callback. This pattern avoids the need
       // for users to manage timer lifecycle manually.
       //==========================================================================
   
       namespace
       {
           // Thread-safe counter for generating unique timer IDs
           std::atomic<int> nextTimerId{1};
   
           // Custom timer class for delayed execution
           class DelayTimer : public juce::Timer
           {
           public:
               DelayTimer(int id, std::function<void()> cb)
                   : timerId(id), callback(std::move(cb)) {}
   
               void timerCallback() override
               {
                   stopTimer();
                   if (callback)
                       callback();
                   // Self-destruct after firing
                   // Must be done on message thread, which we're already on
                   juce::MessageManager::callAsync([this]() {
                       delete this;
                   });
               }
   
               int getTimerId() const { return timerId; }
   
           private:
               int timerId;
               std::function<void()> callback;
           };
   
           // Active delay timers (protected by mutex)
           std::mutex delayTimersMutex;
           std::map<int, DelayTimer*> delayTimers;
   
           // Debounce timers keyed by string (protected by mutex)
           std::mutex debounceMutex;
           std::map<juce::String, std::unique_ptr<juce::Timer>> debounceTimers;
   
           // Throttle state keyed by string (protected by mutex)
           std::mutex throttleMutex;
           struct ThrottleState
           {
               juce::int64 lastExecutionTime = 0;
               std::unique_ptr<juce::Timer> pendingTimer;
           };
           std::map<juce::String, ThrottleState> throttleStates;
   
           // Simple timer wrapper for debounce/throttle
           class CallbackTimer : public juce::Timer
           {
           public:
               explicit CallbackTimer(std::function<void()> cb)
                   : callback(std::move(cb)) {}
   
               void timerCallback() override
               {
                   stopTimer();
                   if (callback)
                       callback();
               }
   
               void setCallback(std::function<void()> cb)
               {
                   callback = std::move(cb);
               }
   
           private:
               std::function<void()> callback;
           };
   
       }  // anonymous namespace
   
       //==========================================================================
       // Background Work (void version)
       //==========================================================================
   
       void runVoid(std::function<void()> work, std::function<void()> onComplete)
       {
           std::thread([work = std::move(work), onComplete = std::move(onComplete)]() {
               if (work)
                   work();
   
               if (onComplete)
               {
                   juce::MessageManager::callAsync([onComplete]() {
                       onComplete();
                   });
               }
           }).detach();
       }
   
       //==========================================================================
       // Delayed Execution
       //==========================================================================
   
       int delay(int delayMs, std::function<void()> callback)
       {
           if (!callback || delayMs < 0)
               return 0;
   
           int timerId = nextTimerId.fetch_add(1);
   
           // Must create timer on message thread
           juce::MessageManager::callAsync([timerId, delayMs, callback = std::move(callback)]() {
               auto* timer = new DelayTimer(timerId, callback);
   
               {
                   std::lock_guard<std::mutex> lock(delayTimersMutex);
                   delayTimers[timerId] = timer;
               }
   
               timer->startTimer(delayMs);
           });
   
           return timerId;
       }
   
       void cancelDelay(int timerId)
       {
           if (timerId <= 0)
               return;
   
           juce::MessageManager::callAsync([timerId]() {
               std::lock_guard<std::mutex> lock(delayTimersMutex);
               auto it = delayTimers.find(timerId);
               if (it != delayTimers.end())
               {
                   it->second->stopTimer();
                   delete it->second;
                   delayTimers.erase(it);
               }
           });
       }
   
       //==========================================================================
       // Debouncing
       //==========================================================================
   
       void debounce(const juce::String& key, int delayMs, std::function<void()> callback)
       {
           if (key.isEmpty() || !callback || delayMs < 0)
               return;
   
           // Debounce logic must run on message thread for timer safety
           juce::MessageManager::callAsync([key, delayMs, callback = std::move(callback)]() {
               std::lock_guard<std::mutex> lock(debounceMutex);
   
               auto it = debounceTimers.find(key);
               if (it != debounceTimers.end())
               {
                   // Reset existing timer with new callback
                   it->second->stopTimer();
                   static_cast<CallbackTimer*>(it->second.get())->setCallback(callback);
                   it->second->startTimer(delayMs);
               }
               else
               {
                   // Create new timer
                   auto timer = std::make_unique<CallbackTimer>(callback);
                   timer->startTimer(delayMs);
                   debounceTimers[key] = std::move(timer);
               }
           });
       }
   
       void cancelDebounce(const juce::String& key)
       {
           if (key.isEmpty())
               return;
   
           juce::MessageManager::callAsync([key]() {
               std::lock_guard<std::mutex> lock(debounceMutex);
               auto it = debounceTimers.find(key);
               if (it != debounceTimers.end())
               {
                   it->second->stopTimer();
                   debounceTimers.erase(it);
               }
           });
       }
   
       void cancelAllDebounces()
       {
           juce::MessageManager::callAsync([]() {
               std::lock_guard<std::mutex> lock(debounceMutex);
               for (auto& pair : debounceTimers)
               {
                   pair.second->stopTimer();
               }
               debounceTimers.clear();
           });
       }
   
       //==========================================================================
       // Throttling
       //==========================================================================
   
       void throttle(const juce::String& key, int periodMs, std::function<void()> callback)
       {
           if (key.isEmpty() || !callback || periodMs < 0)
               return;
   
           juce::MessageManager::callAsync([key, periodMs, callback = std::move(callback)]() {
               std::lock_guard<std::mutex> lock(throttleMutex);
   
               auto& state = throttleStates[key];
               juce::int64 now = juce::Time::currentTimeMillis();
               juce::int64 timeSinceLastExecution = now - state.lastExecutionTime;
   
               if (timeSinceLastExecution >= periodMs)
               {
                   // Enough time has passed, execute immediately
                   state.lastExecutionTime = now;
                   callback();
               }
               else
               {
                   // Schedule for later (replace any pending callback)
                   int remainingMs = static_cast<int>(periodMs - timeSinceLastExecution);
   
                   if (state.pendingTimer)
                   {
                       state.pendingTimer->stopTimer();
                   }
                   else
                   {
                       state.pendingTimer = std::make_unique<CallbackTimer>(nullptr);
                   }
   
                   // Wrap callback to update lastExecutionTime when it fires
                   auto wrappedCallback = [key, callback]() {
                       std::lock_guard<std::mutex> innerLock(throttleMutex);
                       auto it = throttleStates.find(key);
                       if (it != throttleStates.end())
                       {
                           it->second.lastExecutionTime = juce::Time::currentTimeMillis();
                       }
                       callback();
                   };
   
                   static_cast<CallbackTimer*>(state.pendingTimer.get())->setCallback(wrappedCallback);
                   state.pendingTimer->startTimer(remainingMs);
               }
           });
       }
   
       void cancelThrottle(const juce::String& key)
       {
           if (key.isEmpty())
               return;
   
           juce::MessageManager::callAsync([key]() {
               std::lock_guard<std::mutex> lock(throttleMutex);
               auto it = throttleStates.find(key);
               if (it != throttleStates.end())
               {
                   if (it->second.pendingTimer)
                       it->second.pendingTimer->stopTimer();
                   throttleStates.erase(it);
               }
           });
       }
   
   }  // namespace Async
