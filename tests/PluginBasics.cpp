#include "helpers/test_helpers.h"
#include <PluginProcessor.h>
#include <Theme.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

TEST_CASE ("one is equal to one", "[dummy]")
{
    REQUIRE (1 == 1);
}

TEST_CASE ("Plugin instance", "[instance]")
{
    PluginProcessor testPlugin;

    SECTION ("name")
    {
        CHECK_THAT (testPlugin.getName().toStdString(),
            Catch::Matchers::Equals ("Reverse Riser"));
    }

    SECTION ("program name")
    {
        // Steinberg's VST3 validator fails plugins whose programs have no name
        CHECK (testPlugin.getProgramName (0).isNotEmpty());
    }
}


#ifdef PAMPLEJUCE_IPP
    #include <ipp.h>

TEST_CASE ("IPP version", "[ipp]")
{
    #if defined(__APPLE__)
        // macOS uses 2021.9.1 from pip wheel (only x86_64 version available)
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2021.9.1 (r0x7e208212)"));
    #else
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2026.0.0 (r0xa7ad6ebc)"));
    #endif
}
#endif

TEST_CASE ("Generated images are embedded", "[assets]")
{
    const auto& i = Theme::images();
    for (const auto* image : { &i.heroVortex, &i.knobCap, &i.logoMark, &i.glassLight, &i.glassDark })
        CHECK (image->isValid());

    SECTION ("surfaces drawn into circles are square")
    {
        CHECK (i.heroVortex.getWidth() == i.heroVortex.getHeight());
        CHECK (i.knobCap.getWidth() == i.knobCap.getHeight());
        CHECK (i.logoMark.getWidth() == i.logoMark.getHeight());
    }

    SECTION ("cut-outs keep their alpha")
    {
        CHECK (i.knobCap.hasAlphaChannel());
        CHECK (i.logoMark.hasAlphaChannel());
    }
}
