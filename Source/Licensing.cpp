#include "Licensing.h"

#include <juce_cryptography/juce_cryptography.h>

namespace
{
    // The store's catalog slug. Never change it: every issued license names it.
    constexpr auto productId = "glacier-riser";

    // Public half of apps/api LICENSE_SIGNING_KEY (RSA-2048, e = 65537). This is the DEV key:
    // replace it with the production modulus before release (see README, "Licensing").
    constexpr auto publicModulusHex =
        "d55cabb3b069bf8461cd8d330869fdbfaf901f21aa2a9f28db76288599e60bda4fc59c081591c967db728bef2d82dd40d9579e8610dfc398fdbe08c9654615943e6e70f5f07d96d44baec0b45792966b140fe05c4f474879b94376097da8a88e0c3cc30251a0d680bb674b3ee4dea7b5fdca0d8a5913f73f6670fa884d09340d8b636a0e166a42c8f983ef7c7f07175c9d3cfc6c7a40db8e9e11f2d4e6528241d33386b875c455ebf746484c8b23e1679e4e4ad05b80e915682e451fb86a2a8e8bfa498e88a6c30442856b217095310c34b69f67d14c55d2034ea23fadbe75a0e50bba7ad4b31f910427262827a31e7b6c7ad65f034f35275abae0af0320f763";

    // Production: the store's public origin. The web app proxies /api/* to apps/api.
    constexpr auto serverUrl = "http://localhost:3000";

    constexpr juce::int64 recheckIntervalMs = 7LL * 24 * 60 * 60 * 1000;

    juce::File licenseFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("GlacierRiser")
            .getChildFile ("license.json");
    }

    juce::String fingerprint()
    {
        return juce::SHA256 ((juce::SystemStats::getUniqueDeviceID() + productId).toUTF8()).toHexString();
    }

    struct Response
    {
        int status = 0;   // 0: no connection
        juce::var body;
    };

    Response post (const juce::String& path, const juce::var& body)
    {
        // WebInputStream rather than URL::createInputStream, which drops the body of 4xx replies.
        juce::WebInputStream stream (juce::URL (juce::String (serverUrl) + path)
                                         .withPOSTData (juce::JSON::toString (body, true)),
                                     true);
        stream.withExtraHeaders ("Content-Type: application/json").withConnectionTimeout (10000);

        if (! stream.connect (nullptr))
            return {};

        return { stream.getStatusCode(), juce::JSON::parse (stream.readEntireStreamAsString()) };
    }

    juce::var request (const juce::String& key, bool withMachineName = false)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("key", key);
        o->setProperty ("fingerprint", fingerprint());
        if (withMachineName)
            o->setProperty ("machineName", juce::SystemStats::getComputerName().substring (0, 100));
        return juce::var (o);
    }

    juce::String errorMessage (const Response& response)
    {
        if (response.status == 0)
            return "Couldn't reach the license server. Check your internet connection and try again.";

        const auto message = response.body["error"]["message"].toString();
        return message.isNotEmpty() ? message : "Something went wrong (" + juce::String (response.status) + "). Try again.";
    }

    /** Standard base64. (MemoryBlock::fromBase64Encoding is a JUCE-specific format.) */
    juce::MemoryBlock fromBase64 (const juce::String& text)
    {
        juce::MemoryOutputStream out;
        return juce::Base64::convertFromBase64 (out, text) ? out.getMemoryBlock() : juce::MemoryBlock();
    }

    /** The signed payload must name this product and this machine. */
    bool isValidLicense (const juce::var& stored)
    {
        const auto payload = fromBase64 (stored["payload"].toString());
        if (! Licensing::verifySignature (payload, fromBase64 (stored["signature"].toString())))
            return false;

        const auto fields = juce::JSON::parse (payload.toString());
        return fields["product"].toString() == productId && fields["fingerprint"].toString() == fingerprint();
    }
}

//==============================================================================
bool Licensing::verifySignature (const juce::MemoryBlock& payload, const juce::MemoryBlock& signature)
{
    constexpr int modulusBytes = 256;
    if (signature.getSize() != modulusBytes)
        return false;

    juce::BigInteger modulus, exponent (65537), s;
    modulus.parseString (publicModulusHex, 16);
    s.parseString (juce::String::toHexString (signature.getData(), (int) signature.getSize(), 0), 16);
    if (s >= modulus)
        return false;
    s.exponentModulo (exponent, modulus);

    // EMSA-PKCS1-v1_5: 00 01 FF..FF 00 || DigestInfo(SHA-256) || hash. Rebuilding the whole
    // block and comparing avoids parsing the decrypted signature, the classic source of bugs.
    const juce::String digestInfo = "3031300d060960864801650304020105000420";
    const auto hash = juce::SHA256 (payload).toHexString();
    const int padBytes = modulusBytes - 3 - digestInfo.length() / 2 - hash.length() / 2;
    juce::BigInteger expected;
    expected.parseString ("0001" + juce::String::repeatedString ("ff", padBytes) + "00" + digestInfo + hash, 16);

    return s == expected;
}

//==============================================================================
Licensing::Licensing()
{
   #if defined (GLACIER_SKIP_LICENSE)
    unlocked = true;
   #else
    loadFromDisk();
    recheckIfStale();
   #endif
}

Licensing::~Licensing()
{
    pool.removeAllJobs (true, 12000);
}

void Licensing::loadFromDisk()
{
    const auto stored = juce::JSON::parse (licenseFile());
    if (! isValidLicense (stored))
        return;

    const juce::ScopedLock sl (lock);
    key = stored["key"].toString();
    unlocked = true;
}

void Licensing::recheckIfStale()
{
    if (! unlocked)
        return;

    const auto stored = juce::JSON::parse (licenseFile());
    if (juce::Time::currentTimeMillis() - (juce::int64) stored["checkedAt"] < recheckIntervalMs)
        return;

    pool.addJob ([this, stored]
    {
        const auto response = post ("/api/licenses/validate", request (stored["key"].toString()));
        if (response.status != 200)
            return;   // offline or server trouble: never lock a paying user for that

        if (! (bool) response.body["active"])
        {
            licenseFile().deleteFile();
            setState (false, "This license was deactivated or revoked. Enter a key to activate again.");
            return;
        }

        if (auto* o = stored.getDynamicObject())
        {
            o->setProperty ("checkedAt", juce::Time::currentTimeMillis());
            licenseFile().replaceWithText (juce::JSON::toString (stored));
        }
    });
}

void Licensing::activate (const juce::String& keyToActivate)
{
    if (busy.exchange (true))
        return;
    ++version;

    pool.addJob ([this, keyToActivate]
    {
        const auto response = post ("/api/licenses/activate", request (keyToActivate.trim(), true));
        busy = false;

        if (response.status != 200)
        {
            setState (false, errorMessage (response));
            return;
        }

        auto* o = new juce::DynamicObject();
        o->setProperty ("key", keyToActivate.trim());
        o->setProperty ("payload", response.body["payload"]);
        o->setProperty ("signature", response.body["signature"]);
        o->setProperty ("checkedAt", juce::Time::currentTimeMillis());
        const juce::var stored (o);

        if (! isValidLicense (stored))
        {
            setState (false, "The license server sent an invalid license. Contact support.");
            return;
        }

        licenseFile().create();
        licenseFile().replaceWithText (juce::JSON::toString (stored));
        {
            const juce::ScopedLock sl (lock);
            key = keyToActivate.trim();
        }
        setState (true, {});
    });
}

void Licensing::deactivate()
{
    if (busy.exchange (true))
        return;
    ++version;

    pool.addJob ([this]
    {
        juce::String currentKey;
        {
            const juce::ScopedLock sl (lock);
            currentKey = key;
        }
        const auto response = post ("/api/licenses/deactivate", request (currentKey));
        busy = false;

        const bool alreadyFree = response.body["error"]["code"].toString() == "not_activated";
        if (response.status != 200 && ! alreadyFree)
        {
            setState (true, errorMessage (response));
            return;
        }

        licenseFile().deleteFile();
        setState (false, {});
    });
}

void Licensing::setState (bool isUnlocked, const juce::String& newError)
{
    {
        const juce::ScopedLock sl (lock);
        error = newError;
    }
    unlocked = isUnlocked;
    ++version;
}

juce::var Licensing::getSnapshot() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("unlocked", unlocked.load());
    o->setProperty ("busy", busy.load());
    {
        const juce::ScopedLock sl (lock);
        o->setProperty ("error", error);
    }
    o->setProperty ("accountUrl", juce::String (serverUrl) + "/account");
    return juce::var (o);
}
