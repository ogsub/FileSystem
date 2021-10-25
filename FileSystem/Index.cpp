#include "Index.h"
#include "KernelFS.h"



Index::Index() {
}


Index::~Index()
{
}

char Index::doesExist(char* fname, Partition* partition, FileFamilyTree* fft) {//int* clusterNo_, unsigned long* size_) {  //poziva se nad indeksom koji predstavlja direktorijum 1. nivo
	for (int i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		int clusterNoDir2 = table[i];
		if (clusterNoDir2 == 0) 
			return 0;
		Index secondLevelIndex = Index();
		partition->readCluster(clusterNoDir2, (char*)&secondLevelIndex);									//citamo sa zadatog klastera u buffer secondLevelIndex
		
		for (int i = 0; i < sizeof(secondLevelIndex.table) / sizeof(secondLevelIndex.table[0]); i++) {
			int clusterNoDirData = secondLevelIndex.table[i];
			if (clusterNoDirData == 0) 
				return 0;
			Data dirData = Data();
			partition->readCluster(clusterNoDirData, (char*)&dirData);

			for (int i = 0; i < sizeof(dirData.table) / sizeof(dirData.table[0]); i++) {
				if (dirData.isEntryEmpty(i))
					return 0;
				if (dirData.compareFileName(fname, i)) {
					if (fft != nullptr) {
						fft->dir2LvlCluster = clusterNoDir2;
						fft->dirDataCluster = clusterNoDirData;
						fft->file1LvlCluster = dirData.getIndexClusterNo(i);
						fft->size = dirData.getFileSize(i);
					}
					
					return 1;
				}
			} 
		}
	}
}

int Index::readRootDir(Partition *partition) {
	int filesNo = 0;
	for (int i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		int clusterNo = table[i];
		if (clusterNo == 0)
			return filesNo;
		Index secondLevelIndex = Index();
		partition->readCluster(clusterNo, (char*)&secondLevelIndex);

		for (int i = 0; i < sizeof(secondLevelIndex.table) / sizeof(secondLevelIndex.table[0]); i++) {
			int clusterNo = secondLevelIndex.table[i];
			if (clusterNo == 0)
				return filesNo;
			Data dirData = Data();
			partition->readCluster(clusterNo, (char*)&dirData);

			for (int i = 0; i < sizeof(dirData.table) / sizeof(dirData.table[0]); i++) {
				if (dirData.isEntryEmpty(i))
					return filesNo;
				filesNo++;
			}
		}
	}
}

int Index::firstFreeIndexEntry() {
	for (int i = 0; i < 512; i++) {
		if (table[i] == 0)
			return i;
	}

	/*
	int dir1LvlLastEntry = dirIndex.getLastEntryNo();
	if (dir1LvlLastEntry == -1) {						//znaci da je tabela prazna
		int freeClusterNo = bitVector.getFirstFreeClusterNo(); //pronalazimo slobodno mesto za index direktorijuma drugog nivoa
		dirIndex.table[0] = freeClusterNo;
		bitVector.useBitVector(freeClusterNo);
		Index dirIndex2 = Index();
		partition->readCluster(freeClusterNo, (char*)& dirIndex2);
		int freeClusterNo2 = bitVector.getFirstFreeClusterNo();//pronalazimo slobodno mesto za klaster za podatke direktorijuma
		dirIndex2.table[0] = freeClusterNo2;
		bitVector.useBitVector(freeClusterNo2);
		//Dovucemo data deo i u prvi ulaz upisemo potrebne podatke
		Data dirData = Data();
		partition->readCluster(freeClusterNo2, (char*)& dirData);

	}
	*/
		
	/*
	int clusterNo;
	int level1;
	int level2;
	int dataLevel;

	// Pretrazujemo  index prvog nivoa za prvi prazan ulaz, da bi usli u onu pre nje i uradili isto to za index drugog nivoa
	//i da bi nasli u data bloku direktorijuma mesto za fajl
	for (level1 = 0; level1 != sizeof(dirIndex.table) / sizeof(dirIndex.table[0]); level1++) {
		clusterNo = dirIndex.table[level1];
		if (clusterNo == 0 && clusterNo < NUMBEROFCLUSTERS && bitVector.isFree(clusterNo)) {
			// Ako je odmah prazan nulti ulaz u index-u 1 to znaci da nema ni jednog fajla u direktorijumu i da diretkno u ovaj upisujemo
			if (level1 == 0) {
				//U ulaz index1 upisemo broj klastera koji predstavlja index2 i zauzmemo taj klaster u bitVektoru
				int freeClusterNo = bitVector.getFirstFreeClusterNo();
				dirIndex.table[0] = freeClusterNo;
				bitVector.useBitVector(freeClusterNo);
				//U ulaz index2(koji prvo dovucemo sa diska) upisemo broj klastera koji predstavlja data i zauzmemo taj klaster u bitVektoru
				Index dirIndex2 = Index();
				partition->readCluster(freeClusterNo, (char *)&dirIndex2);
				int freeClusterNo2 = bitVector.getFirstFreeClusterNo();
				dirIndex2.table[0] = freeClusterNo2;
				bitVector.useBitVector(freeClusterNo2);
				//Dovucemo data deo i u prvi ulaz upisemo potrebne podatke
				Data dirData = Data();
				partition->readCluster(freeClusterNo2, (char *)&dirData);
				//dirData.
			}
			Index dirIndex = Index();
			partition->readCluster(dirIndex.table[level1-1], (char *)&dirIndex);
			//for (level2 = 0; level2 != sizeof(dirIndex.table) / sizeof(dirIndex.table[0]); level2++)
		}
	}*/


	return 0;
}

int Index::getLastEntryNo() {
	for (int i = 0; i < 512; i++) {
		if (this->table[i] == 0)
			return i - 1;								//vraca -1 ukoliko tabela potpuno prazna
	}
}

int Index::findEntry(int clusterNo) {
	for (int i = 0; i < 512; i++) {
		if (table[i] == clusterNo)
			return i;
	}
	return -1;
}

void Index::setZeros() {
	for (int i = 0; i < 512; i++) {
		table[i] = 0;
	}
}
