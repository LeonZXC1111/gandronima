#pragma once
//
// debuglog.h - debug console + crash log + logging helpers.
//
// In Debug builds this opens a CMD console and installs an unhandled-exception
// filter that writes a crash log (module + offset of the faulting address) to
// a file next to the game, so a crash leaves a trace instead of vanishing.
//
namespace dbg
{
    void init();                       // alloc console (debug) + crash handler
    void log(const char* fmt, ...);    // printf to console + append to log file
}
