#pragma once




#include <Windows.h>
#include "globals.h"  // Enthält bool globals::show



void handleMenuToggle() {
    if (GetAsyncKeyState(VK_INSERT) & 1) {
        globals::show = !globals::show;
    }
}



