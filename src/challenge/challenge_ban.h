#ifndef GAMESERVER_CHALLENGE_BAN_H
#define GAMESERVER_CHALLENGE_BAN_H

#include "common.h"

// Add a temporary ban for an account. Duration is in minutes.
void challenge_ban_add(uint32 AccountID, int DurationMinutes);

// Check if an account is currently temp-banned.
// Returns remaining seconds if banned, 0 if not banned.
int challenge_ban_check(uint32 AccountID);

// Remove expired bans (call periodically).
void challenge_ban_cleanup(void);

#endif // GAMESERVER_CHALLENGE_BAN_H
