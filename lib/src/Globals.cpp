#include "Globals.h"

std::mutex sairedis::Globals::apimutex;

bool sairedis::Globals::apiInitialized = false;
