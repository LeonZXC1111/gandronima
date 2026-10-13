#pragma once
//
// movement.h - movement feature module (auto bhop first).
//
// Installs a MinHook detour on the game's "create move" / input-processing
// function so we can edit the outgoing CUserCmd before it is sent. MinHook is
// already initialized by kiero::init(), so we only create/enable our own hook.
//
namespace movement
{
    // Resolve the target function and install the detour. Call once, after the
    // DX11 hook is up and client.dll is loaded. Returns false if the signature
    // could not be resolved (e.g. offsets/sig stale for this game build).
    bool initialize();

    // Remove the detour. Call on unload.
    void shutdown();

    // True once the CreateMove detour is installed (diagnostic for the menu).
    bool is_hooked();

    // How many times the detour has actually fired (diagnostic).
    unsigned long hook_calls();
}
