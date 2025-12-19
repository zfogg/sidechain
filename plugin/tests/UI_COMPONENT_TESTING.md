# UI Component State Testing with Catch2

## Overview

This guide shows how to test JUCE plugin UI component state changes using Catch2. The key pattern is:

1. **Create** a component with test data
2. **Trigger** a callback/event (e.g., button click)
3. **Verify** the UI state changed as expected

## Example: Testing PostCard Like Button

### What We're Testing

When a user clicks the like button on a post:
- The `onLikeClicked` callback fires
- The network request succeeds
- The UI state updates (like count increases, like button highlights)

### The Test

```cpp
TEST_CASE_METHOD(PostCardTestFixture,
                 "PostCard like button state changes on callback", 
                 "[PostCard][Like][Callback]") {
    PostCard card;
    FeedPost testPost = createTestPost("post-like");
    testPost.isLiked = false;
    testPost.likeCount = 5;
    card.setPost(testPost);
    
    SECTION("UI updates after successful like (network callback)") {
        // Initial state: NOT liked, 5 likes
        REQUIRE(card.getPost().isLiked == false);
        REQUIRE(card.getPost().likeCount == 5);
        
        // Simulate user clicking like button
        card.onLikeClicked = [&](const FeedPost& post) {
            // Simulate network success - update post data
            FeedPost updatedPost = post;
            updatedPost.isLiked = true;
            updatedPost.likeCount = 6;
            card.setPost(updatedPost);
        };
        
        card.likeButtonClicked();
        
        // After callback, UI should reflect the like
        REQUIRE(card.getPost().isLiked == true);
        REQUIRE(card.getPost().likeCount == 6);
    }
}
```

### How It Works

1. **Setup**: Create a `PostCard` with test post data
2. **Define callback**: Set `onLikeClicked` to simulate network success
3. **Trigger**: Call `card.likeButtonClicked()` to simulate user click
4. **Assert**: Verify the component's internal state changed

## Running the Tests

### Run all UI component tests:
```bash
make test-plugin-unit
```

### Run only PostCard tests:
```bash
cd plugin/build && ./SidechainTests "[PostCard]" --reporter compact
```

### Run specific test variant:
```bash
cd plugin/build && ./SidechainTests "[PostCard][Like]" --reporter compact
```

### Run with verbose output:
```bash
cd plugin/build && ./SidechainTests "[PostCard]" --reporter console
```

### Generate JUnit XML for CI:
```bash
cd plugin/build && ./SidechainTests "[PostCard]" --reporter junit --out test-results/postcard.xml
```

## Component State Testing Pattern

### For Any JUCE Component

**Step 1: Expose component state for testing**

In your component header, make test-relevant properties accessible:

```cpp
class MyComponent : public juce::Component {
public:
    // Public for testing
    bool isLiked { false };
    int likeCount { 0 };
    
    // Or use getters
    bool getIsLiked() const { return isLiked; }
    int getLikeCount() const { return likeCount; }
    
    // Callbacks that tests can hook into
    std::function<void(const Data&)> onLikeClicked;
    
    // Method tests can call to simulate user interaction
    void likeButtonClicked() {
        if (onLikeClicked) {
            onLikeClicked(currentData);
        }
    }
};
```

**Step 2: Create a test fixture**

```cpp
class MyComponentTestFixture {
public:
    MyComponentTestFixture() {
        if (!juce::MessageManager::getInstanceWithoutCreating()) {
            juce::MessageManager::getInstance();
        }
    }
    
    Data createTestData(const juce::String& id = "test-1") {
        // ... create test data
    }
};
```

**Step 3: Write the test**

```cpp
TEST_CASE_METHOD(MyComponentTestFixture, "Test case name", "[Component]") {
    MyComponent comp;
    Data testData = createTestData();
    comp.setData(testData);
    
    SECTION("State changes on interaction") {
        // Initial state
        REQUIRE(comp.getIsLiked() == false);
        
        // Simulate interaction
        comp.onLikeClicked = [&](const Data& data) {
            Data updated = data;
            updated.isLiked = true;
            comp.setData(updated);
        };
        
        comp.likeButtonClicked();
        
        // Verify state changed
        REQUIRE(comp.getIsLiked() == true);
    }
}
```

## Testing Patterns

### 1. Simple State Change
```cpp
TEST_CASE("Toggle visibility on button click", "[Component]") {
    Component comp;
    REQUIRE(comp.isVisible() == false);
    
    comp.toggleButton->onClick = [&]() {
        comp.setVisible(!comp.isVisible());
    };
    
    comp.toggleButton->triggerClick();
    REQUIRE(comp.isVisible() == true);
}
```

### 2. Callback with Data Updates
```cpp
TEST_CASE("Like updates post data", "[Component]") {
    Component comp;
    Post post = createPost();
    comp.setPost(post);
    
    comp.onLikeClicked = [&](const Post& p) {
        Post updated = p;
        updated.isLiked = true;
        updated.likeCount++;
        comp.setPost(updated);
    };
    
    comp.likeButtonClicked();
    
    REQUIRE(comp.getPost().isLiked == true);
    REQUIRE(comp.getPost().likeCount == Approx(post.likeCount + 1));
}
```

### 3. Multiple State Transitions
```cpp
TEST_CASE("Complex interaction sequence", "[Component]") {
    Component comp;
    comp.setState(State::Initial);
    
    // First transition
    comp.triggerFirstAction();
    REQUIRE(comp.getState() == State::Processing);
    
    // Second transition
    comp.triggerSecondAction();
    REQUIRE(comp.getState() == State::Complete);
}
```

### 4. Error Handling
```cpp
TEST_CASE("State unchanged on error", "[Component]") {
    Component comp;
    Post post = createPost();
    comp.setPost(post);
    
    comp.onLikeClicked = [](const Post& p) {
        // Simulate error - don't update
    };
    
    comp.likeButtonClicked();
    
    // Post should be unchanged
    REQUIRE(comp.getPost().likeCount == post.likeCount);
}
```

## What NOT to Test Here

These are NOT component state tests - use integration tests instead:

- **Network calls**: Mock the network layer, don't test actual HTTP
- **DAW integration**: Use plugin host testing for this
- **Audio processing**: Use audio buffer tests
- **Graphics rendering**: Screenshot comparison tests (separate)

## What TO Test Here

✅ Component state mutations  
✅ Callback invocations  
✅ Data binding updates  
✅ User interaction handlers  
✅ Animation triggers (state, not visuals)  
✅ Error states  
✅ Edge cases (empty lists, null data, etc.)

## Adding Tests for New Components

When you add a new UI component:

1. Create `tests/YourComponentTest.cpp`
2. Add test fixture with helper methods
3. Test main interactions and state changes
4. Add to `plugin/cmake/Tests.cmake` in `SIDECHAIN_TEST_SOURCES`
5. Run `make test-plugin-unit` to verify

## Common Issues

### "MessageManager not initialized"
```cpp
PostCardTestFixture() {
    if (!juce::MessageManager::getInstanceWithoutCreating()) {
        juce::MessageManager::getInstance();  // Initialize once
    }
}
```

### Component doesn't expose state
Add public getters or friend tests:
```cpp
class MyComponent {
    friend class MyComponentTest;  // Access private members in tests
private:
    bool isLiked { false };
};
```

### Callback never fires
Ensure the callback is set BEFORE triggering the action:
```cpp
comp.onClicked = [&]() { /* ... */ };  // SET FIRST
comp.buttonClicked();                   // THEN TRIGGER
```

### Component needs AppStore
Pass `nullptr` for tests, or mock it:
```cpp
PostCard card(nullptr);  // No store needed for state tests

// Or with mock:
MockAppStore mockStore;
PostCard card(&mockStore);
```

## See Also

- **File**: `plugin/tests/PostCardTest.cpp` - Full PostCard test examples
- **File**: `plugin/tests/FeedDataTest.cpp` - Data model tests
- **Docs**: [Catch2 Documentation](https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md)
- **JUCE**: [JUCE Unit Testing](https://docs.juce.com/master/tutorial_testing_audio_code.html)
