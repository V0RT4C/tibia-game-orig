#include "challenge/challenge_ban.h"

#include <ctime>
#include <map>
#include <mutex>

// AccountID -> ban expiry (unix timestamp)
static std::map<uint32, time_t> BanList;
static std::mutex BanMutex;

void challenge_ban_add(uint32 AccountID, int DurationMinutes) {
	std::lock_guard<std::mutex> Lock(BanMutex);
	BanList[AccountID] = time(NULL) + (DurationMinutes * 60);
}

int challenge_ban_check(uint32 AccountID) {
	std::lock_guard<std::mutex> Lock(BanMutex);
	auto It = BanList.find(AccountID);
	if (It == BanList.end()) return 0;

	int Remaining = (int)(It->second - time(NULL));
	if (Remaining <= 0) {
		BanList.erase(It);
		return 0;
	}
	return Remaining;
}

void challenge_ban_cleanup(void) {
	std::lock_guard<std::mutex> Lock(BanMutex);
	time_t Now = time(NULL);
	for (auto It = BanList.begin(); It != BanList.end(); ) {
		if (It->second <= Now) {
			It = BanList.erase(It);
		} else {
			++It;
		}
	}
}
