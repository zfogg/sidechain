#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * SkeletonLoader - Base class for skeleton loading placeholders
 *
 * Provides shimmer animation effect that moves across placeholder shapes
 * to indicate loading state. Subclasses define the specific shapes.
 *
 * Usage:
 *   class PostCardSkeleton : public SkeletonLoader {
 *       void drawSkeletonShapes(juce::Graphics& g) override {
 *           drawRect(g, avatarBounds);
 *           drawLine(g, titleBounds);
 *           ...
 *       }
 *   };
 */
class SkeletonLoader : public juce::Component, private juce::Timer
{
public:
    //==========================================================================
    SkeletonLoader();
    ~SkeletonLoader() override;

    //==========================================================================
    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;

    //==========================================================================
    // Configuration

    /** Set the base color for skeleton shapes (default: gray) */
    void setBaseColor(juce::Colour color) { baseColor = color; }

    /** Set the shimmer highlight color (default: lighter gray) */
    void setShimmerColor(juce::Colour color) { shimmerColor = color; }

    /** Set corner radius for rounded rects (default: 4) */
    void setCornerRadius(float radius) { cornerRadius = radius; }

    /** Enable/disable shimmer animation (default: true) */
    void setShimmerEnabled(bool enabled);

    /** Set shimmer animation duration in ms (default: 1500) */
    void setShimmerDuration(int durationMs) { shimmerDurationMs = durationMs; }

protected:
    //==========================================================================
    // Drawing helpers for subclasses

    /** Draw a rectangle placeholder (for images, cards) */
    void drawRect(juce::Graphics& g, juce::Rectangle<int> bounds);

    /** Draw a rounded rectangle placeholder */
    void drawRoundedRect(juce::Graphics& g, juce::Rectangle<int> bounds);

    /** Draw a circle placeholder (for avatars) */
    void drawCircle(juce::Graphics& g, juce::Rectangle<int> bounds);

    /** Draw a text line placeholder */
    void drawLine(juce::Graphics& g, juce::Rectangle<int> bounds, float widthPercent = 1.0f);

    /** Draw multiple text lines */
    void drawLines(juce::Graphics& g, juce::Rectangle<int> bounds, int lineCount, int lineHeight = 14, int lineSpacing = 8);

    //==========================================================================
    // Override in subclass to define skeleton shapes
    virtual void drawSkeletonShapes(juce::Graphics& g) = 0;

protected:
    /** Get the color with shimmer effect applied based on position */
    juce::Colour getColorWithShimmer(juce::Rectangle<int> bounds) const;

private:
    void timerCallback() override;

    juce::Colour baseColor;
    juce::Colour shimmerColor;
    float cornerRadius = 4.0f;
    bool shimmerEnabled = true;
    int shimmerDurationMs = 1500;

    // Animation state
    float shimmerProgress = 0.0f;
    juce::int64 animationStartTime = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkeletonLoader)
};

//==============================================================================
/**
 * PostCardSkeleton - Skeleton placeholder for PostCard
 *
 * Mimics the layout of a PostCard while loading
 */
class PostCardSkeleton : public SkeletonLoader
{
public:
    PostCardSkeleton() = default;
    void resized() override;

protected:
    void drawSkeletonShapes(juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostCardSkeleton)
};

//==============================================================================
/**
 * ProfileSkeleton - Skeleton placeholder for Profile view
 *
 * Mimics the profile header (avatar, stats, bio) while loading
 */
class ProfileSkeleton : public SkeletonLoader
{
public:
    ProfileSkeleton() = default;

protected:
    void drawSkeletonShapes(juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfileSkeleton)
};

//==============================================================================
/**
 * StoryCircleSkeleton - Skeleton for story circle (avatar ring)
 */
class StoryCircleSkeleton : public SkeletonLoader
{
public:
    StoryCircleSkeleton() = default;

protected:
    void drawSkeletonShapes(juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StoryCircleSkeleton)
};

//==============================================================================
/**
 * CommentSkeleton - Skeleton for comment rows
 */
class CommentSkeleton : public SkeletonLoader
{
public:
    CommentSkeleton() = default;

protected:
    void drawSkeletonShapes(juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommentSkeleton)
};

//==============================================================================
/**
 * FeedSkeleton - Container that shows multiple PostCardSkeletons
 */
class FeedSkeleton : public juce::Component
{
public:
    FeedSkeleton(int cardCount = 3);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setCardCount(int count);

private:
    juce::OwnedArray<PostCardSkeleton> skeletons;
    int cardHeight = 200;
    int cardSpacing = 12;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeedSkeleton)
};
