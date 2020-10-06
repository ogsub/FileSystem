#include "KernelFile.h"
#include "KernelFS.h"
#include <algorithm>
#include <cmath>
#include <iostream>

HANDLE KernelFile::mutexRW = CreateSemaphore(NULL, 1, 32, NULL);

KernelFile::KernelFile(int clusterNo, unsigned long size) {
	this->clusterNo = clusterNo;
	this->size = size;
	fileOpenedCache = FileOpenedCache();
}

KernelFile::~KernelFile() {

	//999999999999999999999999999999999999999999999
	std::vector<KernelFS::ProcesFCB>::iterator it2;
	this->isFileAvilable(fileToCloseAddr, &it2);

	FileFamilyTree* fft = new FileFamilyTree();
	KernelFS::dir1Lvl->doesExist(it2->name, KernelFS::partition, fft);
	Data* dirData = new Data();
	KernelFS::partition->readCluster(fft->dirDataCluster, (char*) dirData);
	int dirDataEntry = dirData->findEntry(clusterNo);							//trazi ulaz u dirData koji pokazuje na zadati cluster file1Lvl
	dirData->setFileSize(dirDataEntry, this->size);
	KernelFS::partition->writeCluster(fft->dirDataCluster, (char*)dirData);

	delete fft;
	delete dirData;
	//999999999999999999999999999999999999999999999

	//std::unique_lock<std::mutex> lock(this->m);
	//if (KernelFS::getOpenedFilesNo() == 0 && KernelFS::unmountingInProgress)
		//signal(KernelFS::openedFilesSem);
	//std::cout << "-------------" <<this->size << "-------------" << std::endl;

	unsigned long threadId = GetCurrentThreadId();
	unsigned long fileReferences = 0;												//povecava se ukoliko postoji jos neki proces osim ovog koji je otvorio fajl
	for (auto it = KernelFS::threads.begin(); it != KernelFS::threads.end(); it++) {
		auto it2 = std::find_if(it->second->begin(), it->second->end(),
			[this](KernelFS::ProcesFCB& procFCB) {
				return procFCB.file == fileToCloseAddr;
			});

		if (it2 != it->second->end() && it->first != threadId) {
			fileReferences++;														//povecava se ukoliko postoji jos neki proces osim ovog koji je otvorio fajl
		}

		if (it->first == threadId && it2 != it->second->end()) {
			//2222222222222222222222222222222222222222222222222222222222222222222222222222
			if (it2->mode != 'r') {					//Zadnji data koji nije upisan kad se radio write
				if (this->fileOpenedCache.clusterNoFile1Lvl != 0) {
					KernelFS::partition->writeCluster(this->fileOpenedCache.clusterNoFile1Lvl, (char*)this->fileOpenedCache.file1Lvl);
				}
				if (this->fileOpenedCache.clusterNoFile2Lvl != 0) {
					KernelFS::partition->writeCluster(this->fileOpenedCache.clusterNoFile2Lvl, (char*)this->fileOpenedCache.file2Lvl);
				}
				if (this->fileOpenedCache.clusterNoFileData != 0) {
					KernelFS::partition->writeCluster(this->fileOpenedCache.clusterNoFileData, (char*)this->fileOpenedCache.fileData);
				}
			}
			//2222222222222222222222222222222222222222222222222222222222222222222222222222
			it->second->erase(it2);
		}
	}

	if (fileReferences < 1) {
		auto it = std::find_if(KernelFS::openedFiles.begin(), KernelFS::openedFiles.end(),
			[this](std::tuple<char*, File*, char> elem) {
				return std::get<1>(elem) == fileToCloseAddr;
			});

		if (it != KernelFS::openedFiles.end()) {
			KernelFS::openedFiles.erase(it);
			KernelFS::cvCanOpen.notify_all();
		}
	}

	//********************************************
	if (KernelFS::getOpenedFilesNo() == 0) {
		//mySignal(KernelFS::openedFilesSem);
		KernelFS::cvAllFilesClosed.notify_all();
	}
	//********************************************
}
 
int KernelFile::isFileAvilable(File* addr, std::vector<KernelFS::ProcesFCB>::iterator* it2) {
	std::map<DWORD, std::vector<KernelFS::ProcesFCB>*>::iterator it = KernelFS::threads.find(GetCurrentThreadId());
	if (it == KernelFS::threads.end()) {
		return 0;																//ako thread uopste nema ni jedan otvoren fajl, vraca se greska
	}
	auto FCBVect = it->second;
	*it2 = std::find_if(FCBVect->begin(), FCBVect->end(),
		[&addr](KernelFS::ProcesFCB& procFCB) {
			return procFCB.file == addr;
		}
	);
	if (*it2 == FCBVect->end()) {
		return 0;																//ukoliko je fajl zatvoren vraca se greska
	}
	return 1;
}































char KernelFile::write(BytesCnt bytesCnt, char * buffer, File* addr){
	//if (this->eof(addr))
	//	return 0;

	std::vector<KernelFS::ProcesFCB>::iterator it2;
	if (this->isFileAvilable(addr, &it2) == 0)
		return 0;	//fajl nije otvoren za ovaj proces

	//proveravamo da li je u odgovarajucem modu, nije moguce upisivati ukoliko je otvoren u modu 'r'
	if (it2->mode == 'r') {
		std::cout << "***********Nije odgovarajuci mod***********";
		return 0;
	}

	//ovo koristimo da sacuvamo podatke koje se nalaze posle cursor-a, da bi ih sacuvali, i upisali nakon ovoga sto upisujemo
	unsigned long bytesUntilEOF = this->size - it2->cursor;
	char* bytesArray = new char[bytesUntilEOF];

	//procitali smo podatke posle kursora, ako postoje uopste														//trebalo bi da bude jednaka bytesUntillEOF-u
	char* bufferWithRest = new char[bytesCnt + bytesUntilEOF];
	if (bytesUntilEOF > 0) {
		unsigned long oldCursor = it2->cursor;				//posto read mrda cursor
		this->read(bytesUntilEOF, bytesArray, addr);		
		//this->seek(oldCursor, addr);	
		it2->cursor = oldCursor;
	}
	int i;
	for (i = 0; i < bytesCnt; i++) {
		bufferWithRest[i] = buffer[i];
	}
	for (int j = 0; j < bytesUntilEOF; j++) {
		bufferWithRest[i + j] = bytesArray[j];
	}
														//}
	//povecamo size jer dodajemo bytove
	this->size += bytesCnt;
	//this->size = this->size > 2048 ? 2048 : this->size;
	//update size u odgovarajucem ulazu dir2Lvl clustera


	//3333333333333333333Index dir1Lvl = Index();
	//3333333333333333333KernelFS::partition->readCluster(1, (char*)& dir1Lvl);

	//999999999999999999999999999999999
	//FileFamilyTree* fft = new FileFamilyTree();
	//KernelFS::dir1Lvl->doesExist(it2->name, KernelFS::partition, fft);
	//Data* dirData = new Data();
	//KernelFS::partition->readCluster(fft->dirDataCluster, (char*) dirData);
	//int dirDataEntry = dirData->findEntry(clusterNo);							//trazi ulaz u dirData koji pokazuje na zadati cluster file1Lvl
	//dirData->setFileSize(dirDataEntry, this->size);
	//KernelFS::partition->writeCluster(fft->dirDataCluster, (char*)dirData);
	
	//delete fft;
	//delete dirData;
	//999999999999999999999999999999999

	if (this->size > 39475) {
		//std::cout << "asdadasdasd" << std::endl;
	}

	unsigned long bytesToWrite = this->size - it2->cursor;
	//sad cemo da upisemo nove podatke od kursora, i potom na njih nadovezemo ove stare do kraja fajla

	unsigned long startingDataBlock = std::trunc(it2->cursor / ClusterSize);	//broj bloka sa podacima(od 0 do zadnjeg bloka, ne ClusterNo)
	unsigned long startingDataBlockOffset = (it2->cursor % ClusterSize); //dobro je 97%, u slucaju da krenes da se dvoumis	//ofset za pocetni byte od kog se cita, unutar bloka(clustera) sa podacima fajla

	unsigned long numOfEntries = 512;
	unsigned long file1LvlEntry = std::trunc(startingDataBlock / numOfEntries);		//ulaz u index prvog nivoa fajla
	unsigned long file2LvlEntry = startingDataBlock % numOfEntries;					//ulaz u index drugog nivoa fajla

	//update-uj cursor da pokazuje tamo gde treba nakon upisa
	it2->cursor += bytesCnt;

	//BitVector bitVector = BitVector();
	//KernelFS::partition->readCluster(0, (char*)& bitVector);

	bool writeFile1Lvl = false;
	bool writeFile2Lvl = false;

	Index* file1Lvl = nullptr;
	if (fileOpenedCache.clusterNoFile1Lvl != this->clusterNo) {
		//2222222222222222222222222222222222222222222222222222222222222222222222222222
		if (fileOpenedCache.clusterNoFile1Lvl != 0) {		//znaci da je nesto bilo upisivano u njemu
			KernelFS::partition->writeCluster(fileOpenedCache.clusterNoFile1Lvl, (char*)fileOpenedCache.file1Lvl);
			//delete fileOpenedCache.file1Lvl;
		}
		//2222222222222222222222222222222222222222222222222222222222222222222222222222
		file1Lvl = new Index();
		KernelFS::partition->readCluster(this->clusterNo, (char*)file1Lvl);
		fileOpenedCache.clusterNoFile1Lvl = this->clusterNo;
		fileOpenedCache.file1Lvl = file1Lvl;
	}
	else {
		file1Lvl = fileOpenedCache.file1Lvl;
	}
	Index* file2Lvl = nullptr;

	if (file1Lvl->table[file1LvlEntry] == 0) {	//znaci da je prazan taj ulaz i da ne postoji file2Lvl za taj ulaz, moramo da zauzmemo novi
		writeFile1Lvl = true;	//menjali smo stanje u file1Lvl pa moramo da upisemo na hard disk
		unsigned long freeClusterNo = KernelFS::bitVectorPtr->getFirstFreeClusterNo(KernelFS::partition);//;)
		file1Lvl->table[file1LvlEntry] = freeClusterNo;

		//2222222222222222222222222222222222222222222222222222222222222222222222222222
		if (fileOpenedCache.clusterNoFile2Lvl != 0) {		//znaci da je nesto bilo upisivano u njemu
			KernelFS::partition->writeCluster(fileOpenedCache.clusterNoFile2Lvl, (char*)fileOpenedCache.file2Lvl);
			//delete fileOpenedCache.file2Lvl;
		}
		//2222222222222222222222222222222222222222222222222222222222222222222222222222

		file2Lvl = new Index();
		KernelFS::partition->writeCluster(freeClusterNo, (char*) file2Lvl);	//ako je ovo novi cluster koga citamo, a ne neki sa postojecim podacima, moramo ga inicijalizovati sa svim nulama
		

		//cuvanje u cache-u
		fileOpenedCache.clusterNoFile2Lvl = freeClusterNo;
		fileOpenedCache.file2Lvl = file2Lvl;
	}
	else {	//znaci nije prazan ulaz i postoji file2Lvl za taj ulaz, i samo ga citamo
		if (fileOpenedCache.clusterNoFile2Lvl != file1Lvl->table[file1LvlEntry]) {
			//2222222222222222222222222222222222222222222222222222222222222222222222222222
			if (fileOpenedCache.clusterNoFile2Lvl != 0) {		//znaci da je nesto bilo upisivano u njemu
				KernelFS::partition->writeCluster(fileOpenedCache.clusterNoFile2Lvl, (char*)fileOpenedCache.file2Lvl);
				//delete fileOpenedCache.file2Lvl;
			}
			//2222222222222222222222222222222222222222222222222222222222222222222222222222
			
			file2Lvl = new Index();

			KernelFS::partition->readCluster(file1Lvl->table[file1LvlEntry], (char*)file2Lvl);
			//cuvanje u cache-u
			fileOpenedCache.clusterNoFile2Lvl = file1Lvl->table[file1LvlEntry];
			fileOpenedCache.file2Lvl = file2Lvl;
		}
		else {
			file2Lvl = fileOpenedCache.file2Lvl;
		}
	}

	//if (file2Lvl->table[0] == 1684235363) {
	//	std::cout << "ALO BRE" << std::endl;
	//}

	KernelFile::FileDataBlock* dataBlock = nullptr;

	if (file2Lvl->table[file2LvlEntry] == 0) {	//znaci da je prazan taj ulaz i da ne postoji file2Lvl za taj ulaz, moramo da zauzmemo novi
		writeFile2Lvl = true;
		unsigned long freeClusterNo = KernelFS::bitVectorPtr->getFirstFreeClusterNo(KernelFS::partition);//;)
		//if (freeClusterNo == 1684235363) {
		//	std::cout << "ALO BRE" << std::endl;
		//}
		file2Lvl->table[file2LvlEntry] = freeClusterNo;

		//2222222222222222222222222222222222222222222222222222222222222222222222222222
		if (fileOpenedCache.clusterNoFileData != 0) {		//znaci da je nesto bilo upisivano u njemu
			KernelFS::partition->writeCluster(fileOpenedCache.clusterNoFileData, (char*)fileOpenedCache.fileData);
			//delete fileOpenedCache.fileData;
		}
		//2222222222222222222222222222222222222222222222222222222222222222222222222222

		dataBlock = new KernelFile::FileDataBlock();
		KernelFS::partition->writeCluster(freeClusterNo, (char*) dataBlock);	//**** mislim da za data i ne mora, jer sam gledao tu po size-u //ako je ovo novi cluster koga citamo, a ne neki sa postojecim podacima, moramo ga inicijalizovati sa svim nulama
		
		//cuvanje u cache-u
		fileOpenedCache.clusterNoFileData = freeClusterNo;
		fileOpenedCache.fileData = dataBlock;
	}
	else {
		if (fileOpenedCache.clusterNoFileData != file2Lvl->table[file2LvlEntry]) {
			//2222222222222222222222222222222222222222222222222222222222222222222222222222
			if (fileOpenedCache.clusterNoFileData != 0) {		//znaci da je nesto bilo upisivano u njemu
				KernelFS::partition->writeCluster(fileOpenedCache.clusterNoFileData, (char*)fileOpenedCache.fileData);
				//delete fileOpenedCache.fileData;
			}
			//2222222222222222222222222222222222222222222222222222222222222222222222222222
			
			dataBlock = new KernelFile::FileDataBlock();

			KernelFS::partition->readCluster(file2Lvl->table[file2LvlEntry], (char*)dataBlock);
			//cuvanje u cache-u
			fileOpenedCache.clusterNoFileData = file2Lvl->table[file2LvlEntry];
			fileOpenedCache.fileData = dataBlock;
		}
		else {
			dataBlock = fileOpenedCache.fileData;
		}
	}

	unsigned long bytesWritten = 0;
	//unsigned long bytesUntilEOF = this->size - it2->cursor;
	while (bytesWritten < bytesToWrite) {
		//buffer[bytesRead++] = dataBlock.getByte(startingDataBlockOffset++);
		dataBlock->setByte(startingDataBlockOffset++, bufferWithRest[bytesWritten++]);
		//if (--bytesUntilEOF == 0)													//ukoliko se stiglo do kraja fajla
		//	return 1;

		if (bytesWritten == bytesToWrite) {
			//~~~~~~~~~~KernelFS::partition->writeCluster(0, (char*) KernelFS::bitVectorPtr);

			//2222222222222222222222222222222222222222222222222222222222222222222222222222
			//if(writeFile1Lvl)
			//	KernelFS::partition->writeCluster(this->clusterNo, (char*) file1Lvl);
			//if(writeFile2Lvl)
			//	KernelFS::partition->writeCluster(file1Lvl->table[file1LvlEntry], (char*) file2Lvl);
			//KernelFS::partition->writeCluster(file2Lvl->table[file2LvlEntry], (char*) dataBlock);
			//2222222222222222222222222222222222222222222222222222222222222222222222222222

			delete[] bytesArray;
			delete[] bufferWithRest;
			return 1;
		}

		if (startingDataBlockOffset == 2048) {										//ukoliko se stiglo do kraja sa citanjem jednog klastera file data, pa treba da se predje na drugi
			startingDataBlockOffset %= 2048;
			KernelFS::partition->writeCluster(file2Lvl->table[file2LvlEntry], (char*) dataBlock);		//sacuvaj u memoriji

			file2LvlEntry++;
			if (file2LvlEntry == 512) {												//ukoliko se stiglo do kraja(posle zadnjeg ulaza) indexa fajla 2. niva, a ima jos da se cita, pa se prelazi na drugi klaster sa index2Lvl
				file2LvlEntry %= 512;
				KernelFS::partition->writeCluster(file1Lvl->table[file1LvlEntry], (char*) file2Lvl);		//sacuvaj u memoriji

				file1LvlEntry++;
				if (file1LvlEntry == 512) {						//ukoliko se memorija predvidjena za 1 fajl prepuni
					KernelFS::partition->writeCluster(this->clusterNo, (char*) file1Lvl);
					return 0;									//puna memorija, vracam neuspeh, upisao je sta je upisao
				}
				if (file1Lvl->table[file1LvlEntry] != 0) {
					//2222222222222222222222222222222222222222222222222222222222222222222222222222
					if (fileOpenedCache.clusterNoFile2Lvl != 0) {		//znaci da je nesto bilo upisivano u njemu
						KernelFS::partition->writeCluster(fileOpenedCache.clusterNoFile2Lvl, (char*)fileOpenedCache.file2Lvl);
						//delete fileOpenedCache.file2Lvl;
					}
					//2222222222222222222222222222222222222222222222222222222222222222222222222222
					

					KernelFS::partition->readCluster(file1Lvl->table[file1LvlEntry], (char*) file2Lvl);
					//cuvanje u cache-u
					fileOpenedCache.clusterNoFile2Lvl = file1Lvl->table[file1LvlEntry];
					fileOpenedCache.file2Lvl = file2Lvl;
				}
				else {
					//2222222222222222222222222222222222222222222222222222222222222222222222222222
					if (fileOpenedCache.clusterNoFile2Lvl != 0) {		//znaci da je nesto bilo upisivano u njemu
						KernelFS::partition->writeCluster(fileOpenedCache.clusterNoFile2Lvl, (char*)fileOpenedCache.file2Lvl);
						//delete fileOpenedCache.file2Lvl;
					}
					//2222222222222222222222222222222222222222222222222222222222222222222222222222


					file1Lvl->table[file1LvlEntry] = KernelFS::bitVectorPtr->getFirstFreeClusterNo(KernelFS::partition);//;)
					//bitVector.useBitVector(file1Lvl.table[file1LvlEntry], KernelFS::partition);
					KernelFS::partition->readCluster(file1Lvl->table[file1LvlEntry], (char*) file2Lvl);
					file2Lvl->setZeros();

					//cuvanje u cache-u
					fileOpenedCache.clusterNoFile2Lvl = file1Lvl->table[file1LvlEntry];
					fileOpenedCache.file2Lvl = file2Lvl;
				}
			}

			if (file2Lvl->table[file2LvlEntry] != 0) {								//u slucaju da se prelazi na novi file2LvlEntry ulaz koji je u indexu 2. nivoa = 0
				
				//2222222222222222222222222222222222222222222222222222222222222222222222222222
				if (fileOpenedCache.clusterNoFileData != 0) {		//znaci da je nesto bilo upisivano u njemu
					KernelFS::partition->writeCluster(fileOpenedCache.clusterNoFileData, (char*)fileOpenedCache.fileData);
					//delete fileOpenedCache.fileData;
				}
				//2222222222222222222222222222222222222222222222222222222222222222222222222222


				KernelFS::partition->readCluster(file2Lvl->table[file2LvlEntry], (char*) dataBlock);
				//cuvanje u cache-u
				fileOpenedCache.clusterNoFileData = file2Lvl->table[file2LvlEntry];
				fileOpenedCache.fileData = dataBlock;
			}
			else {																
				file2Lvl->table[file2LvlEntry] = KernelFS::bitVectorPtr->getFirstFreeClusterNo(KernelFS::partition);//;)
				//bitVector.useBitVector(file2Lvl.table[file2LvlEntry], KernelFS::partition);


				//2222222222222222222222222222222222222222222222222222222222222222222222222222
				if (fileOpenedCache.clusterNoFileData != 0) {		//znaci da je nesto bilo upisivano u njemu
					KernelFS::partition->writeCluster(fileOpenedCache.clusterNoFileData, (char*)fileOpenedCache.fileData);
					//delete fileOpenedCache.fileData;
				}
				//2222222222222222222222222222222222222222222222222222222222222222222222222222



				KernelFS::partition->readCluster(file2Lvl->table[file2LvlEntry], (char*) dataBlock);
				//cuvanje u cache-u
				fileOpenedCache.clusterNoFileData = file2Lvl->table[file2LvlEntry];
				fileOpenedCache.fileData = dataBlock;
				//KernelFS::partition->writeCluster(file1Lvl.table[file1LvlEntry], (char*)& file2Lvl);			//ne moram ovde i u ostalim else-ovima ove da cuvam, jer ce se oni resiti ukoliko se prepune, a ako se ne prepune, cuvaju se na kraju ovog while-a. Tacno 99.9%!
			}
		}
	}
	

	delete[] bytesArray;
	delete[] bufferWithRest;
}






























BytesCnt KernelFile::read(BytesCnt bytesToRead, char * buffer, File* addr) {
	if (this->eof(addr))
		return 0;

	std::vector<KernelFS::ProcesFCB>::iterator it2;
	if (this->isFileAvilable(addr, &it2) == 0)
		return 0;	//fajl nije otvoren za ovaj proces
	

	unsigned long startingDataBlock = std::trunc(it2->cursor / ClusterSize);	//broj bloka sa podacima(od 0 do zadnjeg bloka, ne ClusterNo)
	unsigned long startingDataBlockOffset = (it2->cursor % ClusterSize); //dobro je 97%, u slucaju da krenes da se dvoumis	//ofset za pocetni byte od kog se cita, unutar bloka(clustera) sa podacima fajla
	//unsigned long numOfBlocksToRead = std::ceil((bytesToRead + (double)it2->cursor) / ClusterSize);

	unsigned long numOfEntries = 512;
	unsigned long file1LvlEntry = std::trunc(startingDataBlock / numOfEntries);		//ulaz u index prvog nivoa fajla
	unsigned long file2LvlEntry = startingDataBlock % numOfEntries;					//ulaz u index drugog nivoa fajla

	Index* file1Lvl = nullptr;
	if (fileOpenedCache.clusterNoFile1Lvl != this->clusterNo) {
		file1Lvl = new Index();
		KernelFS::partition->readCluster(this->clusterNo, (char*)file1Lvl);
		fileOpenedCache.clusterNoFile1Lvl = this->clusterNo;
		fileOpenedCache.file1Lvl = file1Lvl;
	}
	else {
		file1Lvl = fileOpenedCache.file1Lvl;
	}
	Index* file2Lvl;
	if (fileOpenedCache.clusterNoFile2Lvl != file1Lvl->table[file1LvlEntry]) {
		file2Lvl = new Index();
		KernelFS::partition->readCluster(file1Lvl->table[file1LvlEntry], (char*)file2Lvl);
		fileOpenedCache.clusterNoFile2Lvl = file1Lvl->table[file1LvlEntry];
		fileOpenedCache.file2Lvl = file2Lvl;
	}
	else {
		file2Lvl = fileOpenedCache.file2Lvl;
	}
	KernelFile::FileDataBlock* dataBlock = nullptr;
	if (fileOpenedCache.clusterNoFileData != file2Lvl->table[file2LvlEntry]) {
		dataBlock = new KernelFile::FileDataBlock();
		KernelFS::partition->readCluster(file2Lvl->table[file2LvlEntry], (char*)dataBlock);
		fileOpenedCache.clusterNoFileData = file2Lvl->table[file2LvlEntry];
		fileOpenedCache.fileData = dataBlock;
	}
	else {
		dataBlock = fileOpenedCache.fileData;
	}

	unsigned long bytesRead = 0;
	unsigned long bytesUntilEOF = this->size - it2->cursor;

	//update-uj cursor da pokazuje tamo gde treba nakon upisa
	it2->cursor += bytesToRead;
	//it2->cursor = it2->cursor > 2048 ? 2048 : it2->cursor;

	while (bytesRead < bytesToRead) {
		buffer[bytesRead++] = dataBlock->getByte(startingDataBlockOffset++);
		if (--bytesUntilEOF == 0)													//ukoliko se stiglo do kraja fajla
			return bytesRead - 1;

		if (startingDataBlockOffset == 2048) {										//ukoliko se stiglo do kraja sa citanjem jednog klastera file data, pa treba da se predje na drugi
			startingDataBlockOffset %= 2048;
			file2LvlEntry++;
			if (file2LvlEntry == 512) {												//ukoliko se stiglo do kraja(posle zadnjeg ulaza) indexa fajla 2. niva, a ima jos da se cita, pa se prelazi na drugi klaster sa index2Lvl
				file2LvlEntry %= 512;
				file1LvlEntry++;
				KernelFS::partition->readCluster(file1Lvl->table[file1LvlEntry], (char*) file2Lvl);
				//update cache
				fileOpenedCache.clusterNoFile2Lvl = file1Lvl->table[file1LvlEntry];
				fileOpenedCache.file2Lvl = file2Lvl;
			}
			KernelFS::partition->readCluster(file2Lvl->table[file2LvlEntry], (char*) dataBlock);
			//update cache
			fileOpenedCache.clusterNoFileData = file2Lvl->table[file2LvlEntry];
			fileOpenedCache.fileData = dataBlock;
		}
	}

	//delete file1Lvl;
	return bytesRead - 1;
}














char KernelFile::seek(BytesCnt cnt, File* addr) {
	std::vector<KernelFS::ProcesFCB>::iterator it2;
	if (this->isFileAvilable(addr, &it2) == 0)
		return 0;	//fajl nije otvoren za ovaj proces

	if (it2->mode != 'r') {					//Zadnji data koji nije upisan kad se radio write
		if (this->fileOpenedCache.clusterNoFile1Lvl != 0) {
			KernelFS::partition->writeCluster(this->fileOpenedCache.clusterNoFile1Lvl, (char*)this->fileOpenedCache.file1Lvl);
		}
		if (this->fileOpenedCache.clusterNoFile2Lvl != 0) {
			KernelFS::partition->writeCluster(this->fileOpenedCache.clusterNoFile2Lvl, (char*)this->fileOpenedCache.file2Lvl);
		}
		if (this->fileOpenedCache.clusterNoFileData != 0) {
			KernelFS::partition->writeCluster(this->fileOpenedCache.clusterNoFileData, (char*)this->fileOpenedCache.fileData);
		}
	}

	if (this->getFileSize(addr) < cnt)
		return 0;

	it2->cursor = cnt;
	return 1;
}

BytesCnt KernelFile::filePos(File* addr) {
	std::vector<KernelFS::ProcesFCB>::iterator it2;
	if (this->isFileAvilable(addr, &it2) == 0)
		return -1;	//fajl nije otvoren za ovaj proces //vraca -1 zato sto 0 znaci da je cursor na pocetku fajla, a ne gresku
	return it2->cursor;
}

char KernelFile::eof(File* addr) {
	//~~~~~~~~~~~~~~~std::vector<KernelFS::ProcesFCB>::iterator it2;
	//~~~~~~~~~~~~~~~if (this->isFileAvilable(addr, &it2) == 0) 
	//~~~~~~~~~~~~~~~	return 1;	//fajl nije otvoren za ovaj proces
	
	if (this->getFileSize(addr) == this->filePos(addr)) 
		return 2;
	else
		return 0;
}

BytesCnt KernelFile::getFileSize(File* addr) {
	//std::vector<KernelFS::ProcesFCB>::iterator it2;
	//if (this->isFileAvilable(addr, &it2) == 0)
	//	return -1;	//fajl nije otvoren za ovaj proces

	return size;
}

char KernelFile::truncate(File* addr) {
	if (this->eof(addr))
		return 0;

	std::vector<KernelFS::ProcesFCB>::iterator it2;
	if (this->isFileAvilable(addr, &it2) == 0)
		return 0;	//fajl nije otvoren za ovaj proces

	unsigned long startingDataBlock = std::trunc(it2->cursor / ClusterSize);	//broj bloka sa podacima(od 0 do zadnjeg bloka, ne ClusterNo)
	unsigned long startingDataBlockOffset = (it2->cursor % ClusterSize);		//dobro je 97%, u slucaju da krenes da se dvoumis	//ofset za pocetni byte od kog se cita, unutar bloka(clustera) sa podacima fajla

	unsigned long numOfEntries = 512;
	unsigned long file1LvlEntry = std::trunc(startingDataBlock / numOfEntries);		//ulaz u index prvog nivoa fajla
	unsigned long file2LvlEntry = startingDataBlock % numOfEntries;					//ulaz u index drugog nivoa fajla

	//BitVector bitVector = BitVector();
	//KernelFS::partition->readCluster(0, (char*)& bitVector);
	Index file1Lvl = Index();
	KernelFS::partition->readCluster(this->clusterNo, (char*)& file1Lvl);
	Index file2Lvl = Index();
	

	//smanjimo size na onoliko koliko ce biti posle brisanja
	//stavimo nule u narednim ulazima file2Lvl i file1Lvl, sve do prve sledece nule
	this->size = it2->cursor;
	
	bool deleteFile2Lvl = false;
	if (startingDataBlockOffset != 0) { //ukoliko postoji deo u fileData koji se ne brise(kursor nije na pocetku fileData cluster-a), taj cluster se ne oslobadja
		file2LvlEntry++;
	}
	else if (file2LvlEntry == 0) {		//ukoliko je cursor na samom pocetku data klastera koji se brise, i ako je taj klaster u nultom entry file2Lvl clastera, onda se i taj claster file2Lvl brise
		//file1LvlEntry++;
		deleteFile2Lvl = true;
	}

	bool firstCycle = true;				//flag koji koristimo uz deleteFile2Lvl flag da bi znali da li se brise prvi file2Lvl cluster

	//prolazimo kroz sve ulaze u file1Lvl, i oslobadjamo odgovarajuce klastere file2Lvl i fileData
	for (int f1 = file1LvlEntry; file1Lvl.table[file1LvlEntry] != 0 && file1LvlEntry != 512; file1LvlEntry++) {
		KernelFS::partition->readCluster(file1Lvl.table[file1LvlEntry], (char*)& file2Lvl);

		//prolazimo kroz sve ulaze u file2Lvl, i brisemo ih (ukljucujuci i prvi ukoliko nismo usli u onaj if(startingDataBlockOffset != 0)), tada se nije onaj ulaz u file2Lvl gde se nalazi kursor preskocio
		for (int f2 = file2LvlEntry; file2Lvl.table[file2LvlEntry] != 0 && file2LvlEntry != 512; file2LvlEntry++) {
			KernelFS::bitVectorPtr->freeBitVector(file2Lvl.table[file2LvlEntry]);
			file2Lvl.table[file2LvlEntry] = 0;
		}
		file2LvlEntry = 0;
		if (firstCycle && deleteFile2Lvl) {
			KernelFS::bitVectorPtr->freeBitVector(file1Lvl.table[file1LvlEntry]);
			file1Lvl.table[file1LvlEntry] = 0;
			firstCycle = false;
		}
		if (!firstCycle) {				//samo ukoliko nije prvi ciklus kroz prvu for petlju, vazi ova standardna if, a za prvi ciklus zavisi i od deleteFile2Lvl falg-a
			KernelFS::bitVectorPtr->freeBitVector(file1Lvl.table[file1LvlEntry]);
			file1Lvl.table[file1LvlEntry] = 0;
		}

		KernelFS::partition->writeCluster(file1Lvl.table[file1LvlEntry], (char*)& file2Lvl);
	}

	KernelFS::partition->writeCluster(this->clusterNo, (char*)& file1Lvl);
	//oooKernelFS::partition->writeCluster(0, (char*) KernelFS::bitVectorPtr);
	return 1;
}

void KernelFile::setFileToCloseAddr(File* addr) {
	this->fileToCloseAddr = addr;
}
