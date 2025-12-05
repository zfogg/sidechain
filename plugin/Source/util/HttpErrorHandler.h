#pragma once

#include <JuceHeader.h>
#include <functional>
#include <mutex>
#include <deque>

/**
 * HttpErrorHandler - Centralized HTTP error tracking and debug popup system
 *
 * In debug builds, shows a popup for every HTTP error so developers can
 * immediately see what's failing. In release builds, errors are just logged.
 */
class HttpErrorHandler
{
public:
    struct HttpError
    {
        juce::String endpoint;
        juce::String method;
        int statusCode = 0;
        juce::String errorMessage;
        juce::String responseBody;
        juce::Time timestamp;

        juce::String getSummary() const
        {
            return method + " " + endpoint + " -> " + juce::String(statusCode) + ": " + errorMessage;
        }
    };

    // Singleton access
    static HttpErrorHandler& getInstance()
    {
        static HttpErrorHandler instance;
        return instance;
    }

    // Report an HTTP error
    void reportError(const juce::String& endpoint,
                     const juce::String& method,
                     int statusCode,
                     const juce::String& errorMessage,
                     const juce::String& responseBody = "")
    {
        HttpError error;
        error.endpoint = endpoint;
        error.method = method;
        error.statusCode = statusCode;
        error.errorMessage = errorMessage;
        error.responseBody = responseBody;
        error.timestamp = juce::Time::getCurrentTime();

        // Store in history
        {
            std::lock_guard<std::mutex> lock(errorMutex);
            errorHistory.push_back(error);

            // Keep last 100 errors
            while (errorHistory.size() > 100)
                errorHistory.pop_front();
        }

        // Log to debug output
        DBG("HTTP ERROR: " + error.getSummary());
        if (responseBody.isNotEmpty())
            DBG("  Response: " + responseBody.substring(0, 500));

#if JUCE_DEBUG || DEBUG || _DEBUG
        // In debug mode, show popup on message thread
        juce::MessageManager::callAsync([error]() {
            showErrorPopup(error);
        });
#endif

        // Notify listeners
        if (errorCallback)
        {
            juce::MessageManager::callAsync([this, error]() {
                if (errorCallback)
                    errorCallback(error);
            });
        }
    }

    // Set callback for error notifications (useful for UI indicators)
    void setErrorCallback(std::function<void(const HttpError&)> callback)
    {
        errorCallback = callback;
    }

    // Get recent error history
    std::vector<HttpError> getRecentErrors(int count = 10)
    {
        std::lock_guard<std::mutex> lock(errorMutex);
        std::vector<HttpError> result;

        int startIdx = juce::jmax(0, static_cast<int>(errorHistory.size()) - count);
        for (size_t i = startIdx; i < errorHistory.size(); ++i)
            result.push_back(errorHistory[i]);

        return result;
    }

    // Clear error history
    void clearHistory()
    {
        std::lock_guard<std::mutex> lock(errorMutex);
        errorHistory.clear();
    }

private:
    HttpErrorHandler() = default;
    ~HttpErrorHandler() = default;

    HttpErrorHandler(const HttpErrorHandler&) = delete;
    HttpErrorHandler& operator=(const HttpErrorHandler&) = delete;

    std::mutex errorMutex;
    std::deque<HttpError> errorHistory;
    std::function<void(const HttpError&)> errorCallback;

    // Show debug popup for HTTP errors
    static void showErrorPopup(const HttpError& error)
    {
#if JUCE_DEBUG || DEBUG || _DEBUG
        // Format timestamp
        juce::String timeStr = error.timestamp.formatted("%H:%M:%S");

        // Build message
        juce::String message;
        message << "Time: " << timeStr << "\n\n";
        message << "Request: " << error.method << " " << error.endpoint << "\n\n";
        message << "Status: " << error.statusCode << "\n\n";
        message << "Error: " << error.errorMessage;

        if (error.responseBody.isNotEmpty())
        {
            message << "\n\n";
            message << "Response:\n" << error.responseBody.substring(0, 500);
            if (error.responseBody.length() > 500)
                message << "...";
        }

        // Show alert dialog
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "HTTP Error (Debug)",
            message,
            "OK"
        );
#else
        juce::ignoreUnused(error);
#endif
    }
};
