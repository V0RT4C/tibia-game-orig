#ifndef TIBIA_QUERY_H_
#define TIBIA_QUERY_H_ 1

#include "common.h"
#include "threads.h"

enum : int {
	QUERY_STATUS_OK			= 0,
	QUERY_STATUS_ERROR		= 1,
	QUERY_STATUS_FAILED		= 3,
};

struct TQueryManagerConnection{
	TQueryManagerConnection(void) : TQueryManagerConnection(kb(16)) {}
	TQueryManagerConnection(int QueryBufferSize);
	~TQueryManagerConnection(void);

	void connect(void);
	void disconnect(void);
	int write(const uint8 *Buffer, int Size);
	int read(uint8 *Buffer, int Size, int Timeout);
	bool isConnected(void){
		return this->Socket != -1;
	}

	void prepareQuery(int QueryType);
	void sendFlag(bool Flag);
	void sendByte(uint8 Byte);
	void sendWord(uint16 Word);
	void sendQuad(uint32 Quad);
	void sendString(const char *String);
	void sendBytes(const uint8 *Buffer, int Count);

	bool getFlag(void);
	uint8 getByte(void);
	uint16 getWord(void);
	uint32 getQuad(void);
	void getString(char *Buffer, int MaxLength);
	void getBytes(uint8 *Buffer, int Count);

	int executeQuery(int Timeout, bool AutoReconnect);

	int checkAccountPassword(uint32 AccountID, const char *Password, const char *IPAddress);
	int loginAdmin(uint32 AccountID, bool PrivateWorld, int *NumberOfCharacters,
			char (*Characters)[30], char (*Worlds)[30], uint8 (*IPAddresses)[4],
			uint16 *Ports, uint16 *PremiumDaysLeft);
	int loadWorldConfig(int *WorldType, int *RebootTime, int *IPAddress,
			int *Port, int *MaxPlayers, int *PremiumPlayerBuffer, int *MaxNewbies,
			int *PremiumNewbieBuffer);
	int loginGame(uint32 AccountID, char *PlayerName, const char *Password,
			const char *IPAddress, bool PrivateWorld, bool PremiumAccountRequired,
			bool GamemasterRequired, uint32 *CharacterID, int *Sex, char *Guild,
			char *Rank, char *Title, int *NumberOfBuddies, uint32 *BuddyIDs,
			char (*BuddyNames)[30], uint8 *Rights, bool *PremiumAccountActivated);
	int logoutGame(uint32 CharacterID, int Level, const char *Profession,
			const char *Residence, time_t LastLoginTime, int TutorActivities);
	int setNotation(uint32 GamemasterID, const char *PlayerName, const char *IPAddress,
			const char *Reason, const char *Comment, uint32 *BanishmentID);
	int setNamelock(uint32 GamemasterID, const char *PlayerName, const char *IPAddress,
			const char *Reason, const char *Comment);
	int banishAccount(uint32 GamemasterID, const char *PlayerName, const char *IPAddress,
			const char *Reason, const char *Comment, bool *FinalWarning, int *Days,
			uint32 *BanishmentID);
	int reportStatement(uint32 ReporterID, const char *PlayerName, const char *Reason,
			const char *Comment, uint32 BanishmentID, uint32 StatementID,
			int NumberOfStatements, uint32 *StatementIDs, int *TimeStamps,
			uint32 *CharacterIDs, const char (*Channels)[30], const char (*Texts)[256]);
	int banishIPAddress(uint32 GamemasterID, const char *PlayerName, const char *IPAddress,
			const char *Reason, const char *Comment);
	int logCharacterDeath(uint32 CharacterID, int Level, uint32 Offender,
			const char *Remark, bool Unjustified, time_t Time);
	int addBuddy(uint32 AccountID, uint32 Buddy);
	int removeBuddy(uint32 AccountID, uint32 Buddy);
	int decrementIsOnline(uint32 CharacterID);
	int finishAuctions(int *NumberOfAuctions, uint16 *HouseIDs,
			uint32 *CharacterIDs, char (*CharacterNames)[30], int *Bids);
	int excludeFromAuctions(uint32 CharacterID, bool Banish);
	int transferHouses(int *NumberOfTransfers, uint16 *HouseIDs,
			uint32 *NewOwnerIDs, char (*NewOwnerNames)[30], int *Prices);
	int cancelHouseTransfer(uint16 HouseID);
	int evictFreeAccounts(int *NumberOfEvictions, uint16 *HouseIDs, uint32 *OwnerIDs);
	int evictDeletedCharacters(int *NumberOfEvictions, uint16 *HouseIDs);
	int evictExGuildleaders(int NumberOfGuildhouses,
			int *NumberOfEvictions, uint16 *HouseIDs, uint32 *Guildleaders);
	int insertHouseOwner(uint16 HouseID, uint32 OwnerID, int PaidUntil);
	int updateHouseOwner(uint16 HouseID, uint32 OwnerID, int PaidUntil);
	int deleteHouseOwner(uint16 HouseID);
	int getHouseOwners(int *NumberOfOwners, uint16 *HouseIDs,
			uint32 *OwnerIDs, char (*OwnerNames)[30], int *PaidUntils);
	int getAuctions(int *NumberOfAuctions, uint16 *HouseIDs);
	int startAuction(uint16 HouseID);
	int insertHouses(int NumberOfHouses, uint16 *HouseIDs,
			const char **Names, int *Rents, const char **Descriptions,
			int *Sizes, int *PositionsX,int *PositionsY,int *PositionsZ,
			char (*Towns)[30], bool *Guildhouses);
	int clearIsOnline(int *NumberOfAffectedPlayers);
	int createPlayerlist(int NumberOfPlayers, const char **Names,
			int *Levels, const char (*Professions)[30], bool *NewRecord);
	int logKilledCreatures(int NumberOfRaces, const char **Names,
			int *KilledPlayers,int *KilledCreatures);
	int loadPlayers(uint32 MinimumCharacterID, int *NumberOfPlayers,
			char (*Names)[30], uint32 *CharacterIDs);
	int getKeptCharacters(uint32 MinimumCharacterID,
			int *NumberOfPlayers, uint32 *CharacterIDs);
	int getDeletedCharacters(uint32 MinimumCharacterID,
			int *NumberOfPlayers, uint32 *CharacterIDs);
	int deleteOldCharacter(uint32 CharacterID);
	int getHiddenCharacters(uint32 MinimumCharacterID,
			int *NumberOfPlayers, uint32 *CharacterIDs);
	int createHighscores(int NumberOfPlayers, uint32 *CharacterIDs,
		int *ExpPoints, int *ExpLevel, int *Fist, int *Club, int *Axe,
		int *Sword, int *Distance, int *Shielding, int *Magic, int *Fishing);
	int createCensus(void);
	int createKillStatistics(void);
	int getPlayersOnline(int *NumberOfWorlds, char (*Names)[30], uint16 *Players);
	int getWorlds(int *NumberOfWorlds, char (*Names)[30]);
	int getServerLoad(const char *World, int Period, int *Data);

	int insertPaymentDataOld(uint32 PurchaseNr, uint32 ReferenceNr,
			const char *FirstName, const char *LastName, const char *Company,
			const char *Street, const char *Zip, const char *City, const char *Country,
			const char *State, const char *Phone, const char *Fax, const char *EMail,
			const char *PaymentMethod, uint32 ProductID, const char *Registrant,
			uint32 AccountID, uint32 *PaymentID);
	int addPaymentOld(uint32 AccountID, const char *Description,
			uint32 PaymentID, int Days, int *ActionTaken);
	int cancelPaymentOld(uint32 PurchaseNr, uint32 ReferenceNr,
			uint32 AccountID, bool *IllegalUse, char *EMailAddress);

	int insertPaymentDataNew(uint32 PurchaseNr, uint32 ReferenceNr,
			const char *FirstName, const char *LastName, const char *Company,
			const char *Street, const char *Zip, const char *City, const char *Country,
			const char *State, const char *Phone, const char *Fax, const char *EMail,
			const char *PaymentMethod, uint32 ProductID, const char *Registrant,
			const char *PaymentKey, uint32 *PaymentID);
	int addPaymentNew(const char *PaymentKey, uint32 PaymentID,
			int *ActionTaken, char *EMailReceiver);
	int cancelPaymentNew(uint32 PurchaseNr, uint32 ReferenceNr,
			const char *PaymentKey, bool *IllegalUse, bool *Present,
			char *EMailAddress);

	// DATA
	// =================
	int BufferSize;
	uint8 *Buffer;
	TReadBuffer ReadBuffer;
	TWriteBuffer WriteBuffer;
	int Socket;
	bool QueryOk;
};

struct TQueryManagerConnectionPool{
	TQueryManagerConnectionPool(int Connections);
	void init(void);
	void exit(void);
	TQueryManagerConnection *getConnection(void);
	void releaseConnection(TQueryManagerConnection *Connection);

	// DATA
	// =================
	int NumberOfConnections;
	TQueryManagerConnection *QueryManagerConnection;
	bool *QueryManagerConnectionFree;
	Semaphore FreeQueryManagerConnections;
	Semaphore QueryManagerConnectionMutex;
};

struct TQueryManagerPoolConnection{
	TQueryManagerPoolConnection(TQueryManagerConnectionPool *Pool);
	~TQueryManagerPoolConnection(void);

	// TODO(fusion): I don't know if this was the indended way to access this
	// structure but it is only a small wrapper and accessing members directly
	// would be too verbose, specially when it is also named `QueryManagerConnection`.

	TQueryManagerConnection *operator->(void){
		return this->QueryManagerConnection;
	}

	operator bool(void) const {
		return this->QueryManagerConnection != NULL;
	}

	// DATA
	// =================
	TQueryManagerConnectionPool *QueryManagerConnectionPool;
	TQueryManagerConnection *QueryManagerConnection;
};

void SetQueryManagerLoginData(int Type, const char *Data);

#endif //TIBIA_QUERY_H_
