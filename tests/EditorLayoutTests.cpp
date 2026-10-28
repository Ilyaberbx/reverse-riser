#include "helpers/test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

TEST_CASE ("editor layout holds at odd sizes", "[editor][layout]")
{
    const auto [width, height] = GENERATE (std::pair { 380, 520 }, std::pair { 520, 720 }, std::pair { 433, 911 },
        std::pair { 700, 1400 }, std::pair { 1200, 700 }, std::pair { 1800, 1000 }, std::pair { 611, 523 });
    CAPTURE (width, height);

    runWithinPluginEditor ([&] (PluginProcessor& plugin) {
        auto* editor = plugin.getActiveEditor();
        editor->setSize (width, height);
        const auto bounds = editor->getLocalBounds();

        std::vector<juce::Component*> controls;
        for (auto* child : editor->getChildren())
            if (child->isVisible() && dynamic_cast<juce::ResizableCornerComponent*> (child) == nullptr)
                controls.push_back (child);

        juce::Slider* largest = nullptr;
        for (auto* c : controls)
        {
            CAPTURE (c->getTitle(), c->getName(), c->getBounds().toString());
            CHECK (bounds.contains (c->getBounds()));
            CHECK (! c->getBounds().isEmpty());

            if (auto* slider = dynamic_cast<juce::Slider*> (c))
            {
                CHECK (juce::jmin (slider->getWidth(), slider->getHeight()) >= 44);
                if (largest == nullptr || slider->getWidth() * slider->getHeight() > largest->getWidth() * largest->getHeight())
                    largest = slider;
            }
        }

        // nothing overlaps (sliders and their name labels sit in separate rows)
        for (size_t i = 0; i < controls.size(); ++i)
            for (size_t j = i + 1; j < controls.size(); ++j)
            {
                CAPTURE (controls[i]->getTitle(), controls[i]->getBounds().toString(), controls[j]->getTitle(), controls[j]->getBounds().toString());
                CHECK (! controls[i]->getBounds().intersects (controls[j]->getBounds()));
            }

        // Mix is the hero at every size
        REQUIRE (largest != nullptr);
        CHECK (largest->getTitle() == "Mix");
    });
}
