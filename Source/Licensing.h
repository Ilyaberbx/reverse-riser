#pragma once

#include <juce_core/juce_core.h>

/**
    License gate, shared by every GlacierRiser instance in the process (SharedResourcePointer).

    The store (ice-cold-mono apps/api) signs a license for this machine on activation:
    RSA PKCS#1 v1.5 over SHA-256 of a JSON payload { v, product, licenseId, fingerprint, issuedAt }.
    It is kept in <app data>/GlacierRiser/license.json and verified locally on every load, so the
    network is only needed to activate, deactivate, and for the weekly revocation re-check.

    Network calls run on a background thread; the editor polls getVersion() to notice changes.
*/
class Licensing
{
public:
    Licensing();
    ~Licensing();

    bool isUnlocked() const noexcept { return unlocked.load(); }
    int getVersion() const noexcept { return version.load(); }

    /** { unlocked, busy, error, accountUrl } for the UI. */
    juce::var getSnapshot() const;

    void activate (const juce::String& key);
    void deactivate();

    /** Exposed for tests: true if `signature` is the store's signature over `payload`. */
    static bool verifySignature (const juce::MemoryBlock& payload, const juce::MemoryBlock& signature);

private:
    void loadFromDisk();
    void recheckIfStale();
    void setState (bool isUnlocked, const juce::String& error);

    std::atomic<bool> unlocked { false }, busy { false };
    std::atomic<int> version { 0 };

    mutable juce::CriticalSection lock;
    juce::String key, error;   // guarded by lock

    juce::ThreadPool pool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Licensing)
};
