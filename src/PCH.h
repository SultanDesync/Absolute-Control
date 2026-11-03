#pragma once

#include <RE/Starfield.h>
#include <SFSE/SFSE.h>

#include <Windows.h>

// wingdi.h defines ERROR, which breaks REX::ERROR.
#ifdef ERROR
#undef ERROR
#endif
