#pragma once

#include "../util/logging/Logger.h"
#include "DraftStorage.h"
#include "Store.h"
#include <JuceHeader.h>
#include <map>

namespace Sidechain {
namespace Stores {

/**
 * DraftStoreState - Immutable state for recording drafts
 */
struct DraftStoreState {
  // All available drafts (metadata only)
  juce::Array<Draft> drafts;

  // Currently selected/editing draft
  juce::String currentDraftId;

  // Loading states
  bool isLoadingDrafts = false;
  bool isSavingDraft = false;
  bool isDeletingDraft = false;

  // Error state
  juce::String error;

  // Timestamps
  int64_t lastUpdated = 0;

  bool operator==(const DraftStoreState &other) const {
    return drafts.size() == other.drafts.size() && currentDraftId == other.currentDraftId &&
           isLoadingDrafts == other.isLoadingDrafts && isSavingDraft == other.isSavingDraft &&
           isDeletingDraft == other.isDeletingDraft && lastUpdated == other.lastUpdated;
  }
};

/**
 * DraftStore - Reactive store for recording drafts (Task 2.5)
 *
 * Replaces callback-based draft management with reactive subscriptions.
 *
 * Features:
 * - Reactive draft list management
 * - Save/load drafts with audio
 * - Delete drafts
 * - Auto-recovery draft
 * - Optimistic UI updates
 *
 * Usage:
 *   // Get singleton instance
 *   auto& draftStore = DraftStore::getInstance();
 *
 *   // Subscribe to state changes
 *   auto unsubscribe = draftStore.subscribe([](const DraftStoreState& state) {
 *       displayDrafts(state.drafts);
 *   });
 *
 *   // Save a draft
 *   draftStore.saveDraft(draft, audioBuffer);
 *
 *   // Load all drafts
 *   draftStore.loadDrafts();
 */
class DraftStore : public Store<DraftStoreState> {
public:
  /**
   * Get singleton instance
   */
  static DraftStore &getInstance() {
    static DraftStore instance;
    return instance;
  }

  //==========================================================================
  // Draft Operations

  /**
   * Load all drafts (metadata only)
   */
  void loadDrafts();

  /**
   * Save a draft with audio
   * @param draft Draft metadata
   * @param audioBuffer Audio data to save
   */
  void saveDraft(const Draft &draft, const juce::AudioBuffer<float> &audioBuffer);

  /**
   * Load a draft with audio
   * @param draftId Draft ID to load
   * @param audioBuffer [out] Audio data
   * @return The draft metadata
   */
  Draft loadDraft(const juce::String &draftId, juce::AudioBuffer<float> &audioBuffer);

  /**
   * Delete a draft
   * @param draftId Draft ID to delete
   */
  void deleteDraft(const juce::String &draftId);

  /**
   * Select a draft for editing
   * @param draftId Draft ID to select
   */
  void selectDraft(const juce::String &draftId);

  /**
   * Get currently selected draft
   */
  juce::String getCurrentDraftId() const {
    return getState().currentDraftId;
  }

  /**
   * Get all drafts
   */
  juce::Array<Draft> getDrafts() const {
    return getState().drafts;
  }

  //==========================================================================
  // Auto-Recovery

  /**
   * Save auto-recovery draft
   * @param draft Draft to save
   * @param audioBuffer Audio data
   */
  void saveAutoRecoveryDraft(const Draft &draft, const juce::AudioBuffer<float> &audioBuffer);

  /**
   * Check if auto-recovery draft exists
   */
  bool hasAutoRecoveryDraft() const;

  /**
   * Load auto-recovery draft
   * @param audioBuffer [out] Audio data
   * @return Draft metadata
   */
  Draft loadAutoRecoveryDraft(juce::AudioBuffer<float> &audioBuffer);

  /**
   * Clear auto-recovery draft
   */
  void clearAutoRecoveryDraft();

private:
  DraftStore();
  ~DraftStore() override = default;

  // Draft storage backend
  std::unique_ptr<DraftStorage> storage;

  // Singleton enforcement
  DraftStore(const DraftStore &) = delete;
  DraftStore &operator=(const DraftStore &) = delete;
};

} // namespace Stores
} // namespace Sidechain
