#!/bin/bash
# Fix Visual Studio project after Projucer regenerates it
# This adds source files that Projucer forgets to include

VCXPROJ="plugin/Builds/VisualStudio2022/Sidechain_VST3.vcxproj"

echo "Fixing $VCXPROJ..."

# Check if the empty ItemGroup exists
if grep -q "<ItemGroup/>" "$VCXPROJ"; then
    echo "Found empty ItemGroup, adding source files..."

    # Replace the empty ItemGroup with one containing all source files
    sed -i 's|<ItemGroup/>|<ItemGroup>\n    <ClCompile Include="..\\..\\Source\\PluginProcessor.cpp"/>\n    <ClCompile Include="..\\..\\Source\\PluginEditor.cpp"/>\n    <ClCompile Include="..\\..\\Source\\PostsFeedComponent.cpp"/>\n    <ClCompile Include="..\\..\\Source\\ProfileSetupComponent.cpp"/>\n    <ClCompile Include="..\\..\\Source\\AudioCapture.cpp"/>\n    <ClCompile Include="..\\..\\Source\\NetworkClient.cpp"/>\n    <ClInclude Include="..\\..\\Source\\PluginProcessor.h"/>\n    <ClInclude Include="..\\..\\Source\\PluginEditor.h"/>\n    <ClInclude Include="..\\..\\Source\\PostsFeedComponent.h"/>\n    <ClInclude Include="..\\..\\Source\\ProfileSetupComponent.h"/>\n    <ClInclude Include="..\\..\\Source\\AudioCapture.h"/>\n    <ClInclude Include="..\\..\\Source\\NetworkClient.h"/>\n  </ItemGroup>|' "$VCXPROJ"

    echo "✅ Source files added to project"
else
    echo "⚠️  Empty ItemGroup not found - project may already be fixed or structure changed"
fi
