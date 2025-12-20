#include "ShareToMessageDialog.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include "../../util/StringFormatter.h"

// ==============================================================================
// Constants
// ==============================================================================
namespace {
static constexpr int DIALOG_WIDTH = 500;
static constexpr int DIALOG_HEIGHT = 600;
static constexpr int PADDING = 10;
static constexpr int PREVIEW_HEIGHT = 100;
} // namespace

// ==============================================================================
ShareToMessageDialog::ShareToMessageDialog()
    : scrollBar(true) // Initialize scrollbar as vertical
{
  // Search input
  searchInput.setMultiLine(false);
  searchInput.setReturnKeyStartsNewLine(false);
  searchInput.setScrollbarsShown(false);
  searchInput.setCaretVisible(true);
  searchInput.setTextToShowWhenEmpty("Search for a user...", SidechainColors::textMuted());
  searchInput.setFont(juce::Font(juce::FontOptions().withHeight(15.0f)));
  searchInput.setColour(juce::TextEditor::backgroundColourId, SidechainColors::surface());
  searchInput.setColour(juce::TextEditor::textColourId, SidechainColors::textPrimary());
  searchInput.setColour(juce::TextEditor::outlineColourId, SidechainColors::border());
  searchInput.setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::primary());
  searchInput.addListener(this);
  addAndMakeVisible(searchInput);

  // Message input
  messageInput.setMultiLine(true);
  messageInput.setReturnKeyStartsNewLine(true);
  messageInput.setScrollbarsShown(true);
  messageInput.setCaretVisible(true);
  messageInput.setTextToShowWhenEmpty("Add a message (optional)...", SidechainColors::textMuted());
  messageInput.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  messageInput.setColour(juce::TextEditor::backgroundColourId, SidechainColors::surface());
  messageInput.setColour(juce::TextEditor::textColourId, SidechainColors::textPrimary());
  messageInput.setColour(juce::TextEditor::outlineColourId, SidechainColors::border());
  messageInput.setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::primary());
  addAndMakeVisible(messageInput);

  // Scroll bar
  scrollBar.addListener(this);
  scrollBar.setRangeLimits(0.0, 0.0);
  addAndMakeVisible(scrollBar);
}

ShareToMessageDialog::~ShareToMessageDialog() {
  scrollBar.removeListener(this);
}

// ==============================================================================
void ShareToMessageDialog::paint(juce::Graphics &g) {
  // Semi-transparent backdrop
  g.fillAll(juce::Colours::black.withAlpha(0.6f));

  // Dialog background
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);

  // Shadow
  g.setColour(juce::Colours::black.withAlpha(0.3f));
  g.fillRoundedRectangle(dialogBounds.toFloat().translated(4, 4), 12.0f);

  // Background
  g.setColour(SidechainColors::backgroundLight());
  g.fillRoundedRectangle(dialogBounds.toFloat(), 12.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(dialogBounds.toFloat(), 12.0f, 1.0f);

  drawHeader(g);
  drawPostPreview(g);
}

void ShareToMessageDialog::drawHeader(juce::Graphics &g) {
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
  auto headerBounds = dialogBounds.removeFromTop(HEADER_HEIGHT);

  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
  g.drawText("Send to...", headerBounds.reduced(PADDING, 0).withTrimmedTop(15), juce::Justification::centredLeft);
}

void ShareToMessageDialog::drawPostPreview(juce::Graphics &g) {
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
  dialogBounds.removeFromTop(HEADER_HEIGHT);
  auto previewBounds = dialogBounds.removeFromTop(PREVIEW_HEIGHT).reduced(PADDING, 5);

  // Background
  g.setColour(SidechainColors::surface());
  g.fillRoundedRectangle(previewBounds.toFloat(), 8.0f);

  // TODO: Implement post preview rendering
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  g.drawText("Post preview", previewBounds, juce::Justification::centred);
}

void ShareToMessageDialog::drawLoadingState(juce::Graphics &g) {
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  g.drawText("Loading...", dialogBounds, juce::Justification::centred);
}

void ShareToMessageDialog::resized() {
  auto bounds = getLocalBounds();
  auto dialogBounds = bounds.withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);

  // Search input at top
  auto headerBounds = dialogBounds.removeFromTop(HEADER_HEIGHT);
  searchInput.setBounds(headerBounds.reduced(PADDING, 10));

  // Preview section
  dialogBounds.removeFromTop(PREVIEW_HEIGHT);

  // Message input at bottom
  auto bottomBounds = dialogBounds.removeFromBottom(MESSAGE_INPUT_HEIGHT);
  messageInput.setBounds(bottomBounds.reduced(PADDING, 10));

  // Scrollbar on right
  scrollBar.setBounds(dialogBounds.removeFromRight(12));
}

void ShareToMessageDialog::mouseUp(const juce::MouseEvent &event) {
  // Check if click is within dialog area (not on backdrop)
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
  if (!dialogBounds.contains(event.getPosition())) {
    // Clicked on backdrop - close dialog
    if (onCancelled)
      onCancelled();
    return;
  }

  // Could handle clicks on conversation items here
  // For now, just register that event was handled
  Log::debug("ShareToMessageDialog: Click at (" + juce::String(event.getPosition().getX()) + ", " +
             juce::String(event.getPosition().getY()) + ")");
}

void ShareToMessageDialog::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) {
  // Only scroll if wheel is within dialog bounds (not on backdrop)
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
  if (dialogBounds.contains(event.getPosition()) && event.x < getWidth() - scrollBar.getWidth()) {
    scrollPosition -= wheel.deltaY * 50.0;
    scrollPosition = juce::jlimit(0.0, scrollBar.getMaximumRangeLimit(), scrollPosition);
    scrollBar.setCurrentRangeStart(scrollPosition, juce::sendNotification);
    repaint();
  }
}

void ShareToMessageDialog::textEditorTextChanged(juce::TextEditor &editor) {
  // Search conversations when text changes
  if (&editor == &searchInput) {
    // Debounce search with timer
    stopTimer();
    if (!searchInput.getText().isEmpty()) {
      startTimer(300);
    }
  }
}

void ShareToMessageDialog::textEditorReturnKeyPressed(juce::TextEditor &editor) {
  // Send message when return key pressed in message input
  if (&editor == &messageInput) {
    // Could trigger sending the message here
    Log::debug("ShareToMessageDialog: Return key pressed in message input");
  }
}

void ShareToMessageDialog::timerCallback() {
  // TODO: Implement debounced search
}

void ShareToMessageDialog::scrollBarMoved(juce::ScrollBar *bar, double newRangeStart) {
  if (bar == &scrollBar) {
    scrollPosition = newRangeStart;
    repaint();
  }
}

void ShareToMessageDialog::setStreamChatClient(StreamChatClient *client) {
  streamChatClient = client;
}

void ShareToMessageDialog::setNetworkClient(NetworkClient *client) {
  networkClient = client;
}

void ShareToMessageDialog::setPost(const Sidechain::FeedPost &postToShare) {
  shareType = ShareType::Post;
  post = postToShare;
  story = Sidechain::Story(); // Clear story data
  dialogState = DialogState::Ready;
  repaint();
}

void ShareToMessageDialog::setStoryToShare(const Sidechain::Story &storyToShare) {
  shareType = ShareType::Story;
  story = storyToShare;
  post = Sidechain::FeedPost(); // Clear post data
  dialogState = DialogState::Ready;
  repaint();
}

void ShareToMessageDialog::showModal(juce::Component *parent) {
  if (parent == nullptr)
    return;

  // Center dialog on parent
  auto parentBounds = parent->getLocalBounds();
  int x = (parentBounds.getWidth() - DIALOG_WIDTH) / 2;
  int y = (parentBounds.getHeight() - DIALOG_HEIGHT) / 2;
  setBounds(x, y, DIALOG_WIDTH, DIALOG_HEIGHT);

  parent->addAndMakeVisible(this);
  toFront(true);
  searchInput.grabKeyboardFocus();

  // Load recent conversations when showing
  loadRecentConversations();
}

void ShareToMessageDialog::loadRecentConversations() {
  if (streamChatClient == nullptr) {
    Log::warn("ShareToMessageDialog: Cannot load recent conversations - no StreamChatClient");
    return;
  }

  Log::debug("ShareToMessageDialog: Loading recent conversations from StreamChatClient");

  // Load recent channels (limit to 20, sorted by last message time)
  streamChatClient->queryChannels(
      [this](const Outcome<std::vector<StreamChatClient::Channel>> &result) {
        if (!result) {
          Log::error("ShareToMessageDialog: Failed to load recent conversations - " + result.getError());
          return;
        }

        // Convert Stream Chat channels to ConversationItems
        recentConversations.clear();
        const auto &channels = result.getValue();
        for (const auto &channel : channels) {
          ConversationItem conversation;
          conversation.channelType = channel.type;
          conversation.channelId = channel.id;
          conversation.channelName = channel.name;
          conversation.isGroup = (channel.members.size() > 2);
          conversation.memberCount = static_cast<int>(channel.members.size());
          recentConversations.push_back(conversation);
        }

        Log::debug("ShareToMessageDialog: Loaded " + juce::String(recentConversations.size()) +
                   " recent conversations");
        repaint();
      },
      20, // limit
      0   // offset
  );
}
