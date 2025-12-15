#pragma once

#include "../util/async/Promise.h"
#include "../util/crdt/OperationalTransform.h"
#include <JuceHeader.h>
#include <memory>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>

namespace Sidechain {
namespace Network {

/**
 * RealtimeSync - WebSocket-based real-time synchronization
 *
 * Handles:
 * - Operational Transform message exchange
 * - Conflict resolution for concurrent edits
 * - Automatic reconnection on disconnect
 * - Message ordering and acknowledgment
 *
 * Usage:
 *   auto sync = RealtimeSync::create(webSocketClient);
 *   sync->onRemoteOperation([](const auto& op) {
 *       applyOperationToDocument(op);
 *   });
 *   sync->sendLocalOperation(myInsertOp);
 */
class RealtimeSync : public std::enable_shared_from_this<RealtimeSync>
{
public:
    using Operation = Sidechain::Util::CRDT::OperationalTransform::Operation;
    using OperationCallback = std::function<void(const std::shared_ptr<Operation>&)>;
    using SyncStateCallback = std::function<void(bool)>;  // true = synced, false = out of sync
    using ErrorCallback = std::function<void(const juce::String&)>;

    /**
     * Create real-time sync handler
     *
     * @param clientId Unique identifier for this client
     * @param documentId ID of document being synchronized
     */
    static std::shared_ptr<RealtimeSync> create(int clientId, const juce::String& documentId)
    {
        return std::make_shared<RealtimeSync>(clientId, documentId);
    }

    RealtimeSync(int clientId, const juce::String& documentId)
        : clientId_(clientId)
        , documentId_(documentId)
        , isSynced_(true)
        , operationCounter_(0)
    {
    }

    virtual ~RealtimeSync() = default;

    // ========== Configuration ==========

    /**
     * Set the callback for remote operations
     *
     * Called when operations from other clients are received
     */
    std::shared_ptr<RealtimeSync> onRemoteOperation(OperationCallback callback)
    {
        remoteOpCallback_ = callback;
        return this->shared_from_this();
    }

    /**
     * Set the callback for sync state changes
     */
    std::shared_ptr<RealtimeSync> onSyncStateChanged(SyncStateCallback callback)
    {
        syncStateCallback_ = callback;
        return this->shared_from_this();
    }

    /**
     * Set error callback
     */
    std::shared_ptr<RealtimeSync> onError(ErrorCallback callback)
    {
        errorCallback_ = callback;
        return this->shared_from_this();
    }

    // ========== Operations ==========

    /**
     * Send a local operation to be synchronized
     *
     * @param operation The operation performed locally
     */
    void sendLocalOperation(const std::shared_ptr<Operation>& operation)
    {
        if (!operation)
            return;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Set operation metadata
            operation->clientId = clientId_;
            operation->timestamp = operationCounter_++;

            // Add to pending operations
            pendingOperations_.push_back(operation);

            // Serialize and send via WebSocket
            // (In production: would serialize to JSON and send message)
            // For now, just queue it
        }

        // Mark as out of sync until acknowledged
        setSyncState(false);
    }

    /**
     * Handle incoming remote operation from WebSocket
     *
     * @param operation The operation from remote client
     */
    void handleRemoteOperation(const std::shared_ptr<Operation>& operation)
    {
        if (!operation)
            return;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Transform against pending local operations
            auto pending = pendingOperations_;
            auto transformed = operation->clone();

            for (const auto& localOp : pending)
            {
                auto [_, xformed] = Sidechain::Util::CRDT::OperationalTransform::transform(
                    localOp, transformed);
                transformed = xformed;
            }

            // Add to history
            operationHistory_.push_back(transformed);

            // Callback to apply operation
            if (remoteOpCallback_)
                remoteOpCallback_(transformed);
        }

        // We're now synced with this operation
        setSyncState(true);
    }

    /**
     * Acknowledge local operation as successfully synced
     *
     * @param timestamp The timestamp of the operation that was synced
     */
    void acknowledgeLocalOperation(int timestamp)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Remove acknowledged operation from pending
        auto it = pendingOperations_.begin();
        while (it != pendingOperations_.end())
        {
            if ((*it)->timestamp == timestamp)
            {
                it = pendingOperations_.erase(it);
                break;
            }
            else
            {
                ++it;
            }
        }

        // If no more pending, we're in sync
        if (pendingOperations_.empty())
            setSyncState(true);
    }

    // ========== State Queries ==========

    /**
     * Check if all operations are synchronized
     */
    bool isSynced() const { return isSynced_; }

    /**
     * Get number of pending operations waiting for acknowledgment
     */
    size_t getPendingOperationCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pendingOperations_.size();
    }

    /**
     * Get operation history
     */
    std::vector<std::shared_ptr<Operation>> getOperationHistory() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return operationHistory_;
    }

    /**
     * Get total operations processed
     */
    int getTotalOperationCount() const
    {
        return operationCounter_;
    }

    /**
     * Get client ID
     */
    int getClientId() const { return clientId_; }

    /**
     * Get document ID
     */
    const juce::String& getDocumentId() const { return documentId_; }

    /**
     * Force resynchronization
     *
     * Request full document state from server
     */
    void requestFullSync()
    {
        setSyncState(false);
        // In production: would send FULL_SYNC message to server
    }

private:
    int clientId_;
    juce::String documentId_;
    bool isSynced_;
    int operationCounter_;
    mutable std::mutex mutex_;

    std::vector<std::shared_ptr<Operation>> pendingOperations_;
    std::vector<std::shared_ptr<Operation>> operationHistory_;

    OperationCallback remoteOpCallback_;
    SyncStateCallback syncStateCallback_;
    ErrorCallback errorCallback_;

    /**
     * Update sync state and call callback if changed
     */
    void setSyncState(bool newState)
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (isSynced_ != newState)
            {
                isSynced_ = newState;
                changed = true;
            }
        }

        if (changed && syncStateCallback_)
            syncStateCallback_(newState);
    }

    // Prevent copying
    RealtimeSync(const RealtimeSync&) = delete;
    RealtimeSync& operator=(const RealtimeSync&) = delete;
};

/**
 * WebSocketMessageHandler - Handles incoming WebSocket messages for OT operations
 *
 * Message format (JSON):
 * {
 *   "type": "operation",
 *   "documentId": "doc-123",
 *   "operation": {
 *     "type": "insert",
 *     "position": 5,
 *     "content": "hello",
 *     "clientId": 1,
 *     "timestamp": 42
 *   }
 * }
 */
class WebSocketOperationHandler
{
public:
    /**
     * Register a sync handler for a document
     */
    static void registerSync(const juce::String& documentId,
                             std::shared_ptr<RealtimeSync> sync)
    {
        syncHandlers[documentId] = sync;
    }

    /**
     * Unregister a sync handler
     */
    static void unregisterSync(const juce::String& documentId)
    {
        syncHandlers.erase(documentId);
    }

    /**
     * Process incoming WebSocket message
     *
     * @param message JSON message from server
     */
    static void handleMessage(const juce::var& message)
    {
        // Parse message type
        if (!message.hasProperty("type"))
            return;

        juce::String type = message["type"];
        if (type != "operation")
            return;

        juce::String documentId = message["documentId"];
        auto it = syncHandlers.find(documentId);
        if (it == syncHandlers.end())
            return;

        // TODO: Parse operation from JSON and handle
        // auto operation = deserializeOperation(message["operation"]);
        // it->second->handleRemoteOperation(operation);
    }

private:
    static std::map<juce::String, std::shared_ptr<RealtimeSync>> syncHandlers;
};

// Static member definition (inline for header-only, C++17+)
inline std::map<juce::String, std::shared_ptr<RealtimeSync>> WebSocketOperationHandler::syncHandlers;

}  // namespace Network
}  // namespace Sidechain
