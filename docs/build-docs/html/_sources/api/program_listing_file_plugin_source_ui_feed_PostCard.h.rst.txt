
.. _program_listing_file_plugin_source_ui_feed_PostCard.h:

Program Listing for File PostCard.h
===================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_ui_feed_PostCard.h>` (``plugin/source/ui/feed/PostCard.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "../../models/FeedPost.h"
   #include "../../util/Animation.h"
   #include "../../util/HoverState.h"
   #include "../../util/LongPressDetector.h"
   #include "EmojiReactionsPanel.h"
   
   //==============================================================================
   class PostCard : public juce::Component,
                             public juce::Timer
   {
   public:
       PostCard();
       ~PostCard() override;
   
       //==============================================================================
       // Data binding
   
       void setPost(const FeedPost& post);
   
       const FeedPost& getPost() const { return post; }
   
       juce::String getPostId() const { return post.id; }
   
       //==============================================================================
       // Update specific fields without full refresh
   
       void updateLikeCount(int count, bool isLiked);
   
       void updatePlayCount(int count);
   
       void updateFollowState(bool isFollowing);
   
       void updateReaction(const juce::String& emoji);
   
       void setPlaybackProgress(float progress);
   
       void setIsPlaying(bool playing);
   
       void setPlaying(bool playing) { setIsPlaying(playing); }
   
       void setLoading(bool loading);
   
       //==============================================================================
       // Callbacks for user actions
       std::function<void(const FeedPost&)> onPlayClicked;
       std::function<void(const FeedPost&)> onPauseClicked;
       std::function<void(const FeedPost&, bool liked)> onLikeToggled;
       std::function<void(const FeedPost&, const juce::String& emoji)> onEmojiReaction;  // Emoji reaction
       std::function<void(const FeedPost&)> onUserClicked;
       std::function<void(const FeedPost&)> onCommentClicked;
       std::function<void(const FeedPost&)> onShareClicked;
       std::function<void(const FeedPost&)> onMoreClicked;
       std::function<void(const FeedPost&, float position)> onWaveformClicked; // Seek position
       std::function<void(const FeedPost&, bool willFollow)> onFollowToggled; // Follow/unfollow user
       std::function<void(const FeedPost&)> onAddToDAWClicked; // Download audio to DAW project folder
   
       //==============================================================================
       // Component overrides
   
       void paint(juce::Graphics& g) override;
   
       void resized() override;
   
       void mouseDown(const juce::MouseEvent& event) override;
   
       void mouseUp(const juce::MouseEvent& event) override;
   
       void mouseEnter(const juce::MouseEvent& event) override;
   
       void mouseExit(const juce::MouseEvent& event) override;
   
       //==============================================================================
       // Timer override for animations and long-press detection
   
       void timerCallback() override;
   
       //==============================================================================
       // Layout constants
       static constexpr int CARD_HEIGHT = 120;
       static constexpr int AVATAR_SIZE = 50;
       static constexpr int BADGE_HEIGHT = 22;
       static constexpr int BUTTON_SIZE = 32;
   
   private:
       //==============================================================================
       FeedPost post;
   
       // UI state
       HoverState hoverState;
       bool isPlaying = false;
       bool isLoading = false;
       float playbackProgress = 0.0f;
   
       // Like animation
       Animation likeAnimation{400, Animation::Easing::EaseOutCubic};
   
       // Long-press detector for emoji reactions panel
       LongPressDetector longPressDetector{400};  // 400ms threshold
   
       // Cached avatar image (loaded via ImageCache)
       juce::Image avatarImage;
   
       //==============================================================================
       // Drawing methods
       void drawBackground(juce::Graphics& g);
       void drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawFollowButton(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawPlayButton(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawMetadataBadges(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawSocialButtons(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawReactionCounts(juce::Graphics& g, juce::Rectangle<int> likeBounds);
       void drawLikeAnimation(juce::Graphics& g);
   
       // Animation helpers
       void startLikeAnimation();
   
       // Emoji reactions helpers
       void showEmojiReactionsPanel();
       void handleEmojiSelected(const juce::String& emoji);
   
       //==============================================================================
       // Hit testing helpers
       juce::Rectangle<int> getAvatarBounds() const;
       juce::Rectangle<int> getUserInfoBounds() const;
       juce::Rectangle<int> getWaveformBounds() const;
       juce::Rectangle<int> getPlayButtonBounds() const;
       juce::Rectangle<int> getLikeButtonBounds() const;
       juce::Rectangle<int> getCommentButtonBounds() const;
       juce::Rectangle<int> getShareButtonBounds() const;
       juce::Rectangle<int> getMoreButtonBounds() const;
       juce::Rectangle<int> getFollowButtonBounds() const;
       juce::Rectangle<int> getAddToDAWButtonBounds() const;
   
       //==============================================================================
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostCard)
   };
