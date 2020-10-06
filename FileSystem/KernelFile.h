#pragma once

#include "fs.h"
#include "semaphoreMacros.h"
#include "KernelFS.h"

#include <condition_variable>

class KernelFile
{
public:
	struct FileDataBlock {
		char block[2048] = { 0 };

		char getByte(int entry) {
			return block[entry];
		}

		void setByte(int entry, char value) {
			block[entry] = value;
		}
		void resetWholeBlock() {
			for (int i = 0; i < 2048; i++) {
				block[i] = 0;
			}
		}
	};

	struct FileOpenedCache {
		int clusterNoFile1Lvl = 0;
		Index* file1Lvl = nullptr;

		int clusterNoFile2Lvl = 0;
		Index* file2Lvl = nullptr;

		int clusterNoFileData = 0;
		FileDataBlock* fileData = nullptr;
	};

	KernelFile(int clusterNo, unsigned long size);
	~KernelFile();

	std::mutex m;
	static HANDLE mutexRW;

	char write(BytesCnt, char* buffer, File* addr);
	BytesCnt read(BytesCnt, char* buffer, File* addr);
	char seek(BytesCnt, File*);
	BytesCnt filePos(File* addr);
	char eof(File* addr);
	BytesCnt getFileSize(File* addr);
	char truncate(File* addr);
	void setFileToCloseAddr(File* addr);

	int procesReadingNo;
	int procesWritingNo;

	HANDLE writingMutex;
	HANDLE readingMutex;

	//std::condition_variable cvCanOpen;
private:
	int isFileAvilable(File* addr, std::vector<KernelFS::ProcesFCB>::iterator* it2);

	friend class KernelFS;
	int clusterNo;								//ovo moze da bude claster u kom se nalazi index fajla prvog nivoa, ne "moze da bude" vec mi se cini da sam tako i uradio, tako da je pravilnije reci "jeste"
	unsigned long size;
	const unsigned long maxFileSize = 2048;

	FileOpenedCache fileOpenedCache;

	File* fileToCloseAddr = nullptr;				//ono mi je potrebno da bih imao da poredim koji fajl cu izbaciti iz vektora, jer nemam nista drugo za uporediti u destruktoru, jer njemu nije moguce proslediti parametre, tako da cu gledati kao obavezno da destruktor fajla pozove funckiju
													// koja ce postaviti promenljivu fileToCloseName, pa tek onda delete myImpl;
};

