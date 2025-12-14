.. _data-flow:

Data Flow Patterns
==================

This section documents complete data flow patterns through the reactive architecture,
from user actions to UI updates. All flows follow unidirectional data flow principles.

Unidirectional Data Flow
-------------------------

The Sidechain architecture follows strict unidirectional data flow:

.. code-block:: text

    User Action → Store Action → API Call → Response Handler →
    → setState() → Subscribers Notified → UI Updates

This ensures:

* Predictable state changes
* Easier debugging (trace back from UI to action)
* No circular dependencies
* Testable pure functions

Complete Flow Examples
----------------------

Feed Like Flow
~~~~~~~~~~~~~~

User clicks like button on a post. Here's the complete flow:

**1. User Action** (PostCard.cpp):

.. code-block:: cpp

    void PostCard::mouseUp(const juce::MouseEvent& event) {
        if (getLikeButtonBounds().contains(event.getPosition())) {
            // Don't call network directly - delegate to FeedStore
            if (feedStore) {
                feedStore->toggleLike(post.id);
            }
        }
    }

**2. Store Action** (FeedStore.cpp):

.. code-block:: cpp

    void FeedStore::toggleLike(const juce::String& postId) {
        // Find post in current feed
        auto& currentFeed = state.feeds[state.currentFeedType];
        auto it = std::find_if(currentFeed.posts.begin(), currentFeed.posts.end(),
            [&postId](const FeedPost& p) { return p.id == postId; });

        if (it == currentFeed.posts.end())
            return;

        bool newLikedState = !it->isLiked;

        // Optimistic update
        auto newState = state;
        auto& post = newState.feeds[state.currentFeedType].posts[std::distance(currentFeed.posts.begin(), it)];
        post.isLiked = newLikedState;
        post.likeCount += newLikedState ? 1 : -1;
        setState(newState);  // Notifies subscribers immediately

        // Background API call
        networkClient->toggleLike(postId, newLikedState, [this, postId, newLikedState](Outcome<juce::var> result) {
            if (result.isError()) {
                // Revert optimistic update on error
                auto revertState = state;
                auto& post = findPostById(revertState, postId);
                post.isLiked = !newLikedState;
                post.likeCount += newLikedState ? -1 : 1;
                setState(revertState);
            }
            // Success: optimistic update was correct, no change needed
        });
    }

**3. State Update Notification**:

.. code-block:: cpp

    void Store<FeedStoreState>::setState(const FeedStoreState& newState) {
        state = newState;

        // Notify all subscribers
        for (const auto& subscriber : subscribers) {
            subscriber(state);
        }
    }

**4. Component Subscription** (PostCard.cpp):

.. code-block:: cpp

    void PostCard::setFeedStore(FeedStore* store) {
        feedStore = store;

        storeUnsubscribe = feedStore->subscribe([this](const FeedStoreState& state) {
            // Find updated post
            const auto& currentFeed = state.getCurrentFeed();
            for (const auto& p : currentFeed.posts) {
                if (p.id == post.id) {
                    post = p;  // Update local post data
                    break;
                }
            }
            // ReactiveBoundComponent triggers repaint() automatically
        });
    }

**5. UI Update** (PostCard::paint):

.. code-block:: cpp

    void PostCard::paint(juce::Graphics& g) override {
        // Draw updated like state
        drawLikeButton(g, getLikeButtonBounds(), post.likeCount, post.isLiked);
    }

**Timeline**:

.. code-block:: text

    t=0ms:   User clicks like button
    t=1ms:   toggleLike() called
    t=2ms:   Optimistic setState() → subscribers notified
    t=3ms:   PostCard subscription fires
    t=4ms:   repaint() triggered
    t=16ms:  UI repaints with new like state (60fps)
    t=150ms: API response received
    t=151ms: Success (no change) OR revert optimistic update

Message Send Flow
~~~~~~~~~~~~~~~~~

User sends a chat message with real-time updates:

**1. User Types & Presses Send** (MessageThread.cpp):

.. code-block:: cpp

    void MessageThread::textEditorReturnKeyPressed(juce::TextEditor& editor) {
        juce::String text = editor.getText().trim();
        if (text.isEmpty())
            return;

        if (chatStore && !channelId.isEmpty()) {
            chatStore->sendMessage(channelId, text);
            editor.clear();
        }
    }

**2. Store Action** (ChatStore.cpp):

.. code-block:: cpp

    void ChatStore::sendMessage(const juce::String& channelId, const juce::String& text) {
        // Create optimistic message
        StreamChatClient::Message optimisticMsg;
        optimisticMsg.id = "temp-" + juce::Uuid().toString();
        optimisticMsg.text = text;
        optimisticMsg.userId = state.userId;
        optimisticMsg.createdAt = juce::Time::getCurrentTime().toISO8601(true);
        optimisticMsg.isOptimistic = true;

        // Add to state immediately
        auto newState = state;
        newState.channels[channelId].messages.push_back(optimisticMsg);
        setState(newState);  // UI shows message immediately

        // Send to server
        streamChatClient->sendMessage(channelId, text, [this, channelId, optimisticMsg](Outcome<Message> result) {
            if (result.isOk()) {
                // Replace optimistic message with server message
                auto updateState = state;
                auto& messages = updateState.channels[channelId].messages;

                auto it = std::find_if(messages.begin(), messages.end(),
                    [&optimisticMsg](const Message& m) { return m.id == optimisticMsg.id; });

                if (it != messages.end()) {
                    *it = result.getValue();  // Replace with server version
                    setState(updateState);
                }
            } else {
                // Remove failed optimistic message
                auto failState = state;
                auto& messages = failState.channels[channelId].messages;
                messages.erase(std::remove_if(messages.begin(), messages.end(),
                    [&optimisticMsg](const Message& m) { return m.id == optimisticMsg.id; }),
                    messages.end());
                failState.error = result.getError();
                setState(failState);
            }
        });
    }

**3. WebSocket Broadcast** (other users receive):

.. code-block:: cpp

    // WebSocket listener receives message event
    websocketClient->on("message.new", [this](const juce::var& data) {
        juce::String channelId = data["channel_id"];
        Message newMessage = parseMessage(data["message"]);

        auto newState = state;
        newState.channels[channelId].messages.push_back(newMessage);
        setState(newState);  // All connected users see the message
    });

**4. UI Updates** (MessageThread.cpp):

.. code-block:: cpp

    void MessageThread::paint(juce::Graphics& g) override {
        const auto* channel = chatStore->getState().getCurrentChannel();
        if (!channel)
            return;

        // Draw all messages (including optimistic ones)
        for (const auto& message : channel->messages) {
            drawMessageBubble(g, message);

            if (message.isOptimistic) {
                // Show sending indicator
                g.setOpacity(0.6f);
                g.drawText("Sending...", bounds, juce::Justification::bottomRight);
            }
        }
    }

Feed Refresh Flow
~~~~~~~~~~~~~~~~~

User pulls to refresh the feed:

**1. User Action** (PostsFeed.cpp):

.. code-block:: cpp

    void PostsFeed::mouseUp(const juce::MouseEvent& event) {
        if (getRefreshButtonBounds().contains(event.getPosition())) {
            if (feedStore) {
                feedStore->refreshCurrentFeed();
            }
        }
    }

**2. Store Action** (FeedStore.cpp):

.. code-block:: cpp

    void FeedStore::refreshCurrentFeed() {
        loadFeed(state.currentFeedType, true);  // refresh=true
    }

    void FeedStore::loadFeed(FeedType type, bool refresh) {
        // Set refreshing state
        auto newState = state;
        newState.feeds[type].isRefreshing = refresh;
        newState.feeds[type].isLoading = !refresh;
        setState(newState);  // Show loading/refreshing indicator

        // API call
        networkClient->getFeed(type, 50, "", [this, type](Outcome<FeedResponse> result) {
            auto resultState = state;

            if (result.isOk()) {
                const auto& response = result.getValue();
                resultState.feeds[type].posts = response.posts;
                resultState.feeds[type].cursor = response.cursor;
                resultState.feeds[type].hasMore = response.hasMore;
                resultState.feeds[type].error = "";
            } else {
                resultState.feeds[type].error = result.getError();
            }

            resultState.feeds[type].isLoading = false;
            resultState.feeds[type].isRefreshing = false;
            setState(resultState);  // Update with results
        });
    }

**3. UI Updates** (PostsFeed.cpp):

.. code-block:: cpp

    void PostsFeed::paint(juce::Graphics& g) override {
        const auto& state = feedStore->getState();
        const auto& currentFeed = state.getCurrentFeed();

        if (currentFeed.isRefreshing) {
            drawRefreshingIndicator(g);
        } else if (currentFeed.isLoading) {
            drawLoadingState(g);
        } else if (!currentFeed.error.isEmpty()) {
            drawErrorState(g, currentFeed.error);
        } else {
            drawPosts(g, currentFeed.posts);
        }
    }

Real-Time Update Flow
~~~~~~~~~~~~~~~~~~~~~

WebSocket receives like count update from another user:

**1. WebSocket Event** (RealtimeSync.cpp):

.. code-block:: cpp

    websocketClient->on("like.update", [&feedStore](const juce::var& data) {
        juce::String postId = data["post_id"];
        int newLikeCount = data["like_count"];

        feedStore.handleLikeCountUpdate(postId, newLikeCount);
    });

**2. Store Update** (FeedStore.cpp):

.. code-block:: cpp

    void FeedStore::handleLikeCountUpdate(const juce::String& postId, int likeCount) {
        auto newState = state;

        // Update across all feed types
        for (auto& [type, feed] : newState.feeds) {
            for (auto& post : feed.posts) {
                if (post.id == postId) {
                    post.likeCount = likeCount;
                    // Don't change isLiked - that's user-specific
                }
            }
        }

        setState(newState);  // All PostCards showing this post update
    }

**3. UI Updates** (all PostCards with matching post ID):

.. code-block:: cpp

    // PostCard subscription automatically triggered
    void PostCard::setFeedStore(FeedStore* store) {
        storeUnsubscribe = feedStore->subscribe([this](const FeedStoreState& state) {
            const auto& currentFeed = state.getCurrentFeed();
            for (const auto& p : currentFeed.posts) {
                if (p.id == post.id) {
                    post = p;  // Update like count
                    break;     // Triggers repaint
                }
            }
        });
    }

Error Handling Flow
~~~~~~~~~~~~~~~~~~~

Network error with retry logic:

**1. API Call Fails** (NetworkClient.cpp):

.. code-block:: cpp

    networkClient->get("/api/feed/timeline", [callback](Outcome<juce::var> result) {
        if (result.isError()) {
            callback(Outcome<FeedResponse>::error(result.getError()));
        }
    });

**2. Store Handles Error** (FeedStore.cpp):

.. code-block:: cpp

    void FeedStore::loadFeed(FeedType type, bool refresh) {
        // ... API call
        networkClient->getFeed(type, 50, "", [this, type](Outcome<FeedResponse> result) {
            auto newState = state;

            if (result.isError()) {
                newState.feeds[type].error = result.getError();
                newState.feeds[type].isLoading = false;
                setState(newState);  // Show error UI
            }
        });
    }

**3. UI Shows Error** (PostsFeed.cpp):

.. code-block:: cpp

    void PostsFeed::paint(juce::Graphics& g) override {
        const auto& currentFeed = feedStore->getState().getCurrentFeed();

        if (!currentFeed.error.isEmpty()) {
            // Error state component shows retry button
            drawErrorState(g, currentFeed.error);
        }
    }

**4. User Retries**:

.. code-block:: cpp

    void PostsFeed::mouseUp(const juce::MouseEvent& event) {
        if (getRetryButtonBounds().contains(event.getPosition())) {
            feedStore->refreshCurrentFeed();  // Try again
        }
    }

Performance Patterns
--------------------

Optimistic Updates
~~~~~~~~~~~~~~~~~~

Update UI immediately, revert on error:

.. code-block:: cpp

    void Store::performAction(const Params& params) {
        // 1. Optimistic update
        auto optimisticState = state;
        applyOptimisticChange(optimisticState, params);
        setState(optimisticState);  // UI updates immediately

        // 2. API call
        api->performAction(params, [this, params](Outcome<Result> result) {
            if (result.isError()) {
                // 3a. Revert on error
                auto revertState = state;
                revertOptimisticChange(revertState, params);
                setState(revertState);
            }
            // 3b. Success - optimistic update was correct
        });
    }

Pagination
~~~~~~~~~~

Load more data when scrolling:

.. code-block:: cpp

    void FeedStore::loadMore() {
        auto& currentFeed = state.feeds[state.currentFeedType];

        if (currentFeed.isLoading || !currentFeed.hasMore)
            return;

        auto newState = state;
        newState.feeds[state.currentFeedType].isLoading = true;
        setState(newState);

        networkClient->getFeed(state.currentFeedType, 50, currentFeed.cursor,
            [this](Outcome<FeedResponse> result) {
                if (result.isOk()) {
                    const auto& response = result.getValue();

                    auto appendState = state;
                    auto& feed = appendState.feeds[state.currentFeedType];

                    // Append new posts
                    feed.posts.insert(feed.posts.end(), response.posts.begin(), response.posts.end());
                    feed.cursor = response.cursor;
                    feed.hasMore = response.hasMore;
                    feed.isLoading = false;

                    setState(appendState);
                }
            });
    }

Debouncing
~~~~~~~~~~

Avoid excessive updates (e.g., search-as-you-type):

.. code-block:: cpp

    class SearchComponent : public ReactiveBoundComponent, public juce::Timer {
    public:
        SearchComponent() {
            bindProperty(searchQuery);

            searchQuery.observe([this](const juce::String& query) {
                // Debounce: wait 300ms before searching
                stopTimer();
                pendingQuery = query;
                startTimer(300);
            });
        }

        void timerCallback() override {
            stopTimer();
            if (searchStore) {
                searchStore->performSearch(pendingQuery);
            }
        }

    private:
        ObservableProperty<juce::String> searchQuery;
        juce::String pendingQuery;
        SearchStore* searchStore = nullptr;
    };

Caching
~~~~~~~

Cache data to avoid redundant fetches:

.. code-block:: cpp

    void FeedStore::switchFeedType(FeedType type) {
        if (state.currentFeedType == type)
            return;

        auto newState = state;
        newState.currentFeedType = type;

        // Check if feed is already loaded
        if (newState.feeds[type].posts.empty() && !newState.feeds[type].isLoading) {
            // Load if not cached
            loadFeed(type);
        } else {
            // Use cached data
            setState(newState);  // Instant switch to cached feed
        }
    }

Testing Patterns
----------------

Testing Stores
~~~~~~~~~~~~~~

Stores are pure functions and easy to test:

.. code-block:: cpp

    TEST_CASE("FeedStore toggleLike updates state") {
        FeedStore store;

        // Setup initial state
        FeedPost post;
        post.id = "post-123";
        post.isLiked = false;
        post.likeCount = 5;

        FeedStoreState initialState;
        initialState.feeds[FeedType::Timeline].posts.push_back(post);
        store.setState(initialState);

        // Subscribe to state changes
        bool stateChanged = false;
        store.subscribe([&stateChanged](const FeedStoreState& state) {
            stateChanged = true;
        });

        // Action
        store.toggleLike("post-123");

        // Assert
        REQUIRE(stateChanged);
        const auto& updatedPost = store.getState().feeds[FeedType::Timeline].posts[0];
        REQUIRE(updatedPost.isLiked == true);
        REQUIRE(updatedPost.likeCount == 6);
    }

Testing Components
~~~~~~~~~~~~~~~~~~

Components can be tested with mock stores:

.. code-block:: cpp

    TEST_CASE("PostCard displays like count") {
        // Create component
        PostCard card;

        // Setup mock store
        MockFeedStore mockStore;
        FeedPost testPost;
        testPost.id = "test-post";
        testPost.likeCount = 42;
        mockStore.setPost(testPost);

        card.setFeedStore(&mockStore);

        // Assert
        // (Would use image comparison or bounds checking in real test)
        REQUIRE(card.getPost().likeCount == 42);
    }

See Also
--------

* :doc:`stores` - Store implementation details
* :doc:`observables` - Observable patterns
* :doc:`reactive-components` - Component integration
* :doc:`threading` - Thread safety in data flows
