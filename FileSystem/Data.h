#pragma once

#include <cstring>
#include "semaphoreMacros.h"
#include <mutex>

//class Data - klasa koja predstavlja klaster u vidu klastera podataka direktorijuma, u kom se nalazi 64 ulaza od po 32B(20B koji se aktivno koriste)


class Data {
public:
	
	static std::mutex m;
	struct Entry {
		char name[8] = { 0 };
		char extension[3] = { 0 };
		char unusedByte = 0;
		int indexClusterNo = 0;														//ulaz koji sadrzi index prvog nivoa za zadati fajl, = 0 ako je fajl prazan
		int fileSize = 0;																//velicina fajla u B
		char unusedBytes[12] = { 0 };
	};
	Data();
	~Data();
	bool compareFileName(char* fName, int entry);									//vraca true ako su isti, fName je puno ime fajla, sa ekstenzijom
	char* getFName(int entry);														//cupa ime iz ulaza podataka direktorijuma(prvih 8 bajtova)
	char* getFExtension(int entry);													//isto za ove ostale get funkcije, sa odgovarajucim brojem bajtova
	int getIndexClusterNo(int entry);
	int getLastEntryNo();
	int getFileSize(int entry);
	bool isEntryEmpty(int entry);
	void setFullFileName(int entry, char* fullFileName);
												//isto za ove ostale get funkcije, sa odgovarajucim brojem bajtova
	void setIndexClusterNo(int entry, int indexClusterNo);
	void setFileSize(int entry, int fileSize);
	int findEntry(int indexClusterNo);
	
	
	Entry table[64];
};

