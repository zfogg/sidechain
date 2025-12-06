
.. _program_listing_file_plugin_source_util_Result.h:

Program Listing for File Result.h
=================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_Result.h>` (``plugin/source/util/Result.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <functional>
   #include <optional>
   #include "Log.h"
   
   //==============================================================================
   template<typename T>
   class Outcome
   {
   public:
       //==========================================================================
       // Factory methods
   
       static Outcome<T> ok(const T& value)
       {
           return Outcome<T>(value);
       }
   
       static Outcome<T> ok(T&& value)
       {
           return Outcome<T>(std::move(value));
       }
   
       static Outcome<T> error(const juce::String& message)
       {
           return Outcome<T>(message, true);
       }
   
       //==========================================================================
       // State checking
   
       bool isOk() const noexcept { return hasValue; }
   
       bool isError() const noexcept { return !hasValue; }
   
       explicit operator bool() const noexcept { return hasValue; }
   
       //==========================================================================
       // Value access
   
       const T& getValue() const
       {
           if (!hasValue)
           {
               Log::error("Outcome::getValue() called on error result: " + errorMessage);
               static T defaultValue{};
               return defaultValue;
           }
           return value.value();
       }
   
       T& getValue()
       {
           if (!hasValue)
           {
               Log::error("Outcome::getValue() called on error result: " + errorMessage);
               static T defaultValue{};
               return defaultValue;
           }
           return value.value();
       }
   
       T getValueOr(const T& defaultValue) const
       {
           return hasValue ? value.value() : defaultValue;
       }
   
       T getValueOrElse(std::function<T()> defaultFn) const
       {
           return hasValue ? value.value() : defaultFn();
       }
   
       const juce::String& getError() const noexcept
       {
           return errorMessage;
       }
   
       //==========================================================================
       // Monadic operations
   
       template<typename U>
       Outcome<U> map(std::function<U(const T&)> fn) const
       {
           if (hasValue)
               return Outcome<U>::ok(fn(value.value()));
           return Outcome<U>::error(errorMessage);
       }
   
       template<typename U>
       Outcome<U> flatMap(std::function<Outcome<U>(const T&)> fn) const
       {
           if (hasValue)
               return fn(value.value());
           return Outcome<U>::error(errorMessage);
       }
   
       const Outcome<T>& onSuccess(std::function<void(const T&)> fn) const
       {
           if (hasValue)
               fn(value.value());
           return *this;
       }
   
       const Outcome<T>& onError(std::function<void(const juce::String&)> fn) const
       {
           if (!hasValue)
               fn(errorMessage);
           return *this;
       }
   
       const Outcome<T>& logIfError(const juce::String& context = "") const
       {
           if (!hasValue)
           {
               if (context.isNotEmpty())
                   Log::error(context + ": " + errorMessage);
               else
                   Log::error(errorMessage);
           }
           return *this;
       }
   
       Outcome<T> mapError(std::function<juce::String(const juce::String&)> fn) const
       {
           if (hasValue)
               return *this;
           return Outcome<T>::error(fn(errorMessage));
       }
   
       Outcome<T> recover(std::function<T(const juce::String&)> fn) const
       {
           if (hasValue)
               return *this;
           return Outcome<T>::ok(fn(errorMessage));
       }
   
   private:
       //==========================================================================
       // Private constructors
   
       // Ok constructor
       explicit Outcome(const T& val)
           : value(val), hasValue(true) {}
   
       explicit Outcome(T&& val)
           : value(std::move(val)), hasValue(true) {}
   
       // Error constructor
       Outcome(const juce::String& error, bool)
           : errorMessage(error), hasValue(false) {}
   
       //==========================================================================
       // Members
   
       std::optional<T> value;
       juce::String errorMessage;
       bool hasValue = false;
   };
   
   //==============================================================================
   template<>
   class Outcome<void>
   {
   public:
       //==========================================================================
       // Factory methods
   
       static Outcome<void> ok()
       {
           return Outcome<void>(true);
       }
   
       static Outcome<void> error(const juce::String& message)
       {
           return Outcome<void>(message);
       }
   
       //==========================================================================
       // State checking
   
       bool isOk() const noexcept { return success; }
       bool isError() const noexcept { return !success; }
       explicit operator bool() const noexcept { return success; }
   
       //==========================================================================
       // Error access
   
       const juce::String& getError() const noexcept
       {
           return errorMessage;
       }
   
       //==========================================================================
       // Chaining
   
       const Outcome<void>& onSuccess(std::function<void()> fn) const
       {
           if (success)
               fn();
           return *this;
       }
   
       const Outcome<void>& onError(std::function<void(const juce::String&)> fn) const
       {
           if (!success)
               fn(errorMessage);
           return *this;
       }
   
       const Outcome<void>& logIfError(const juce::String& context = "") const
       {
           if (!success)
           {
               if (context.isNotEmpty())
                   Log::error(context + ": " + errorMessage);
               else
                   Log::error(errorMessage);
           }
           return *this;
       }
   
       template<typename U>
       Outcome<U> then(std::function<Outcome<U>()> fn) const
       {
           if (success)
               return fn();
           return Outcome<U>::error(errorMessage);
       }
   
   private:
       explicit Outcome(bool ok) : success(ok) {}
       Outcome(const juce::String& error) : errorMessage(error), success(false) {}
   
       juce::String errorMessage;
       bool success = false;
   };
   
   //==============================================================================
   // Type aliases for common result types
   
   using VoidOutcome = Outcome<void>;
   using StringOutcome = Outcome<juce::String>;
   using IntOutcome = Outcome<int>;
   using BoolOutcome = Outcome<bool>;
   using JsonOutcome = Outcome<juce::var>;
