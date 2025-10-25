# Windows Development Notes

## Critical: Visual Studio Project Fix

**Problem**: Projucer doesn't properly add source files to the Visual Studio project, causing none of your code changes to compile.

**Symptoms**:
- Code changes don't appear in the plugin after rebuilding
- Old/grey UI shows instead of your new colors
- No logout buttons appear

**Solution**: The Makefile now automatically fixes this when you run `make plugin` or `make plugin-project`.

### How It Works
The `fix-vcxproj.sh` script runs after Projucer and adds all source files to the Visual Studio project.

### Manual Fix
If the automatic fix doesn't work:

1. Open `plugin/Builds/VisualStudio2022/Sidechain_VST3.vcxproj`
2. Find the empty `<ItemGroup/>` near the end (around line 271)
3. Replace it with:
```xml
<ItemGroup>
  <ClCompile Include="..\..\Source\PluginProcessor.cpp"/>
  <ClCompile Include="..\..\Source\PluginEditor.cpp"/>
  <ClCompile Include="..\..\Source\PostsFeedComponent.cpp"/>
  <ClCompile Include="..\..\Source\ProfileSetupComponent.cpp"/>
  <ClCompile Include="..\..\Source\AudioCapture.cpp"/>
  <ClCompile Include="..\..\Source\NetworkClient.cpp"/>
  <ClInclude Include="..\..\Source\PluginProcessor.h"/>
  <ClInclude Include="..\..\Source\PluginEditor.h"/>
  <ClInclude Include="..\..\Source\PostsFeedComponent.h"/>
  <ClInclude Include="..\..\Source\ProfileSetupComponent.h"/>
  <ClInclude Include="..\..\Source\AudioCapture.h"/>
  <ClInclude Include="..\..\Source\NetworkClient.h"/>
</ItemGroup>
```

## Build Tips

### Standard Build
```bash
make plugin-install  # This automatically runs the fix
```

### Adding New Source Files
1. Add the file to `plugin/Source/`
2. Open `plugin/Sidechain.jucer` in Projucer GUI
3. Add the file to the project in the File Explorer pane
4. Save the project
5. Run `make plugin` - the fix script will automatically add it to the VS project
6. Rebuild

## Why This Happens
JUCE/Projucer has a bug where it generates an empty `<ItemGroup/>` in the vcxproj instead of populating it with source files. This is a known issue with certain JUCE project configurations.
