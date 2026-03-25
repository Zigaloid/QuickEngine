#pragma once

#ifdef _WIN32
#include "WinBerkeleySocket.h"
#endif

#ifdef _WIN32
using BerkeleySocket = WinBerkeleySocket;
#endif