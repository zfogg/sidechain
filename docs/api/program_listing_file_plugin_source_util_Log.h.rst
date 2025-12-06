
.. _program_listing_file_plugin_source_util_Log.h:

Program Listing for File Log.h
==============================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_Log.h>` (``plugin/source/util/Log.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   
   //==============================================================================
   namespace Log
   {
       //==========================================================================
       // Log levels
       enum class Level
       {
           Debug,
           Info,
           Warn,
           Error
       };
   
       //==========================================================================
       // Core logging functions
       void debug(const juce::String& message);
       void info(const juce::String& message);
       void warn(const juce::String& message);
       void error(const juce::String& message);
   
       // Log with explicit level
       void log(Level level, const juce::String& message);
   
       //==========================================================================
       // Formatted logging (printf-style via juce::String::formatted)
       template<typename... Args>
       void debugf(const char* format, Args... args)
       {
           debug(juce::String::formatted(format, args...));
       }
   
       template<typename... Args>
       void infof(const char* format, Args... args)
       {
           info(juce::String::formatted(format, args...));
       }
   
       template<typename... Args>
       void warnf(const char* format, Args... args)
       {
           warn(juce::String::formatted(format, args...));
       }
   
       template<typename... Args>
       void errorf(const char* format, Args... args)
       {
           error(juce::String::formatted(format, args...));
       }
   
       //==========================================================================
       // Configuration
   
       // Set minimum log level (messages below this level are ignored)
       void setMinLevel(Level level);
       Level getMinLevel();
   
       // Enable/disable file logging
       void setFileLoggingEnabled(bool enabled);
       bool isFileLoggingEnabled();
   
       // Enable/disable console logging
       void setConsoleLoggingEnabled(bool enabled);
       bool isConsoleLoggingEnabled();
   
       // Get the current log file path (empty if file logging failed)
       juce::String getLogFilePath();
   
       // Flush any buffered log entries to file
       void flush();
   
       //==========================================================================
       // Utility
   
       // Get string representation of log level
       const char* levelToString(Level level);
   
   }  // namespace Log
