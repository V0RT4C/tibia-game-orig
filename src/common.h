#ifndef TIBIA_COMMON_H_
#define TIBIA_COMMON_H_ 1

#include "common/types/types.h"
#include "common/assert/assert.h"
#include "logging/logging.h"
#include "common/utils/utils.h"
#include "common/strings/strings.h"

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>

// Constants
// =============================================================================
// TODO(fusion): There are many constants that are hardcoded as decompilation
// artifacts. We should define them here and use when appropriate. It is not
// as simple because `std::size` is used in some cases and they're used
// essentially everywhere.

//#define MAX_NAME 30 // used with most short strings (should replace MAX_IDENT_LENGTH too)
//#define MAX_IPADDRESS 16
//#define MAX_BUDDIES 100
//#define MAX_SKILLS 25
//#define MAX_SPELLS 256
//#define MAX_QUESTS 500
#define MAX_DEPOTS 9
//#define MAX_OPEN_CONTAINERS 16
#define MAX_SPELL_SYLLABLES 10

// shm/shm.h
// =============================================================================
#include "shm/shm.h"

// time/time_utils.h
// =============================================================================
#include "time/time_utils.h"

// Stream classes (extracted to common/ modules)
#include "common/read_stream/read_stream.h"
#include "common/write_stream/write_stream.h"
#include "common/dynamic_buffer/dynamic_buffer.h"

#endif //TIBIA_COMMON_H_
