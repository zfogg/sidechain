.. _services:

Service Layer
=============

The Service layer provides business logic and domain operations, sitting between Stores
and Network clients. Services encapsulate reusable operations and keep stores focused
on state management.

Architecture Pattern
--------------------

.. code-block:: text

    Component → Store → Service → NetworkClient → API

Services provide:

* Business logic reusability
* Domain-specific operations
* Complex multi-step workflows
* Error handling and retries
* Data transformation (DTO ↔ Domain models)

Current State
-------------

**Note**: The service layer is currently being developed. Most business logic currently
resides in Stores. The following documents the planned service architecture.

Planned Services
----------------

FeedService
~~~~~~~~~~~

**Responsibility**: Feed-related business logic

**Location** (planned): ``plugin/src/services/FeedService.h``

**Operations**:

.. code-block:: cpp

    class FeedService {
    public:
        // Feed operations
        Outcome<FeedResponse> loadFeed(FeedType type, int limit, const juce::String& cursor);
        Outcome<void> toggleLike(const juce::String& postId, bool liked);
        Outcome<void> toggleSave(const juce::String& postId, bool saved);
        Outcome<void> createPost(const CreatePostRequest& request);

        // Complex operations
        Outcome<void> sharePost(const juce::String& postId, const juce::String& channelId);
        Outcome<RemixData> downloadRemixSource(const juce::String& postId);

    private:
        NetworkClient* networkClient;
        MultiTierCache* cache;
    };

**Benefits**:

* Reusable across multiple stores (FeedStore, ProfileStore, etc.)
* Centralized caching logic
* Consistent error handling

AudioService
~~~~~~~~~~~~

**Responsibility**: Audio capture, upload, and processing

**Location** (planned): ``plugin/src/services/AudioService.h``

**Operations**:

.. code-block:: cpp

    class AudioService {
    public:
        // Recording
        void startRecording();
        void stopRecording();
        Outcome<AudioBuffer<float>> getRecordedAudio();

        // Upload
        Outcome<UploadResult> uploadAudio(
            const AudioBuffer<float>& buffer,
            const AudioMetadata& metadata
        );

        // Download
        Outcome<AudioBuffer<float>> downloadAudio(const juce::String& url);

        // Analysis
        WaveformData generateWaveform(const AudioBuffer<float>& buffer);
        AudioFeatures extractFeatures(const AudioBuffer<float>& buffer);

    private:
        AudioCapture* audioCapture;
        NetworkClient* networkClient;
        FFTProcessor* fftProcessor;
    };

**Benefits**:

* Encapsulates audio processing complexity
* Handles format conversion (WAV → MP3)
* Progress callbacks for uploads/downloads

AuthService
~~~~~~~~~~~

**Responsibility**: Authentication and authorization

**Location** (planned): ``plugin/src/services/AuthService.h``

**Operations**:

.. code-block:: cpp

    class AuthService {
    public:
        // Authentication
        Outcome<LoginResult> login(const juce::String& email, const juce::String& password);
        Outcome<void> logout();
        Outcome<SignupResult> signup(const SignupRequest& request);

        // Token management
        Outcome<juce::String> refreshToken();
        bool isAuthenticated() const;

        // Secure storage
        void saveCredentials(const Credentials& creds);
        Outcome<Credentials> loadCredentials();

    private:
        NetworkClient* networkClient;
        SecureTokenStore* tokenStore;
    };

**Benefits**:

* Secure credential storage (Keychain/DPAPI)
* Automatic token refresh
* Session management

ChatService
~~~~~~~~~~~

**Responsibility**: Messaging and channel operations

**Location** (planned): ``plugin/src/services/ChatService.h``

**Operations**:

.. code-block:: cpp

    class ChatService {
    public:
        // Channels
        Outcome<std::vector<Channel>> loadChannels();
        Outcome<Channel> createChannel(const CreateChannelRequest& request);
        Outcome<void> leaveChannel(const juce::String& channelId);

        // Messages
        Outcome<std::vector<Message>> loadMessages(const juce::String& channelId, int limit);
        Outcome<Message> sendMessage(const juce::String& channelId, const juce::String& text);
        Outcome<void> deleteMessage(const juce::String& messageId);

        // Typing indicators
        void startTyping(const juce::String& channelId);
        void stopTyping(const juce::String& channelId);

    private:
        StreamChatClient* chatClient;
        MultiTierCache* cache;
    };

**Benefits**:

* Abstracts Stream Chat SDK complexity
* Handles optimistic updates
* Message drafts and persistence

Migration Pattern
-----------------

From Store to Service
~~~~~~~~~~~~~~~~~~~~~

Current pattern (business logic in store):

.. code-block:: cpp

    // FeedStore.cpp
    void FeedStore::toggleLike(const juce::String& postId) {
        // 1. Find post
        auto& currentFeed = state.feeds[state.currentFeedType];
        auto it = std::find_if(currentFeed.posts.begin(), currentFeed.posts.end(),
            [&postId](const FeedPost& p) { return p.id == postId; });

        if (it == currentFeed.posts.end())
            return;

        bool newLikedState = !it->isLiked;

        // 2. Optimistic update
        auto newState = state;
        auto& post = newState.feeds[state.currentFeedType].posts[...];
        post.isLiked = newLikedState;
        post.likeCount += newLikedState ? 1 : -1;
        setState(newState);

        // 3. Network call
        networkClient->toggleLike(postId, newLikedState, [this, postId](auto result) {
            if (result.isError()) {
                // Revert...
            }
        });
    }

Planned pattern (business logic in service):

.. code-block:: cpp

    // FeedService.cpp
    Outcome<LikeResult> FeedService::toggleLike(const juce::String& postId, bool liked) {
        // Business logic here
        return networkClient->toggleLike(postId, liked)
            .map([](const juce::var& response) {
                return LikeResult{
                    postId: response["post_id"],
                    newLikeCount: response["like_count"]
                };
            });
    }

    // FeedStore.cpp
    void FeedStore::toggleLike(const juce::String& postId) {
        // 1. Optimistic update (state management only)
        auto& post = findPostById(state, postId);
        bool newLikedState = !post.isLiked;

        auto newState = state;
        auto& optimisticPost = findPostById(newState, postId);
        optimisticPost.isLiked = newLikedState;
        optimisticPost.likeCount += newLikedState ? 1 : -1;
        setState(newState);

        // 2. Delegate to service
        feedService->toggleLike(postId, newLikedState)
            .onSuccess([this](const LikeResult& result) {
                // Update with server result
                auto finalState = state;
                auto& post = findPostById(finalState, result.postId);
                post.likeCount = result.newLikeCount;
                setState(finalState);
            })
            .onError([this, postId, newLikedState](const juce::String& error) {
                // Revert optimistic update
                auto revertState = state;
                auto& post = findPostById(revertState, postId);
                post.isLiked = !newLikedState;
                post.likeCount += newLikedState ? -1 : 1;
                setState(revertState);
            });
    }

Benefits of Service Layer
--------------------------

1. Reusability
~~~~~~~~~~~~~~

Services can be shared across multiple stores:

.. code-block:: cpp

    // FeedStore uses FeedService
    feedStore->toggleLike(postId);  // Calls FeedService::toggleLike

    // ProfileStore also uses FeedService
    profileStore->likeUserPost(postId);  // Calls same FeedService::toggleLike

2. Testability
~~~~~~~~~~~~~~

Services are easier to test in isolation:

.. code-block:: cpp

    TEST_CASE("FeedService toggleLike updates backend") {
        // Mock network client
        MockNetworkClient mockClient;
        FeedService service(&mockClient);

        // Test service logic
        auto result = service.toggleLike("post-123", true);

        REQUIRE(result.isOk());
        REQUIRE(mockClient.lastRequestPath == "/api/posts/post-123/like");
    }

3. Separation of Concerns
~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Stores**: State management only
* **Services**: Business logic and workflows
* **Network**: HTTP/WebSocket communication
* **Components**: UI rendering and events

4. Complex Workflows
~~~~~~~~~~~~~~~~~~~~

Services handle multi-step operations:

.. code-block:: cpp

    Outcome<ShareResult> FeedService::sharePostToMessage(
        const juce::String& postId,
        const juce::String& channelId
    ) {
        // 1. Get post details
        return loadPost(postId)
            .flatMap([this, channelId](const FeedPost& post) {
                // 2. Create share message
                return createShareMessage(post, channelId);
            })
            .flatMap([this](const Message& message) {
                // 3. Send message
                return chatService->sendMessage(message);
            })
            .map([](const Message& sent) {
                // 4. Return result
                return ShareResult{sent.id, sent.channelId};
            });
    }

Service Implementation Patterns
--------------------------------

Outcome<T> Return Type
~~~~~~~~~~~~~~~~~~~~~~~

Services use ``Outcome<T>`` for error handling:

.. code-block:: cpp

    Outcome<FeedResponse> FeedService::loadFeed(FeedType type) {
        return networkClient->getFeed(type)
            .map([](const juce::var& json) {
                return parseFeedResponse(json);
            })
            .mapError([](const juce::String& error) {
                return "Failed to load feed: " + error;
            });
    }

Async Operations
~~~~~~~~~~~~~~~~

Services handle async operations with callbacks:

.. code-block:: cpp

    void FeedService::loadFeedAsync(
        FeedType type,
        std::function<void(Outcome<FeedResponse>)> callback
    ) {
        networkClient->getFeed(type, [callback](Outcome<juce::var> result) {
            if (result.isOk()) {
                callback(Outcome<FeedResponse>::ok(
                    parseFeedResponse(result.getValue())
                ));
            } else {
                callback(Outcome<FeedResponse>::error(result.getError()));
            }
        });
    }

Caching
~~~~~~~

Services integrate with caching layer:

.. code-block:: cpp

    Outcome<FeedResponse> FeedService::loadFeed(FeedType type) {
        juce::String cacheKey = "feed:" + feedTypeToString(type);

        // Check cache first
        if (auto cached = cache->get<FeedResponse>(cacheKey)) {
            return Outcome<FeedResponse>::ok(cached.value());
        }

        // Fetch from network
        return networkClient->getFeed(type)
            .map([this, cacheKey](const juce::var& json) {
                auto response = parseFeedResponse(json);

                // Cache response
                cache->set(cacheKey, response, 300);  // 5 min TTL

                return response;
            });
    }

Retry Logic
~~~~~~~~~~~

Services implement retry strategies:

.. code-block:: cpp

    Outcome<T> FeedService::retryableOperation(
        std::function<Outcome<T>()> operation,
        int maxRetries = 3
    ) {
        int attempts = 0;

        while (attempts < maxRetries) {
            auto result = operation();

            if (result.isOk()) {
                return result;
            }

            // Exponential backoff
            int delayMs = std::pow(2, attempts) * 100;
            juce::Thread::sleep(delayMs);

            attempts++;
        }

        return Outcome<T>::error("Operation failed after " + juce::String(maxRetries) + " retries");
    }

Data Transformation
~~~~~~~~~~~~~~~~~~~

Services handle DTO ↔ Domain model conversion:

.. code-block:: cpp

    // DTO from API
    struct FeedPostDTO {
        juce::var json;  // Raw JSON from API
    };

    // Domain model
    struct FeedPost {
        juce::String id;
        juce::String userId;
        juce::String audioUrl;
        // ... fields
    };

    // Service transforms
    FeedPost FeedService::parseFeedPost(const FeedPostDTO& dto) {
        FeedPost post;
        post.id = dto.json["id"];
        post.userId = dto.json["user_id"];
        post.audioUrl = dto.json["audio_url"];
        // ... parse all fields
        return post;
    }

Testing Services
----------------

Unit Tests
~~~~~~~~~~

Mock network clients for isolated testing:

.. code-block:: cpp

    class MockNetworkClient : public NetworkClient {
    public:
        Outcome<juce::var> getFeed(FeedType type) override {
            lastFeedType = type;
            return mockResponse;
        }

        FeedType lastFeedType;
        Outcome<juce::var> mockResponse;
    };

    TEST_CASE("FeedService loads timeline feed") {
        MockNetworkClient mockClient;
        mockClient.mockResponse = Outcome<juce::var>::ok(createMockFeedJson());

        FeedService service(&mockClient);

        auto result = service.loadFeed(FeedType::Timeline);

        REQUIRE(result.isOk());
        REQUIRE(mockClient.lastFeedType == FeedType::Timeline);
        REQUIRE(result.getValue().posts.size() == 10);
    }

Integration Tests
~~~~~~~~~~~~~~~~~

Test service with real network (staging environment):

.. code-block:: cpp

    TEST_CASE("FeedService integration test", "[integration]") {
        NetworkClient realClient("https://staging.api.sidechain.live");
        FeedService service(&realClient);

        auto result = service.loadFeed(FeedType::Timeline);

        REQUIRE(result.isOk());
        REQUIRE(!result.getValue().posts.empty());
    }

Best Practices
--------------

1. **Single Responsibility**

   Each service handles one domain area:

   .. code-block:: cpp

       // ✅ Good: Focused service
       class FeedService {
           Outcome<FeedResponse> loadFeed(...);
           Outcome<void> createPost(...);
           Outcome<void> deletePost(...);
       };

       // ❌ Bad: Mixed responsibilities
       class AppService {
           Outcome<FeedResponse> loadFeed(...);
           Outcome<LoginResult> login(...);  // Should be AuthService
           Outcome<Message> sendMessage(...);  // Should be ChatService
       };

2. **Dependency Injection**

   Inject dependencies via constructor:

   .. code-block:: cpp

       class FeedService {
       public:
           FeedService(NetworkClient* network, MultiTierCache* cache)
               : networkClient(network), cache(cache) {}

       private:
           NetworkClient* networkClient;
           MultiTierCache* cache;
       };

3. **Error Handling**

   Use Outcome<T> for consistent error handling:

   .. code-block:: cpp

       Outcome<T> operation() {
           if (error)
               return Outcome<T>::error("Error message");

           return Outcome<T>::ok(result);
       }

4. **Async by Default**

   Provide async versions for all network operations:

   .. code-block:: cpp

       // Async version (preferred)
       void loadFeedAsync(FeedType type, std::function<void(Outcome<FeedResponse>)> callback);

       // Sync version (for testing)
       Outcome<FeedResponse> loadFeed(FeedType type);

5. **Immutable Operations**

   Services should not mutate state - return new data:

   .. code-block:: cpp

       // ✅ Good: Returns new data
       Outcome<FeedPost> updatePost(const juce::String& postId, const juce::String& newText) {
           return networkClient->updatePost(postId, newText)
               .map([](const juce::var& json) {
                   return parseFeedPost(json);
               });
       }

       // ❌ Bad: Mutates state
       void updatePost(FeedPost& post, const juce::String& newText) {
           post.text = newText;  // Direct mutation
       }

Migration Timeline
------------------

The service layer is being introduced gradually:

**Phase 1** (Current): Business logic in stores
  * Stores handle everything
  * Direct NetworkClient usage

**Phase 2** (Planned Q1 2025): Extract core services
  * FeedService for feed operations
  * AuthService for authentication
  * ChatService for messaging

**Phase 3** (Planned Q2 2025): Complete migration
  * All business logic in services
  * Stores only manage state
  * Full test coverage

See Also
--------

* :doc:`stores` - Store pattern that services support
* :doc:`data-flow` - How services fit in data flow
* :doc:`threading` - Service thread safety
