#pragma once

#include <AssetData.h>
#include <juce_graphics/juce_graphics.h>

// The phoenix mark, embedded from Assets/phoenix.png (silver on transparent).
namespace ember
{
    inline juce::Image loadPhoenix()
    {
        return juce::ImageFileFormat::loadFrom (AssetData::phoenix_png, (size_t) AssetData::phoenix_pngSize);
    }

    inline void drawPhoenix (juce::Graphics& g, const juce::Image& phoenix, juce::Rectangle<float> area, float alpha = 1.0f)
    {
        if (! phoenix.isValid())
            return;

        g.setOpacity (alpha);
        g.drawImage (phoenix, area, juce::RectanglePlacement::centred);
    }
}
