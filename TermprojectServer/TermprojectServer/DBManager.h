#pragma once
#include "includes.h"
#include "protocol.h"
#include "Objects.h"

#define NAME_LEN 50

#define SELECT 0
#define INSERT 1
#define UPDATE 2
;
class DBManager
{
public:
	DBManager();
	~DBManager();

	bool ConnectDB();
	bool searchDB(int id, unsigned char* packet);
	bool insertDB(int id, unsigned char* packet);
	bool updateDB(const ClientObject& id);
	bool DB(wchar_t *sqlfn, int job);
	void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);

	ClientObject* getClientObj() { return DBClient; }

private :

	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt = 0;
	ClientObject* DBClient;
};

