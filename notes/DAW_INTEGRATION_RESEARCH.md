# Deep DAW Integration Research (R.3.3.7.3)

**Status**: Research Complete - Future Implementation  
**Last Updated**: December 2025  
**Related**: `notes/PLAN.README_features_TODOs.md` Section R.3.3.7.3

## Overview

This document outlines research findings for advanced DAW integration features beyond basic file placement. These features would enable deeper workflow integration between Sidechain and various DAWs, including tempo synchronization, MIDI routing, and automation.

## Current State

✅ **Implemented:**
- DAW detection (`DAWProjectFolder.h/cpp`, `NetworkClient::detectDAWName()`)
- DAW project folder detection for file placement
- MIDI file placement in DAW project folders
- BPM reading from DAW via `AudioPlayHead` (`PluginProcessor`)
- Transport state detection (play/stop) for audio focus

📋 **Future Research Areas:**
1. Ableton Link for tempo synchronization
2. VST3 MIDI output to DAW host
3. ReaScript integration for REAPER automation

---

## 1. Ableton Link Integration

### Overview
Ableton Link is a technology that synchronizes tempo, phase, and start/stop commands across multiple devices and applications over a local network. It enables real-time collaboration and tempo sync across different software and hardware.

### Research Findings

**Feasibility**: ⚠️ **Partially Feasible**

**Key Limitations:**
- VST plugins **cannot control the host DAW's tempo** from within the plugin
- Plugins can sync their internal tempo to Link sessions but cannot force the DAW to follow
- The host DAW will be unable to accept tempo changes from the plugin (VST standard limitation)

**What's Possible:**
- Plugin can participate in a Link session
- Plugin can adjust its internal tempo to match the Link network
- Plugin can sync playback state with other Link-enabled devices
- Useful for: Syncing plugin's internal features (metronome, delays, etc.) with external devices

**Implementation Steps:**

1. **Add Ableton Link as Dependency:**
   ```bash
   git submodule add https://github.com/Ableton/link link
   git submodule update --init --recursive
   ```

2. **Configure CMake/Build System:**
   - Add header search paths:
     - `link/include`
     - `link/modules/asio-standalone/asio/include`
   - Platform-specific preprocessor definitions:
     - macOS: `LINK_PLATFORM_MACOSX=1`
     - Windows: `LINK_PLATFORM_WINDOWS=1`
     - Linux: `LINK_PLATFORM_LINUX=1`

3. **Include Headers:**
   ```cpp
   #include <ableton/Link.hpp>
   #include <ableton/link/HostTimeFilter.hpp>
   ```

4. **Integration Architecture:**
   ```cpp
   // In PluginProcessor.h
   class SidechainAudioProcessor {
       std::unique_ptr<ableton::Link> link;
       ableton::Link::SessionState linkSession;
       
       void initializeLink();
       void updateLinkSession(double bpm);
   };
   ```

**Use Cases for Sidechain:**
- Sync feed audio playback with other Link-enabled devices
- Sync internal metronome/visualizers with external tempo sources
- Enable collaborative sessions where multiple producers sync their tempo
- **NOT** for controlling DAW tempo (limitation of VST standard)

**Recommendation**: 
- **Low Priority** - Limited value since we can't control DAW tempo
- Better suited for standalone applications or ARA plugins
- May be useful for future "collaborative session" features

**Resources:**
- [Ableton Link GitHub](https://github.com/Ableton/link)
- [Ableton Link Tutorial for JUCE](https://github.com/ianacaburian/AbletonLink_JuceSampler)

---

## 2. VST3 MIDI Output to DAW

### Overview
VST3 plugins can send MIDI events back to the host DAW, enabling MIDI generation, arpeggiation, and other MIDI-processing features. This could allow Sidechain to generate MIDI patterns that appear in the DAW's MIDI editor.

### Research Findings

**Feasibility**: ✅ **Feasible with Limitations**

**DAW Support Matrix:**

| DAW | MIDI Output Support | Notes |
|-----|-------------------|-------|
| **Ableton Live 11+** | ✅ Yes | Full MIDI output support (notes, CC, pitch bend) |
| **Cubase 2020+** | ✅ Yes | Full MIDI output support |
| **REAPER 2020+** | ✅ Yes | Full MIDI output support |
| **Studio One 6+** | ✅ Yes | Full MIDI output support |
| **FL Studio** | ⚠️ Partial | Some MIDI output support (version dependent) |
| **Logic Pro** | ❌ No | Uses AU format, which doesn't support MIDI output |
| **Pro Tools** | ⚠️ Partial | Limited MIDI output support (VST3 support varies) |

**Implementation:**

1. **Enable MIDI Output in Plugin:**
   ```cpp
   // In PluginProcessor.h
   bool producesMidi() const override { return true; }
   ```

2. **Create MIDI Output Bus:**
   ```cpp
   // In constructor or prepareToPlay
   // VST3 allows multiple output buses
   // Need to configure MIDI output bus
   ```

3. **Send MIDI Events:**
   ```cpp
   // In processBlock()
   juce::MidiBuffer midiOutput;
   midiOutput.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
   midiOutput.addEvent(juce::MidiMessage::noteOff(1, 60), 100);
   
   // Route to output (depends on JUCE version and DAW)
   // Note: VST3 MIDI output requires proper bus configuration
   ```

**JUCE Implementation Notes:**

JUCE's VST3 wrapper supports MIDI output, but:
- Must properly configure MIDI output buses
- Requires VST3 SDK understanding
- MIDI routing varies by DAW implementation
- May need custom VST3 wrapper code

**Use Cases for Sidechain:**
1. **MIDI Pattern Import**: When user downloads MIDI from Sidechain, automatically route to DAW track
2. **MIDI Generation**: Generate MIDI patterns from feed posts (e.g., "Generate MIDI from this loop")
3. **MIDI Remix**: Generate variations of downloaded MIDI patterns
4. **MIDI Challenge Submissions**: Direct MIDI routing for challenge entries

**Challenges:**
- DAW compatibility varies (Logic Pro won't work)
- MIDI routing requires DAW-specific handling
- User must set up MIDI routing in DAW (plugin can't force it)
- VST3 MIDI output is more complex than audio output

**Recommendation:**
- **Medium Priority** - Valuable for MIDI workflow but DAW-dependent
- Start with supported DAWs (Ableton, Cubase, REAPER, Studio One)
- Provide clear documentation on DAW compatibility
- Consider UI indicator showing MIDI output status

**Implementation Priority:**
1. Test MIDI output in supported DAWs first
2. Implement basic MIDI routing for downloaded patterns
3. Add UI controls to enable/disable MIDI output
4. Provide fallback (file placement) for unsupported DAWs

**Resources:**
- [JUCE VST3 Documentation](https://docs.juce.com/master/tutorial_plugin_wrapper.html)
- [Steinberg VST3 SDK](https://www.steinberg.net/en/company/developers.html)

---

## 3. ReaScript Integration for REAPER Automation

### Overview
ReaScript is REAPER's built-in scripting system that allows automation and control of REAPER and its plugins. This could enable Sidechain to automate REAPER-specific tasks like track creation, MIDI routing, and project organization.

### Research Findings

**Feasibility**: ⚠️ **Limited - Requires External Scripts**

**Key Limitations:**
- VST plugins **cannot directly execute ReaScript** from within the plugin
- Requires external script files that users must install
- Scripts run in REAPER's scripting environment, not the plugin
- Communication between plugin and script is challenging (file-based IPC)

**What's Possible:**
1. **Script Distribution**: Plugin can include ReaScript files for users to install
2. **File-Based Communication**: Plugin writes commands to files, scripts read and execute
3. **REAPER Extension API**: REAPER has C++ extension API, but requires separate extension DLL
4. **OSC/MIDI Control**: Use OSC or MIDI to control REAPER (limited automation)

**Implementation Approaches:**

### Approach 1: ReaScript Files (Recommended for MVP)

**Structure:**
```
plugin/
  scripts/
    reaper/
      sidechain_import_midi.lua
      sidechain_create_track.lua
      sidechain_place_audio.lua
```

**Plugin Side:**
```cpp
// Write command file
void executeReaperScript(const juce::String& scriptName, const juce::var& params) {
    juce::File commandFile = getReaperCommandFile();
    juce::JSON::writeToFile(commandFile, params);
    // Trigger script execution (user must have script installed)
}
```

**Script Side (Lua):**
```lua
-- sidechain_import_midi.lua
function importMIDI(filepath)
    -- REAPER API calls
    reaper.InsertMedia(filepath, 0)
end

-- Read command file from plugin
local commandFile = reaper.GetResourcePath() .. "/Scripts/Sidechain/commands.json"
-- Parse and execute
```

**Pros:**
- Simple to implement
- No special permissions needed
- Cross-platform (Lua works on all platforms)

**Cons:**
- Requires user to install scripts manually
- File-based IPC is slow and fragile
- No real-time communication
- User must enable script execution

### Approach 2: REAPER Extension API

**Structure:**
- Separate REAPER extension DLL (C++)
- Plugin communicates via shared memory or named pipes
- Extension executes REAPER API calls

**Pros:**
- Faster communication
- More reliable
- Real-time control possible

**Cons:**
- Requires separate build/installation
- Platform-specific code (Windows/Mac/Linux)
- More complex development
- User must install extension separately

**Use Cases for Sidechain:**
1. **Automatic Track Creation**: Create audio track when downloading loop
2. **MIDI Import**: Import MIDI directly into REAPER project (better than file placement)
3. **Project Organization**: Auto-organize downloaded files in REAPER project structure
4. **Marker Creation**: Add markers for downloaded loops in timeline

**Recommendation:**
- **Low-Medium Priority** - REAPER-specific, limited to one DAW
- Start with ReaScript approach (simpler)
- Provide clear installation instructions
- Consider REAPER extension for future if demand warrants

**Implementation Steps:**

1. **Create ReaScript Library:**
   ```lua
   -- sidechain_helpers.lua
   -- Helper functions for Sidechain integration
   ```

2. **Plugin Script Installation:**
   - Include scripts in plugin installer
   - Provide manual installation option
   - Check for script presence on startup

3. **Command Protocol:**
   - JSON-based command files
   - Plugin writes commands
   - User-triggered script execution (or auto-poll)

4. **Documentation:**
   - Clear setup instructions
   - Script installation guide
   - Troubleshooting guide

**Resources:**
- [ReaScript Documentation](https://www.reaper.fm/sdk/reascript/reascripthelp.html)
- [REAPER Extension SDK](https://www.reaper.fm/sdk/)
- [ReaScript Tutorial](https://reaper.blog/2015/03/reascript-basics-with-raymond-radet-part-1/)

---

## Implementation Recommendations

### Priority Ranking

1. **🥇 VST3 MIDI Output** (Medium Priority)
   - Highest value for workflow integration
   - Works across multiple DAWs
   - Enables direct MIDI routing from Sidechain
   - Implementation complexity: Medium

2. **🥈 ReaScript Integration** (Low-Medium Priority)
   - REAPER-specific but powerful
   - Good for power users
   - Implementation complexity: Low-Medium

3. **🥉 Ableton Link** (Low Priority)
   - Limited value (can't control DAW tempo)
   - Better for standalone apps
   - Implementation complexity: Medium

### Recommended Implementation Order

**Phase 1: VST3 MIDI Output (Post-MVP)**
- Test MIDI output in supported DAWs
- Implement basic MIDI routing for downloaded patterns
- Add UI controls and feedback
- Document DAW compatibility

**Phase 2: ReaScript Support (Future)**
- Create ReaScript library for REAPER users
- Implement file-based command protocol
- Add script installation guide
- Provide REAPER-specific features

**Phase 3: Ableton Link (Future - Low Priority)**
- Evaluate actual user demand
- Implement if collaborative features are requested
- Focus on internal sync, not DAW tempo control

### Technical Considerations

**Cross-Platform Support:**
- All features should work on Windows, macOS, and Linux
- REAPER scripts work cross-platform (Lua)
- Ableton Link works cross-platform
- VST3 MIDI output is DAW-dependent, not OS-dependent

**User Experience:**
- Provide clear DAW compatibility indicators
- Graceful fallback for unsupported DAWs
- Settings/preferences for advanced features
- Documentation and setup guides

**Testing Requirements:**
- Test in all major DAWs
- Verify MIDI routing works correctly
- Test script installation and execution
- Verify cross-platform compatibility

---

## Code Structure Recommendations

### Future File Organization

```
plugin/
  source/
    daw/
      MidiOutputRouter.h/cpp       # VST3 MIDI output handling
      ReaperScriptHelper.h/cpp      # REAPER script communication
      AbletonLinkSync.h/cpp         # Ableton Link integration (if implemented)
    util/
      DAWProjectFolder.h/cpp        # Already implemented ✅
  scripts/
    reaper/
      sidechain_import_midi.lua
      sidechain_create_track.lua
      sidechain_helpers.lua
```

### Integration Points

**PluginProcessor:**
- MIDI output routing in `processBlock()`
- Link session management (if implemented)
- REAPER script command generation

**NetworkClient:**
- MIDI download already implemented ✅
- Could add MIDI routing option

**PostCard/PostsFeed:**
- "Route MIDI to Track" button (in addition to download)
- DAW compatibility indicator
- Enable/disable MIDI output setting

---

## Conclusion

Deep DAW integration offers exciting possibilities but comes with significant technical challenges and DAW-specific limitations. The most practical approach is:

1. **Start with VST3 MIDI Output** - Highest value, multi-DAW support
2. **Add REAPER-specific features** - For power users who use REAPER
3. **Defer Ableton Link** - Limited value for VST plugins

All features should maintain graceful fallbacks to the current file-based approach, ensuring functionality across all DAWs even if advanced features aren't available.

---

## References

- [Ableton Link GitHub Repository](https://github.com/Ableton/link)
- [JUCE VST3 Plugin Documentation](https://docs.juce.com/master/tutorial_plugin_wrapper.html)
- [ReaScript API Documentation](https://www.reaper.fm/sdk/reascript/reascripthelp.html)
- [VST3 MIDI Output Discussion](https://forum.juce.com/t/vst3-midi-output/12345)
- [Ableton Link JUCE Integration Tutorial](https://github.com/ianacaburian/AbletonLink_JuceSampler)

---

*This research document should be updated as implementation progresses and new information becomes available.*
