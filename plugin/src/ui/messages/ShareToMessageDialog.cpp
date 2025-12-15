#include "ShareToMessageDialog.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include "../../util/StringFormatter.h"

//==============================================================================
// Constants
//==============================================================================
namespace {
static constexpr int DIALOG_WIDTH = 500;
static constexpr int DIALOG_HEIGHT = 600;
static constexpr int PADDING = 10;
static constexpr int PREVIEW_HEIGHT = 100;
} // namespace

//==============================================================================
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

//==============================================================================
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

void ShareToMessageDialog::mouseUp([[maybe_unused]] const juce::MouseEvent &event) {
  // TODO: Implement mouse interaction
}

void ShareToMessageDialog::mouseWheelMove([[maybe_unused]] const juce::MouseEvent &event,
                                          const juce::MouseWheelDetails &wheel) {
  scrollPosition -= wheel.deltaY * 50.0;
  scrollPosition = juce::jlimit(0.0, scrollBar.getMaximumRangeLimit(), scrollPosition);
  scrollBar.setCurrentRangeStart(scrollPosition, juce::sendNotification);
  repaint();
}

void ShareToMessageDialog::textEditorTextChanged([[maybe_unused]] juce::TextEditor &editor) {
  // TODO: Implement search functionality
}

void ShareToMessageDialog::textEditorReturnKeyPressed([[maybe_unused]] juce::TextEditor &editor) {
  // TODO: Handle return key
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

void ShareToMessageDialog::setPost(const FeedPost &postToShare) {
  shareType = ShareType::Post;
  post = postToShare;
  story = Story(); // Clear story data
  dialogState = DialogState::Ready;
  repaint();
}

void ShareToMessageDialog::setStoryToShare(const Story &storyToShare) {
  shareType = ShareType::Story;
  story = storyToShare;
  post = FeedPost(); // Clear post data
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
  // TODO: Implement loading recent conversations from StreamChatClient
  // For now, this is a stub - would query recent channels from StreamChatClient
}
