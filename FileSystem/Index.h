#pragma once

#include "Data.h"
#include "part.h"
#include "file.h"
#include "bitVector.h"
#include "FileFamilyTree.h"


class Index {
public:
	Index();
	~Index();

	char doesExist(char* fname, Partition* part, FileFamilyTree* fft = nullptr); //int* cluserNo = nullptr, unsigned long* size = nullptr);	//vraca takodje i broj klastera u kom se fajl nalazi ako je pronadjen, kao i velicinu. Ako se pozovu bez zadnja 2 argumenta ne vraca ta dva argumenta, vec samo proverava da li fajl postoji. Ako ne postoji a zeli se da se vrati clusterNo i size, za njih ce se vratiti -1.
	int readRootDir(Partition *part);																		//poziva je fs.h, istoimena metoda, to tek treba da implementiras
	int firstFreeIndexEntry();															
	int getLastEntryNo();
	int findEntry(int clusterNo);																			//nalazi u kom entry-ju tabele se nalazi zadati broj klastera za data direktorijuma
	void setZeros();

	int table[512] = { 0 };																					// broj ulaza u jednom klasteru. Tu se nalazi broj indexa 2. nivoa ako je u pitanju tabela indeksa prvog nivoa, ili klaster podataka ako je u pitanju tablea 2. nivoa

private:
};

