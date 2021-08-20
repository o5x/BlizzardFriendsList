#include <iostream>
#include <string>
#include <list>
#include <fstream>
#include <vector>

#include <stdio.h>
#include <Windows.h>
#include <winver.h>
#include <psapi.h>
#include <TlHelp32.h>

// Open namespaces
using namespace std;

// Structures
enum class Status
{
	online = 0,
	away = 1,
	busy = 2,
	NC,
	offline = 4
};

// used to sort
typedef struct
{
	uint32_t accountImplAddr;
	string name;
	Status status;
	bool favourite;
} BlizzardUser;

typedef struct
{
	const char* id;
	const char* description;
}BlizzardClient;


// Global variables
const char* status[] =
{
	"online",
	"away",
	"busy",
	"",
	"offline"
};

const BlizzardClient client[] =
{
	{"WoW", "World of Warcraft"},
	{"S2", "StarCraft 2" },
	{"D3", "Diablo 3" },
	{"WTCG", "Hearthstone" },
	{"App", "Battle.net Desktop App" },
	{"BSAp", "Battle.net Mobile App" },
	{"Hero", "Heroes of the Storm" },
	{"Pro", "Overwatch" },
	{"CLNT", "	"},
	{"S1", "StarCraft : Remastered"},
	{"DST2", "Destiny 2"},
	{"VIPR", "Call of Duty : Black Ops 4"},
	{"ODIN", "Call of Duty : Modern Warfare"},
	{"LAZR", "Call of Duty : Modern Warfare 2"},
	{"ZEUS", "Call of Duty : Black Ops Cold War"},
	{"W3", "Warcraft III : Reforged"},
	{"RTRO", "Blizzard Arcade Collection"},
	{"WLBY", "Crash Bandicoot 4"},
	{"OSI", "Diablo II : Resurrected"},
};

// Open executable function
HANDLE openExecutable(string _FileName)
{
	HANDLE myhProcess;
	PROCESSENTRY32 mype;
	mype.dwSize = sizeof(PROCESSENTRY32);
	BOOL mybRet;
	myhProcess = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	mybRet = Process32First(myhProcess, &mype);

	LPCSTR FileNamelp = _FileName.c_str();

	wcout << "List of \"" << FileNamelp << "\" processes :" << endl;

	vector<DWORD> PIDs;

	while (mybRet)
	{
		if (!lstrcmp(FileNamelp, mype.szExeFile))
		{
			wcout << " - " << FileNamelp << " PID " << mype.th32ProcessID << " parent PID = " << mype.th32ParentProcessID << endl;

			// if multiple battlenet has same parent, open it
			if (count(PIDs.begin(), PIDs.end(), mype.th32ParentProcessID))
			{
				cout << "2 processes with same parent, opening PID " << mype.th32ParentProcessID << endl;
				HANDLE Handle = OpenProcess(PROCESS_ALL_ACCESS, 0, mype.th32ParentProcessID);

				if (Handle)
				{
					printf("Handler opened PID %d 0x%x\n", mype.th32ParentProcessID, mype.th32ParentProcessID);
					return Handle;
				}
			}

			PIDs.push_back(mype.th32ParentProcessID);
		}

		mybRet = Process32Next(myhProcess, &mype);
	}

	cout << "Executable \"" << FileNamelp << "\" not Found !" << endl;
	exit(0);
}

// Function to compare users
bool compare_users(BlizzardUser& first, BlizzardUser& second)
{
	if (first.favourite != second.favourite)
	{
		return first.favourite;
	}

	Status one = first.status;
	Status two = second.status;

	if (one == Status::busy) one = Status::online;
	if (two == Status::busy) two = Status::online;

	if (one != two)
	{
		return one < two;
	}

	unsigned int i = 0;

	while ((i < first.name.length()) && (i < second.name.length()))
	{
		if (tolower(first.name[i]) < tolower(second.name[i])) return true;
		else if (tolower(first.name[i]) > tolower(second.name[i])) return false;
		++i;
	}
	return (first.name.length() < second.name.length());
}

// function to print date
void printdate(int64_t ts)
{
	if (ts == 0)
	{
		printf(" |                    ");
	}
	else
	{
		time_t t = ts / 1000000;
		tm tm;
		localtime_s(&tm, &t);
		printf(" | %02d:%02d:%02d %02d/%02d/%04d", tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
	}
}

// function to put string in buffer (2 way blizzard implementation)
char* formatBlizzardString(HANDLE Handle, char* obuffer, char* ibuffer, uint32_t strOffset)
{
	uint32_t stringLen = *(uint32_t*)(ibuffer + strOffset + 0x10);
	uint32_t allocLen = *(uint32_t*)(ibuffer + strOffset + 0x14);

	if (stringLen == 0 || stringLen > 0xFF || allocLen > 0xFF)
	{
		obuffer[0] = '\0';
		return obuffer;
	}

	if (allocLen == 0xF)
	{
		memcpy(obuffer, ibuffer + strOffset, stringLen);
	}
	else
	{
		ReadProcessMemory(Handle, (char*)*(uint32_t*)(ibuffer + strOffset), obuffer, stringLen, NULL);
	}

	obuffer[stringLen] = '\0';
	return obuffer;
}

// Entry point
int main(int argc, char** argv)
{
	cout << "  __  __                   _____       " << endl;
	cout << " |  \\/  |  ___  _ __ ___  | ____|__  __" << endl;
	cout << " | |\\/| | / _ \\| '_ ` _ \\ |  _|  \\ \\/ /" << endl;
	cout << " | |  | ||  __/| | | | | || |___  >  < " << endl;
	cout << " |_|  |_| \\___||_| |_| |_||_____|/_/\\_\\" << endl;
	cout << " Blizzard Friend Explorer " << " by Arrol https://arrol.fr/ " << endl;
	cout << " Compiled on " << __DATE__ << " at " << __TIME__ << endl << endl;

	SetConsoleOutputCP(CP_UTF8);

	HANDLE Handle = openExecutable("Battle.net.exe");

	MEMORY_BASIC_INFORMATION mbi;
	uint64_t currentAddr = 0;
	list<BlizzardUser> users;

	char accountBuffer[276];
	char accountDataBuffer[0xff];
	char accountSubData[0xFF];
	char temp[0xFF];

	printf("\nThe not processed status in memory : \n\n");

	printf("    MemAddr |         ID           |        Name          |          Status\n");
	while (VirtualQueryEx(Handle, (char*)currentAddr, &mbi, sizeof(mbi)))
	{
		if (mbi.Protect & PAGE_READWRITE)
		{
			char* buffer = new char[mbi.RegionSize + 1];

			ReadProcessMemory(Handle, (char*)currentAddr, buffer, mbi.RegionSize, NULL);

			for (size_t i = 0; i < mbi.RegionSize - 8; i += 4)
			{
				///////////////////////////////////////////////////////////////////////////////////////////////////

				char* displayBuffer = buffer + i;
				if (
					*(uint32_t*)(displayBuffer + 0x04) == 0 &&
					*(uint32_t*)(displayBuffer + 0x08) == 1023 &&
					*(uint32_t*)(displayBuffer + 0x0C) == 0
					)
				{
					char strID[0xFF];
					ReadProcessMemory(Handle, (char*)*(uint32_t*)(displayBuffer + 0x10), accountBuffer, 0x18, NULL);
					formatBlizzardString(Handle, strID, accountBuffer, 0x00);
					if (strID[0] != '2')
					{
						continue;
					}
					char strName[0xFF];
					char strApp[0xFF];
					char strGameStatus[0xFF];
					char strStatus[0xFF];
					ReadProcessMemory(Handle, (char*)*(uint32_t*)(displayBuffer + 0x14), accountBuffer, 0x18, NULL);
					formatBlizzardString(Handle, strName, accountBuffer, 0x00);
					ReadProcessMemory(Handle, (char*)*(uint32_t*)(displayBuffer + 0x18), accountBuffer, 0x18, NULL);
					formatBlizzardString(Handle, strApp, accountBuffer, 0x00);
					ReadProcessMemory(Handle, (char*)*(uint32_t*)(displayBuffer + 0x1C), accountBuffer, 0x18, NULL);
					formatBlizzardString(Handle, strGameStatus, accountBuffer, 0x00);
					ReadProcessMemory(Handle, (char*)*(uint32_t*)(displayBuffer + 0x20), accountBuffer, 0x18, NULL);
					formatBlizzardString(Handle, strStatus, accountBuffer, 0x00);

					if (!strcmp(strStatus, "offline"))
					{
						continue;
					}

					printf(" - %08x", (uint32_t)(currentAddr + i));
					printf(" | %-20s", strID);
					printf(" | %-20s", strName);
					//printf(" | %-20s", strStatus);
					//printf(" | %-20s", strApp);
					//printf(" | %-20s", strGameStatus);

					for (size_t i = 0; i < sizeof(client) / sizeof(BlizzardClient); i++)
					{
						if (!strcmp(strApp, client[i].id))
						{
							printf(" | %s : ", client[i].description);
						}
					}

					printf("%s", strGameStatus);

					// debug
					//ReadProcessMemory(Handle, (char*)*(uint32_t*)(displayBuffer + 0x24), accountBuffer, 0x18, NULL);
					//printf(" | %-20s", formatBlizzardString(Handle, temp, accountBuffer, 0x00));
					/*for (size_t j = 0x28; j <= 0x34 + 3; j += 4)
					{
						printf(" %08x ", *(uint32_t*)((char*)displayBuffer + j));
					}*/

					printf("\n");
				}

				///////////////////////////////////////////////////////////////////////////////////////////////////

				char* accountBufferPtr = buffer + i;

				if (*(uint32_t*)(accountBufferPtr + 0x1C) != 0 &&	// skip if not friend
					*(uint32_t*)(accountBufferPtr + 0xB8) != 0 &&	// skip if self
					*(uint32_t*)(accountBufferPtr + 0x2C) == 7 &&	// pattern scan begin	
					*(uint32_t*)(accountBufferPtr + 0x30) == 8 &&
					*(uint32_t*)(accountBufferPtr + 0x48) == 0 &&
					*(uint32_t*)(accountBufferPtr + 0x54) == 0 &&
					*(uint32_t*)(accountBufferPtr + 0x58) == 1 &&
					*(uint32_t*)(accountBufferPtr + 0x5C) == 0 &&
					*(uint32_t*)(accountBufferPtr + 0x60) == 0 &&	// pattern scan end
					*(uint32_t*)(accountBufferPtr + 0x88) <= 4		// has valid onlineStatus
					)
				{
					// Add user to list
					BlizzardUser u = {
						(uint32_t)(currentAddr + i),
						string(accountBufferPtr + 0x8C),
						(Status)(accountBufferPtr[0x88] & 0xFF),
						(bool)accountBufferPtr[0xC0]
					};
					users.push_back(u);
				}
			}
			delete[] buffer;
		}
		currentAddr += mbi.RegionSize;
	}

	// Sort users using same blizzard constraints
	users.sort(compare_users);

	printf("\nThe %d Friends we found on memory :\n\n", (int)users.size());
	printf("          Battletag     |     Name         |  MemAddr  |  Status |   Last Seen Mobile  |  Last Seen Desktop  |  SessId  |        Friend Note\n");
	for (auto it = users.begin(); it != users.end(); ++it)
	{
		ReadProcessMemory(Handle, (char*)(it->accountImplAddr), accountBuffer, 275, NULL);
		ReadProcessMemory(Handle, (char*)*(uint32_t*)(accountBuffer + 0x68), accountDataBuffer, 0xFE, NULL);
		ReadProcessMemory(Handle, (char*)*(uint32_t*)(accountDataBuffer + 0x08), accountSubData, 0xFE, NULL);

		// Display name, favourite and status
		if (it->favourite)
			printf(" \xe2\x99\xa5");
		else
			printf(" -");

		printf(" %-20s", formatBlizzardString(Handle, temp, accountSubData, 0x00));
		printf(" | %-16s", formatBlizzardString(Handle, temp, accountBuffer, 0x8C));
		printf(" | %08x ", (uint32_t)it->accountImplAddr);
		printf(" | %-7s", status[(int)it->status]);

		// Display timestamps
		int64_t lastSeenDesktopTimestamp = *(int64_t*)(accountBuffer + 0x78);
		int64_t lastSeenMobileTimestamp = *(int64_t*)(accountBuffer + 0x80);
		printdate(lastSeenMobileTimestamp);
		printdate(lastSeenDesktopTimestamp);

		// Display session ID
		printf(" | %08x", *(uint32_t*)(accountBuffer + 0xA8));

		// show serveral bytes (debug)
		/*for (size_t i = 0x04; i <= 0x08 + 3; i+= 4)
		{
			printf(" %08x ", *(uint32_t*)((char*)accountBuffer + i));
		}*/

		// Show friend note
		printf(" | %-7s", formatBlizzardString(Handle, temp, accountBuffer, 0xC8));
		cout << "\n";
	}

	CloseHandle(Handle);

	return 0;
}