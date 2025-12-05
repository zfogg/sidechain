#include "EmojiReactionsPanel.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"

//==============================================================================
// EmojiReactionsPanel Implementation
//==============================================================================

EmojiReactionsPanel::EmojiReactionsPanel()
{
    Log::debug("EmojiReactionsPanel: Initializing");
    setSize(getPreferredSize().getWidth(), getPreferredSize().getHeight());
}

EmojiReactionsPanel::~EmojiReactionsPanel()
{
    Log::debug("EmojiReactionsPanel: Destroying");
}

//==============================================================================
void EmojiReactionsPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background with rounded corners
    g.setColour(SidechainColors::surface());
    g.fillRoundedRectangle(bounds, 12.0f);

    // Border
    g.setColour(SidechainColors::border());
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    // Draw each emoji
    const auto& emojis = FeedPost::REACTION_EMOJIS;

    for (int i = 0; i < emojis.size(); ++i)
    {
        auto emojiBounds = getEmojiBounds(i);

        // Highlight if hovered
        if (i == hoveredIndex)
        {
            g.setColour(SidechainColors::surfaceHover());
            g.fillRoundedRectangle(emojiBounds.toFloat().expanded(2.0f), 6.0f);
        }

        // Highlight if selected
        if (emojis[i] == selectedEmoji)
        {
            g.setColour(SidechainColors::skyBlue().withAlpha(0.3f));
            g.fillRoundedRectangle(emojiBounds.toFloat().expanded(2.0f), 6.0f);

            // Selection ring
            g.setColour(SidechainColors::skyBlue());
            g.drawRoundedRectangle(emojiBounds.toFloat().expanded(2.0f), 6.0f, 2.0f);
        }

        // Draw the emoji
        g.setFont(static_cast<float>(EMOJI_SIZE - 4));
        g.setColour(SidechainColors::textPrimary());
        g.drawText(emojis[i], emojiBounds, juce::Justification::centred);
    }
}

void EmojiReactionsPanel::resized()
{
    // Layout is calculated in getEmojiBounds
}

void EmojiReactionsPanel::mouseUp(const juce::MouseEvent& event)
{
    int index = getEmojiIndexAtPosition(event.getPosition());

    if (index >= 0 && index < FeedPost::REACTION_EMOJIS.size())
    {
        juce::String emoji = FeedPost::REACTION_EMOJIS[index];

        if (onEmojiSelected)
            onEmojiSelected(emoji);

        if (onDismiss)
            onDismiss();
    }
}

void EmojiReactionsPanel::mouseMove(const juce::MouseEvent& event)
{
    int newHovered = getEmojiIndexAtPosition(event.getPosition());

    if (newHovered != hoveredIndex)
    {
        hoveredIndex = newHovered;
        repaint();
    }
}

void EmojiReactionsPanel::mouseExit(const juce::MouseEvent& /*event*/)
{
    if (hoveredIndex != -1)
    {
        hoveredIndex = -1;
        repaint();
    }
}

//==============================================================================
void EmojiReactionsPanel::setSelectedEmoji(const juce::String& emoji)
{
    selectedEmoji = emoji;
    Log::debug("EmojiReactionsPanel: Selected emoji - " + emoji);
    repaint();
}

juce::Rectangle<int> EmojiReactionsPanel::getPreferredSize()
{
    int numEmojis = FeedPost::REACTION_EMOJIS.size();
    int width = PANEL_PADDING * 2 + numEmojis * EMOJI_SIZE + (numEmojis - 1) * EMOJI_SPACING;
    return juce::Rectangle<int>(0, 0, width, PANEL_HEIGHT);
}

//==============================================================================
juce::Rectangle<int> EmojiReactionsPanel::getEmojiBounds(int index) const
{
    if (index < 0 || index >= FeedPost::REACTION_EMOJIS.size())
        return {};

    int x = PANEL_PADDING + index * (EMOJI_SIZE + EMOJI_SPACING);
    int y = (getHeight() - EMOJI_SIZE) / 2;

    return juce::Rectangle<int>(x, y, EMOJI_SIZE, EMOJI_SIZE);
}

int EmojiReactionsPanel::getEmojiIndexAtPosition(juce::Point<int> pos) const
{
    for (int i = 0; i < FeedPost::REACTION_EMOJIS.size(); ++i)
    {
        if (getEmojiBounds(i).expanded(4).contains(pos))
            return i;
    }
    return -1;
}

//==============================================================================
// EmojiReactionsBubble Implementation
//==============================================================================

EmojiReactionsBubble::EmojiReactionsBubble(juce::Component* targetComponent)
    : target(targetComponent)
{
    panel = std::make_unique<EmojiReactionsPanel>();

    // Forward callbacks
    panel->onEmojiSelected = [this](const juce::String& emoji) {
        if (onEmojiSelected)
            onEmojiSelected(emoji);
        dismiss();
    };

    panel->onDismiss = [this]() {
        dismiss();
    };

    addAndMakeVisible(panel.get());

    // Calculate size including arrow
    auto panelSize = EmojiReactionsPanel::getPreferredSize();
    setSize(panelSize.getWidth(), panelSize.getHeight() + ARROW_SIZE);
}

EmojiReactionsBubble::~EmojiReactionsBubble()
{
}

//==============================================================================
void EmojiReactionsBubble::show()
{
    if (target == nullptr)
        return;

    // Get target bounds in screen coordinates
    targetBounds = target->getScreenBounds();

    // Position bubble above the target, centered horizontally
    int bubbleX = targetBounds.getCentreX() - getWidth() / 2;
    int bubbleY = targetBounds.getY() - getHeight() - 5;

    // Get the top-level component to add this bubble to
    if (auto* topLevel = target->getTopLevelComponent())
    {
        // Convert to parent coordinates
        auto topLeftScreen = juce::Point<int>(bubbleX, bubbleY);
        auto topLeftLocal = topLevel->getLocalPoint(nullptr, topLeftScreen);

        setTopLeftPosition(topLeftLocal);
        topLevel->addAndMakeVisible(this);

        // Make modal to capture clicks outside
        enterModalState(false);

        toFront(true);
    }
}

void EmojiReactionsBubble::dismiss()
{
    exitModalState(0);

    if (auto* parent = getParentComponent())
        parent->removeChildComponent(this);

    // Self-destruct after dismissal
    juce::MessageManager::callAsync([this]() {
        delete this;
    });
}

//==============================================================================
void EmojiReactionsBubble::paint(juce::Graphics& g)
{
    // Draw the bubble background with arrow pointing down
    juce::Path bubblePath;

    auto bounds = getLocalBounds().toFloat();
    auto panelBounds = bounds.removeFromTop(bounds.getHeight() - ARROW_SIZE);

    // Main bubble
    bubblePath.addRoundedRectangle(panelBounds, CORNER_RADIUS);

    // Arrow pointing down (centered)
    float arrowX = bounds.getCentreX();
    float arrowY = panelBounds.getBottom();

    juce::Path arrowPath;
    arrowPath.startNewSubPath(arrowX - ARROW_SIZE, arrowY);
    arrowPath.lineTo(arrowX, arrowY + ARROW_SIZE);
    arrowPath.lineTo(arrowX + ARROW_SIZE, arrowY);
    arrowPath.closeSubPath();

    bubblePath.addPath(arrowPath);

    // Fill background
    g.setColour(SidechainColors::surface());
    g.fillPath(bubblePath);

    // Border
    g.setColour(SidechainColors::border());
    g.strokePath(bubblePath, juce::PathStrokeType(1.0f));
}

void EmojiReactionsBubble::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromBottom(ARROW_SIZE);
    panel->setBounds(bounds);
}

void EmojiReactionsBubble::mouseDown(const juce::MouseEvent& event)
{
    // Check if click is outside the panel
    if (!panel->getBounds().contains(event.getPosition()))
    {
        dismiss();
    }
}

void EmojiReactionsBubble::inputAttemptWhenModal()
{
    // User clicked outside the modal - dismiss
    dismiss();
}

//==============================================================================
void EmojiReactionsBubble::setSelectedEmoji(const juce::String& emoji)
{
    if (panel)
        panel->setSelectedEmoji(emoji);
}
