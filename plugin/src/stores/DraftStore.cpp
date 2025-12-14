#include "DraftStore.h"
#include "../util/Log.h"
#include <thread>

namespace Sidechain {
namespace Stores {

//==============================================================================
// DraftStore implementation
//==============================================================================

DraftStore::DraftStore()
    : storage(std::make_unique<DraftStorage>())
{
    Log::info("DraftStore initialized");

    // Initialize state
    DraftStoreState initialState;
    initialState.isLoadingDrafts = false;
    initialState.isSavingDraft = false;
    initialState.isDeletingDraft = false;
    initialState.error = "";
    initialState.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();

    setState(initialState);
}

//==============================================================================
// Draft Operations
//==============================================================================

void DraftStore::loadDrafts()
{
    // Set loading state
    {
        auto state = getState();
        state.isLoadingDrafts = true;
        state.error = "";
        setState(state);
    }

    // Load drafts on background thread
    std::thread([this]() {
        try
        {
            juce::Array<Draft> drafts = storage->getAllDrafts();

            auto state = getState();
            state.drafts = drafts;
            state.isLoadingDrafts = false;
            state.error = "";
            state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();

            setState(state);
            Log::debug("DraftStore: Loaded " + juce::String(drafts.size()) + " drafts");
        }
        catch (const std::exception& e)
        {
            auto state = getState();
            state.isLoadingDrafts = false;
            state.error = "Failed to load drafts: " + juce::String(e.what());
            setState(state);

            Log::error("DraftStore::loadDrafts: " + state.error);
        }
    }).detach();
}

void DraftStore::saveDraft(const Draft& draft, const juce::AudioBuffer<float>& audioBuffer)
{
    // Set saving state
    {
        auto state = getState();
        state.isSavingDraft = true;
        state.error = "";
        setState(state);
    }

    // Save draft on background thread
    std::thread([this, draft, audioBuffer]() {
        try
        {
            // Make a copy of the buffer
            juce::AudioBuffer<float> bufferCopy = audioBuffer;

            Draft savedDraft = storage->saveDraft(draft, bufferCopy);

            if (savedDraft.id.isEmpty())
            {
                auto state = getState();
                state.isSavingDraft = false;
                state.error = "Failed to save draft";
                setState(state);

                Log::error("DraftStore::saveDraft: Storage returned empty draft");
                return;
            }

            // Add to drafts list
            auto state = getState();

            // Remove old version if updating
            for (int i = 0; i < state.drafts.size(); ++i)
            {
                if (state.drafts[i].id == savedDraft.id)
                {
                    state.drafts.remove(i);
                    break;
                }
            }

            // Add at beginning (newest first)
            state.drafts.insert(0, savedDraft);
            state.isSavingDraft = false;
            state.error = "";
            state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();

            setState(state);
            Log::info("DraftStore: Saved draft " + savedDraft.id);
        }
        catch (const std::exception& e)
        {
            auto state = getState();
            state.isSavingDraft = false;
            state.error = "Failed to save draft: " + juce::String(e.what());
            setState(state);

            Log::error("DraftStore::saveDraft: " + state.error);
        }
    }).detach();
}

Draft DraftStore::loadDraft(const juce::String& draftId, juce::AudioBuffer<float>& audioBuffer)
{
    if (draftId.isEmpty())
    {
        Log::warn("DraftStore::loadDraft: Empty draft ID");
        return Draft();
    }

    try
    {
        Draft draft = storage->loadDraft(draftId, audioBuffer);

        if (draft.id.isEmpty())
        {
            Log::warn("DraftStore::loadDraft: Draft not found: " + draftId);
            return Draft();
        }

        // Select the draft
        selectDraft(draftId);

        Log::info("DraftStore: Loaded draft " + draftId);
        return draft;
    }
    catch (const std::exception& e)
    {
        auto state = getState();
        state.error = "Failed to load draft: " + juce::String(e.what());
        setState(state);

        Log::error("DraftStore::loadDraft: " + state.error);
        return Draft();
    }
}

void DraftStore::deleteDraft(const juce::String& draftId)
{
    if (draftId.isEmpty())
    {
        Log::warn("DraftStore::deleteDraft: Empty draft ID");
        return;
    }

    // Set deleting state
    {
        auto state = getState();
        state.isDeletingDraft = true;
        state.error = "";
        setState(state);
    }

    // Delete draft on background thread
    std::thread([this, draftId]() {
        try
        {
            bool success = storage->deleteDraft(draftId);

            if (!success)
            {
                auto state = getState();
                state.isDeletingDraft = false;
                state.error = "Failed to delete draft";
                setState(state);

                Log::warn("DraftStore::deleteDraft: Storage failed to delete draft");
                return;
            }

            // Remove from drafts list
            auto state = getState();
            for (int i = 0; i < state.drafts.size(); ++i)
            {
                if (state.drafts[i].id == draftId)
                {
                    state.drafts.remove(i);
                    break;
                }
            }

            // Clear selection if needed
            if (state.currentDraftId == draftId)
            {
                state.currentDraftId = "";
            }

            state.isDeletingDraft = false;
            state.error = "";
            state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();

            setState(state);
            Log::info("DraftStore: Deleted draft " + draftId);
        }
        catch (const std::exception& e)
        {
            auto state = getState();
            state.isDeletingDraft = false;
            state.error = "Failed to delete draft: " + juce::String(e.what());
            setState(state);

            Log::error("DraftStore::deleteDraft: " + state.error);
        }
    }).detach();
}

void DraftStore::selectDraft(const juce::String& draftId)
{
    auto state = getState();
    state.currentDraftId = draftId;
    setState(state);

    Log::debug("DraftStore: Selected draft " + draftId);
}

//==============================================================================
// Auto-Recovery
//==============================================================================

void DraftStore::saveAutoRecoveryDraft(const Draft& draft, const juce::AudioBuffer<float>& audioBuffer)
{
    std::thread([this, draft, audioBuffer]() {
        try
        {
            juce::AudioBuffer<float> bufferCopy = audioBuffer;
            storage->saveAutoRecoveryDraft(draft, bufferCopy);
            Log::debug("DraftStore: Saved auto-recovery draft");
        }
        catch (const std::exception& e)
        {
            Log::error("DraftStore::saveAutoRecoveryDraft: " + juce::String(e.what()));
        }
    }).detach();
}

bool DraftStore::hasAutoRecoveryDraft() const
{
    try
    {
        return storage->hasAutoRecoveryDraft();
    }
    catch (const std::exception& e)
    {
        Log::error("DraftStore::hasAutoRecoveryDraft: " + juce::String(e.what()));
        return false;
    }
}

Draft DraftStore::loadAutoRecoveryDraft(juce::AudioBuffer<float>& audioBuffer)
{
    try
    {
        Draft draft = storage->loadAutoRecoveryDraft(audioBuffer);

        if (draft.hasAudio())
        {
            Log::info("DraftStore: Loaded auto-recovery draft");
            return draft;
        }

        return Draft();
    }
    catch (const std::exception& e)
    {
        Log::error("DraftStore::loadAutoRecoveryDraft: " + juce::String(e.what()));
        return Draft();
    }
}

void DraftStore::clearAutoRecoveryDraft()
{
    std::thread([this]() {
        try
        {
            storage->clearAutoRecoveryDraft();
            Log::debug("DraftStore: Cleared auto-recovery draft");
        }
        catch (const std::exception& e)
        {
            Log::error("DraftStore::clearAutoRecoveryDraft: " + juce::String(e.what()));
        }
    }).detach();
}

}  // namespace Stores
}  // namespace Sidechain
