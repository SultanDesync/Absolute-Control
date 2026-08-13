#pragma once

#include <RE/Starfield.h>
#include <SFSE/SFSE.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <intrin.h>
#ifdef ERROR
#undef ERROR
#endif

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
