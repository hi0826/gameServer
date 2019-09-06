#include <stdlib.h>
#include "DBManager.h"



DBManager::DBManager()
{
	DBClient = new ClientObject;
}


DBManager::~DBManager()
{
	if (DBClient) delete(DBClient);

	SQLDisconnect(hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
}

bool DBManager::ConnectDB()
{
	SQLRETURN retcode;

	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

				// Connect to data source  
				// ODBC 이름넣어주자
				retcode = SQLConnect(hdbc, (SQLWCHAR*)L"2018GameServer", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

				return true;
			}
		}
	}


	return false;
}

bool DBManager::searchDB(int id, unsigned char * packet)
{
	wchar_t SQL[NAME_LEN] = L"EXEC selectID ";
	wchar_t inputID[NAME_LEN];

	for (int i = 1; ; ++i) {
		if (packet[i] == NULL) {
			inputID[i - 1] = '\0';
			DBClient->name[i - 1] = '\0';
			break;
		}
		inputID[i - 1] = packet[i];
		DBClient->name[i - 1] = packet[i];
	}

	wchar_t* QLFn = lstrcatW(SQL, inputID);

	if (!DB(QLFn, SELECT)) {
		if (!insertDB(id, packet)) return false;
		//DB(id, QLFn, SELECT);
	}

	return true;
}

bool DBManager::insertDB(int id, unsigned char * packet)
{
	wchar_t SQL[NAME_LEN] = L"EXEC insertID ";
	wchar_t inputID[NAME_LEN];

	for (int i = 1; ; ++i) {
		if (packet[i] == NULL) {
			inputID[i - 1] = '\0';
			DBClient->name[i - 1] = '\0';
			break;
		}
		inputID[i - 1] = packet[i];
		DBClient->name[i - 1] = packet[i];
	}

	wchar_t* QLFn = lstrcatW(SQL, inputID);


	if (!DB(QLFn, INSERT)) {
		return false;
	}

	return true;
}

bool DBManager::updateDB(const ClientObject& id)
{
	wchar_t SQL[100] = L"EXEC updateID ";

	char* Query = new char[NAME_LEN];
	char* temp = new char[20];
	char dot[5] = ", ";


	strcpy(Query, id.name);

	itoa(id.hp, temp, 10);
	strcat(Query, dot);
	strcat(Query, temp);

	itoa(id.mp, temp, 10);
	strcat(Query, dot);
	strcat(Query, temp);

	itoa(id.level, temp, 10);
	strcat(Query, dot);
	strcat(Query, temp);

	itoa(id.exp, temp, 10);
	strcat(Query, dot);
	strcat(Query, temp);

	itoa(id.x, temp, 10);
	strcat(Query, dot);
	strcat(Query, temp);

	itoa(id.y, temp, 10);
	strcat(Query, dot);
	strcat(Query, temp);

	itoa(id.sp, temp, 10);
	strcat(Query, dot);
	strcat(Query, temp);

	itoa(id.skill_S, temp, 10);
	strcat(Query, dot);
	strcat(Query, temp);

	itoa(id.skill_D, temp, 10);
	strcat(Query, dot);
	strcat(Query, temp);

	cout << Query << endl;

	int requiredSize = mbstowcs(NULL, Query, 0);
	cout << requiredSize << endl;

	wchar_t* pwc = (wchar_t *)malloc((requiredSize + 1) * sizeof(wchar_t));
	int size = mbstowcs(pwc, Query, requiredSize + 1);

	printf("%S\n", pwc);

	//lstrcpyW(inputID, name);
	wchar_t* QLFn = lstrcatW(SQL, pwc);

	printf("%S\n", QLFn);

	if (!DB(QLFn, UPDATE)) {
		return false;
	}

	free(pwc);

	delete Query;
	delete temp;

	return true;
}

bool DBManager::DB(wchar_t * sqlfn, int job)
{
	SQLWCHAR szName[NAME_LEN];

	SQLINTEGER tempX, tempY, tempLevel, tempExp, tempHp, tempMp, tempSp, tempSkillS, tempSkillD;

	SQLLEN cbName = 0;
	SQLLEN cbposX = 0, cbposY = 0;
	SQLLEN cbLevel = 0, cbExp = 0, cbHp = 0, cbMp = 0, cbSp = 0, cbSkillS = 0, cbSkillD = 0;

	int retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);

	switch (job) {
	case SELECT:
		retcode = SQLExecDirect(hstmt, sqlfn, SQL_NTS);

		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLBindCol(hstmt, 1, SQL_C_CHAR, szName, NAME_LEN, &cbName);
			retcode = SQLBindCol(hstmt, 2, SQL_C_LONG, &tempX, 100, &cbposX);
			retcode = SQLBindCol(hstmt, 3, SQL_C_LONG, &tempY, 100, &cbposY);
			retcode = SQLBindCol(hstmt, 4, SQL_C_LONG, &tempLevel, 100, &cbLevel);
			retcode = SQLBindCol(hstmt, 5, SQL_C_LONG, &tempExp, 100, &cbExp);
			retcode = SQLBindCol(hstmt, 6, SQL_C_LONG, &tempHp, 100, &cbHp);
			retcode = SQLBindCol(hstmt, 7, SQL_C_LONG, &tempMp, 100, &cbMp);
			retcode = SQLBindCol(hstmt, 8, SQL_C_LONG, &tempSp, 100, &cbSp);
			retcode = SQLBindCol(hstmt, 9, SQL_C_LONG, &tempSkillS, 100, &cbSkillS);
			retcode = SQLBindCol(hstmt, 10, SQL_C_LONG, &tempSkillD, 100, &cbSkillD);
		}

		for (int i = 0; ; ++i) {
			retcode = SQLFetch(hstmt);
			if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
				HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
			}
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				wprintf(L"%d: %S %d %d %d %d %d %d %d %d %d\n",
					i + 1, szName, tempX, tempY, tempLevel, tempExp, tempHp, tempMp, tempSp, tempSkillS, tempSkillD);
				DBClient->x = tempX;
				DBClient->y = tempY;
				DBClient->level = tempLevel;
				DBClient->exp = tempExp;
				DBClient->hp = tempHp;
				DBClient->mp = tempMp;
				DBClient->sp = tempSp;
				DBClient->skill_S = tempSkillS;
				DBClient->skill_D = tempSkillD;
				break;
			}
			else
				return false;
		}
		break;

	case INSERT:
		retcode = SQLExecDirect(hstmt, sqlfn, SQL_NTS);

		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			cout << "new ID insert in DB success" << endl;
			DBClient->x = 13;
			DBClient->y = 13;
			DBClient->level = 1;
			DBClient->exp = 0;
			DBClient->hp = 10;
			DBClient->mp = 10;
			DBClient->sp = 0;
			DBClient->skill_S = 1;
			DBClient->skill_D = 1;
		}
		else
			return false;
		break;

	case UPDATE :
		retcode = SQLExecDirect(hstmt, sqlfn, SQL_NTS);

		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
			cout << "save..." << endl;
	}

	SQLCancel(hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);

	return true;
}

void DBManager::HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER  iError;
	WCHAR       wszMessage[1000];
	WCHAR       wszState[SQL_SQLSTATE_SIZE + 1];
	if (RetCode == SQL_INVALID_HANDLE)
	{
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, ++iRec, (SQLWCHAR*)wszState, &iError, (SQLWCHAR*)wszMessage, (SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)),
		(SQLSMALLINT *)NULL) == SQL_SUCCESS)
	{ // Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5))
		{
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}
