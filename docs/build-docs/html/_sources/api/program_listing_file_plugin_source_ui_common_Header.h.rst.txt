
.. _program_listing_file_plugin_source_ui_common_Header.h:

Program Listing for File Header.h
=================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_ui_common_Header.h>` (``plugin/source/ui/common/Header.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <functional>
   
   class NetworkClient;
   
   class Header : public juce::Component
   {
   public:
       static constexpr int HEADER_HEIGHT = 60;
   
       Header();
       ~Header() override = default;
   
       void paint(juce::Graphics&) override;
       void resized() override;
       void mouseUp(const juce::MouseEvent& event) override;
   
       // Set user information for display
       void setUserInfo(const juce::String& username, const juce::String& profilePicUrl);
   
       // Set profile image directly (from UserDataStore) - avoids redundant downloads
       void setProfileImage(const juce::Image& image);
   
       // Set NetworkClient for HTTP requests
       void setNetworkClient(NetworkClient* client);
   
       // Set unread message count for badge display
       void setUnreadMessageCount(int count);
   
       // Callbacks for header interactions
       std::function<void()> onSearchClicked;
       std::function<void()> onProfileClicked;
       std::function<void()> onLogoClicked;
       std::function<void()> onRecordClicked;
       std::function<void()> onStoryClicked;
       std::function<void()> onMessagesClicked;
   
   private:
       juce::String username;
       juce::String profilePicUrl;
       juce::Image cachedProfileImage;
       NetworkClient* networkClient = nullptr;
       int unreadMessageCount = 0;
   
       // Load profile image from URL
       void loadProfileImage(const juce::String& url);
   
       // Draw helper methods
       void drawLogo(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawSearchButton(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawRecordButton(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawMessagesButton(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawProfileSection(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds);
   
       // Hit test bounds
       juce::Rectangle<int> getLogoBounds() const;
       juce::Rectangle<int> getSearchButtonBounds() const;
       juce::Rectangle<int> getRecordButtonBounds() const;
       juce::Rectangle<int> getMessagesButtonBounds() const;
       juce::Rectangle<int> getProfileBounds() const;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Header)
   };
