#include "KernelFS.h"
#include "KernelFile.h"
#include <iostream>												//samo za debug, izbrisi posle
#include <memory>
#include <algorithm>

Partition* KernelFS::partition = nullptr;
BitVector* KernelFS::bitVectorPtr = new BitVector();
//3333333333333333333333
Index* KernelFS::dir1Lvl = new Index();
//3333333333333333333333

std::map<DWORD, std::vector<KernelFS::ProcesFCB>*> KernelFS::threads;
std::vector<std::tuple<char*, File*, char>> KernelFS::openedFiles;
HANDLE KernelFS::openedFilesSem;														//VAZNO!!! Pocetna vrednost mu je 0, postaje 1 kad god se neki fajl zatvori
bool KernelFS::unmountingInProgress = false;

std::condition_variable KernelFS::cvAllFilesClosed;
std::condition_variable KernelFS::cvCanOpen;

std::mutex KernelFS::m;


KernelFS::KernelFS() { 
	this->mutexMountPartition  = CreateSemaphore(NULL, 1, 32, NULL);
	this->mutexMonitor    = CreateSemaphore(NULL, 1, 32, NULL);
	this->openedFilesSem = CreateSemaphore(NULL, 0, 32, NULL);

	openedFiles.reserve(100);
}


KernelFS::~KernelFS() {
	CloseHandle(this->mutexMountPartition);
	CloseHandle(this->mutexMonitor);
	CloseHandle(this->openedFilesSem);
}

char KernelFS::mount(Partition * partition) {
	myWait(this->mutexMountPartition);															//ako je mutex 1, onda je this->partition = 0 ako je mutex = 0 onda je this->partition zauzeta
	this->partition = partition;
	partition->readCluster(0, (char*)bitVectorPtr);
	partition->readCluster(1, (char*)dir1Lvl);
	return 1;
}

char KernelFS::unmount() {
	if (this->partition == nullptr) return 0;
	else {
		std::unique_lock<std::mutex> lock(this->m);
		unmountingInProgress = true;
		//while (this->getOpenedFilesNo() > 0) myWait(this->openedFilesSem);				//mutex blokira nit ukoliko broj otvorenih fajlova nije 0 - openedFilesSem je prvobitno = 0. Kad god se neki fajl zatvori, on signalizira openedFilesSem, i broj otvorenih fajlova se opet proverava
		cvAllFilesClosed.wait(lock, [&]() {return this->getOpenedFilesNo() == 0; });
		this->partition = 0;
		mySignal(this->mutexMountPartition);													//signalizira se da nijedna particija nije mounted
		unmountingInProgress = false;
		return 1;
	}
}

char KernelFS::format()
{
	if (this->partition == nullptr || formatingPartition) return 0;
	else {
		std::unique_lock<std::mutex> lock(this->m);
		this->formatingPartition = true;												//kako sam stavio da samo 1 nit moze da udje u 1 metodu KernelFS-a, ovo je suvisno, ali ispravi to kad budes imao projekat koji radi ovako
		//while (this->getOpenedFilesNo() > 0) myWait(this->openedFilesSem);
		cvAllFilesClosed.wait(lock, [&]() {return this->getOpenedFilesNo() == 0; });
		//this->partition->writeCluster(0, (const char*)&BitVector());				/////////////////////////////////////PROVERI
		bitVectorPtr->setAllFREE();
		this->partition->writeCluster(0, (const char*) bitVectorPtr);

		//333333333333333333
		dir1Lvl->setZeros();
		//333333333333333333
		//333333333333333333this->partition->writeCluster(1, (const char*)&Index());					/////////////////////////////////////PROVERI
		this->partition->writeCluster(1, (const char*) this->dir1Lvl);
		this->formatingPartition = false;
	}
	return 1;
}

FileCnt KernelFS::readRootDir() {
	if (this->partition == nullptr)
		return -1;
	//Index dirIndex = Index();
	//this->partition->readCluster(1, (char *)&dirIndex);
	return dir1Lvl->readRootDir(this->partition);
}

char KernelFS::doesExist(char * fname, FileFamilyTree* fft) {//, int* cluserNo_, unsigned long* size_) {
	if (this->partition == nullptr)
		return 0;
	//Index dirIndex = Index();
	//this->partition->readCluster(1, (char *)&dirIndex);
	if (fname[0] == '/') {
		std::memcpy(fname, fname + 1, strlen(fname) - 1);								//jer se u doesExist prosledjuje apsolutna putanja npr. /fajl.txt a ove ostale "dublje" funkcije rade bez ovog / za root
		fname[strlen(fname) - 1] = '\0';
	}
	return dir1Lvl->doesExist(fname, this->partition, fft);//cluserNo_, size_);
}

void KernelFS::pushInThreadVectors(char* fname, char mode, File* file, unsigned long cursor) {
	std::map<DWORD, std::vector<ProcesFCB>*>::iterator it = threads.find(GetCurrentThreadId());
	if (it == threads.end()) {
		//u slucaju da ne postoji ulaz za dati thread, napravimo ga, alocirali 	smo u vektoru 20 mesta za ProcesFCB-ove, 
		//da se ne bi resizeovao kad dodajemo novi fajl stalno
		std::vector<ProcesFCB>* procesFCBVect = new std::vector<ProcesFCB>;
		procesFCBVect->reserve(20);
		threads.insert(std::pair<DWORD, std::vector<ProcesFCB>*>(GetCurrentThreadId(), procesFCBVect));
		procesFCBVect->emplace_back(fname, mode, file, cursor);
	}
	else {
		(*it).second->emplace_back(fname, mode, file, cursor);
	}
}

File * KernelFS::open(char * fname, char mode) {
	if (this->formatingPartition)													//trenutno se vrsi formatiranje particije
		return nullptr;

	std::unique_lock<std::mutex> lock(this->m);				//ne treba mu unlock, jer se pri unistavanju unique_lock-a automatski unlock-uje

	if (fname[0] == '/') {
		std::memcpy(fname, fname + 1, strlen(fname) - 1);
		fname[strlen(fname) - 1] = '\0';
	}

	switch (mode) {
	case 'r':
	{
		//Moramo zastititi ovaj deo od simultanog ulazenja vise tredova, da ne vide 2 treda da isti objekat nije otvorenpa posle da oba krenu da citaju sa
		//diska, nego ako vec nije otvoren, da se onda 1 thred pomuci i potrazi ga sa diska, a 2. tred koji ce posle da udje ce ga naci kao otvoren
		
		//*******************wait(mutexMonitor); ///mislim da ovo ne mora, jer svaki fajl ima svoj mutex, najbolje prepisi sa kdp-a

		// Prvo proverimo da nije vec otvoren da ne bismo trazili po hard disku ponovo, jer je to skupa operacija
		
		
		
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		bool mustWait = false;  //promenjljiva koja govori da li ce se cekati ukoliko je neko vec otvorio za pisanje fajl
		
		std::vector<std::tuple<char*, File*, char>>::iterator it;
		for (it = openedFiles.begin(); it != openedFiles.end(); it++) {
			if (memcmp(std::get<0>(*it), fname, strlen(std::get<0>(*it))) == 0) {
				File* file;		// = new File();
				file = std::get<1>(*it);
				//proveravamo da li neki proces upisuje, ako upisuje, cekamo na writingSemaphor-u iz KernelFile-a

				//upisujemo ga u procesFCBVect odgovarajuceg procesa(iz threads-a)
				
				//---------------------------
				if (std::get<2>(*it) == 'r') { 					//ukoliko je vec otvoren i nije otvoren za upisivanje, moze da ga otvori jos jedan proces za citanje
					////////////////////////////////////////////////////////////////OOOO
					FileFamilyTree* fft = new FileFamilyTree();
					this->doesExist(fname, fft);
					file = new File();
					file->myImpl = new KernelFile(fft->file1LvlCluster, fft->size);
					delete fft;
					////////////////////////////////////////////////////////////////OOOO
					this->pushInThreadVectors(fname, mode, file);
					return file;
				}
				mustWait = true;
				//------------------------------
				//**************mySignal(mutexMonitor);
				//return file;
			}
		}

		//********************
		File* file = nullptr;
		if (mustWait) {		//to znaci da smo ga nasli medju otvorenima, ali da ne moze da se otvori odmah zato sto ga je neko otvorio i za citanje
			this->cvCanOpen.wait(lock, 
				[&]() {
					std::vector<std::tuple<char*, File*, char>>::iterator it;
					for (it = openedFiles.begin(); it != openedFiles.end(); it++) {
						if (memcmp(std::get<0>(*it), fname, strlen(std::get<0>(*it))) == 0) {
							//File* file;		// = new File();
							//file = std::get<1>(*it);
							
							//---------------------------
							if (std::get<2>(*it) == 'r') { 					//ukoliko je vec otvoren i nije otvoren za upisivanje, moze da ga otvori jos jedan proces za citanje
								////////////////////////////////////////////////////////////////OOOO
								FileFamilyTree* fft = new FileFamilyTree();
								this->doesExist(fname, fft);
								file = new File();
								file->myImpl = new KernelFile(fft->file1LvlCluster, fft->size);
								delete fft;
								////////////////////////////////////////////////////////////////OOOO
								this->pushInThreadVectors(fname, mode, file);
								return true;
							}
							else {
								return false;
							}
						}
					}
					//ukoliko se pri notify_all u openedFiles vise ne nalazi ni jedan fajl, moze se otvoriti ovaj fajl cije je otvaranje bilo blokirano jer je ranije bio taj fajl otvoren u modu koji nije 'r'
					FileFamilyTree* fft = new FileFamilyTree();
					if (this->doesExist(fname, fft)) { //&clusterNo, &size)) {
						file = new File();
						file->myImpl = new KernelFile(fft->file1LvlCluster, fft->size);

						this->pushInThreadVectors(fname, mode, file);	//ovo je isto sto i ovo ispod, napravio sam funkciju za to, izbrisi kad proveris lepo

						//ubacimo ga u vektor otvorenih fajlova
						openedFiles.emplace_back(fname, file, mode); /////////(std::pair<char*, File*>(fname, file));

						delete fft;
						return true;
					}
					//else {
					//	delete fft;
					//	return false;
					//}
					
					//this->pushInThreadVectors(fname, mode, file);
					//return true;
				});
		}
		if (mustWait) {		//ako je ovo tacno, znaci da su sigurno obavili posao u ovoj lambdi gore
			return file;
		}
		//********************
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!



		//Ako ne postoji u otvorenim, moramo traziti u hard disku
		FileFamilyTree* fft = new FileFamilyTree();
		if (this->doesExist(fname, fft)) { //&clusterNo, &size)) {
			File* file = new File();
			file->myImpl = new KernelFile(fft->file1LvlCluster, fft->size);
			//std::cout << "***********" << fft->size << "***********" << std:: endl;
			//66666666666666666666666666
			//this->doesExist(fname, fft);
			//66666666666666666666666666


			this->pushInThreadVectors(fname, mode, file);	//ovo je isto sto i ovo ispod, napravio sam funkciju za to, izbrisi kad proveris lepo

			//ubacimo ga u vektor otvorenih fajlova
			openedFiles.emplace_back(fname, file, mode); /////////(std::pair<char*, File*>(fname, file));
			mySignal(mutexMonitor);

			delete fft;
			return file;
		}
		else {
			delete fft;
			return nullptr;
		}
	}
	break;
	case 'w':
	{
		//***************************************wait(mutexMonitor);
		// Prvo proverimo da nije vec otvoren da ne bismo trazili po hard disku odmah jer je to skupa operacija,
		//pa prvo ovde vidimo da nije vec otvoren, jer ako je otvoren, znaci da sigurno postoji, pa onda ne moramo da trazimo po disku.
		//ako ga ne nadjemo ovde, moramo da proverimo da ne postoji na disku fajl takvog naziva i da ga obrisemo.



		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		bool fileExist = false;
		
		std::vector<std::tuple<char*, File*, char>>::iterator it;
		for (it = openedFiles.begin(); it != openedFiles.end(); it++) {
			if (memcmp(std::get<0>(*it), fname, strlen(fname)) == 0) {
				fileExist = true;

				
				//*********************************************************************************************************
				//OVDE BI TREBALO ODRADITI NEKU SINHRONIZACIJU DA SE CEKA AKO JE DATI FAJL KOJI SE BRISE OTVOREN

				//*********************************************************************************************************
				break;
			}
		}
		if (fileExist) {		//to znaci da smo ga nasli medju otvorenima, ali da ne moze da se otvori odmah zato sto ga je neko otvorio za citanje ili pisanje
			this->cvCanOpen.wait(lock,
				[&]() {
					std::vector<std::tuple<char*, File*, char>>::iterator it;
					for (it = openedFiles.begin(); it != openedFiles.end(); it++) {
						if (memcmp(std::get<0>(*it), fname, strlen(std::get<0>(*it))) == 0) {
							return false;		//ako i dalje postoji medju otvorenima, i dalje se ceka
						}
					}
					return true;				//ako vise nije medju otvorenima, mozemo da nastavimo dalje i da ga obrisemo ukoliko je samo na disku
				});
		}
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 



		FileFamilyTree* fft = new FileFamilyTree();
		if (this->doesExist(fname, fft)) {  //, nullptr, nullptr)) {
			this->deleteFile(fname, fft);
		}
		delete fft;

		////////////////////

		File* file = new File();										//fajl koji cemo vratiti kad ga napravimo (ukoliko se nesto ne izjalovi pa moramo da vratimo nullptr)
		//file->myImpl = new KernelFile(fft->file1LvlCluster, fft->size);

		//BitVector* bitVector = new BitVector();
		//partition->readCluster(0, (char*) bitVector);		vec imamo bitVectorPtr koji je lokalna verzija toga
		//3333333333333333333Index* dir1Lvl = new Index();
		//3333333333333333333partition->readCluster(1, (char*) dir1Lvl);


		int dir1LvlLastEntry = dir1Lvl->getLastEntryNo();
		if (dir1LvlLastEntry == -1) {						//znaci da je tabela prazna
			int freeClusterNo = bitVectorPtr->getFirstFreeClusterNo(partition);//;) //pronalazimo slobodno mesto za index direktorijuma drugog nivoa
			dir1Lvl->table[0] = freeClusterNo;
			//bitVector->useBitVector(freeClusterNo, partition);
			Index* dir2Lvl = new Index();
			//partition->readCluster(freeClusterNo, (char*)& dir2Lvl);	//ovo mi ne treba jer je nulti ulaz i u nultom ulazu nema sta da citamo sta je prethodno bilo u clusteru
			int freeClusterNo2 = bitVectorPtr->getFirstFreeClusterNo(partition);//;)		//pronalazimo slobodno mesto za klaster za podatke direktorijuma
			dir2Lvl->table[0] = freeClusterNo2;
			//bitVector->useBitVector(freeClusterNo2, partition);
			//Dovucemo data deo i u prvi ulaz upisemo potrebne podatke
			Data* dirData = new Data();
			//partition->readCluster(freeClusterNo2, (char*)& dirData); //ovo mi ne treba jer je nulti ulaz i u nultom ulazu nema sta da citamo sta je
			dirData->setFullFileName(0, fname);
			int file1LvlClusterNo = bitVectorPtr->getFirstFreeClusterNo(partition);//;)
			dirData->setIndexClusterNo(0, file1LvlClusterNo);
			//bitVector->useBitVector(file1LvlClusterNo, partition);
			Index* file1Lvl = new Index();
			file->myImpl = new KernelFile(file1LvlClusterNo, 0);

			this->pushInThreadVectors(fname, mode, file);
			openedFiles.emplace_back(fname, file, mode);

			//dodaj u Vector otvorenih fajlova i u vektor otvorenih fajlova ove niti

			//ooothis->partition->writeCluster(1, (char*) dir1Lvl);
			this->partition->writeCluster(freeClusterNo, (char*) dir2Lvl);
			this->partition->writeCluster(freeClusterNo2, (char*) dirData);
			this->partition->writeCluster(file1LvlClusterNo, (char*) file1Lvl);

			delete dir2Lvl;
			delete dirData;
			delete file1Lvl;
		}
		else {
			Index* dir2Lvl = new Index();
			partition->readCluster(dir1Lvl->table[dir1LvlLastEntry], (char*) dir2Lvl);
			int dir2LvlLastEntry = dir2Lvl->getLastEntryNo();
			Data* dirData = new Data();
			partition->readCluster(dir2Lvl->table[dir2LvlLastEntry], (char*) dirData);
			int dirDataLastEntry = dirData->getLastEntryNo();
			if (dirDataLastEntry == 63) {													//to znaci da je data puna, i da moramo da proverimo da li je naredni ulaz u indexu drugog levela particije slobodan. Nismo to ranije proveravali u dir2Lvl i dir1Lvl jer je mozda slobodno u dirData i dir2Lvl respektivno
				if (dir2LvlLastEntry == 511) {												//to znaci da je ovo bio poslednji ulaz u dir2Lvl i da je pun, znaci moramo u dir1Lvl i novi dir2Lvl da zauzmemo
					if (dir1LvlLastEntry + 1 == 511) {
						//delete bitVector;
						//3333333333333333333delete dir1Lvl;
						delete dir2Lvl;
						return nullptr;
					}
					int freeClusterNo4 = bitVectorPtr->getFirstFreeClusterNo(partition);//;)
					dir1Lvl->table[++dir1LvlLastEntry] = freeClusterNo4;
					partition->readCluster(freeClusterNo4, (char*) dir2Lvl);

					dir2LvlLastEntry = dir2Lvl->getLastEntryNo();
					int freeClusterNo5 = bitVectorPtr->getFirstFreeClusterNo(partition);//;)
					dir2Lvl->table[++dir2LvlLastEntry] = freeClusterNo5;
					partition->readCluster(freeClusterNo5, (char*) dirData);

					dirDataLastEntry = dirData->getLastEntryNo();
					dirData->setFullFileName(dirDataLastEntry + 1, fname);
					int file1LvlClusterNo = bitVectorPtr->getFirstFreeClusterNo(partition);//;)
					dirData->setIndexClusterNo(dirDataLastEntry + 1, file1LvlClusterNo);
					//bitVector->useBitVector(file1LvlClusterNo, partition);
					Index* file1Lvl = new Index();
					file->myImpl = new KernelFile(file1LvlClusterNo, 0);

					this->pushInThreadVectors(fname, mode, file);
					openedFiles.emplace_back(fname, file, mode);

					//ooothis->partition->writeCluster(1, (char*) dir1Lvl);
					this->partition->writeCluster(dir1Lvl->table[dir1LvlLastEntry], (char*) dir2Lvl);
					this->partition->writeCluster(dir2Lvl->table[dir2LvlLastEntry], (char*) dirData);
					this->partition->writeCluster(file1LvlClusterNo, (char*) file1Lvl);

					delete file1Lvl;
				}
				else {																		//nema mesta u starom dirData, pa alociramo novi
					int freeClusterNo3 = bitVectorPtr->getFirstFreeClusterNo(partition);//;)
					dir2Lvl->table[++dir2LvlLastEntry] = freeClusterNo3;
					partition->readCluster(freeClusterNo3, (char*) dirData);

					dirDataLastEntry = dirData->getLastEntryNo();
					dirData->setFullFileName(dirDataLastEntry + 1, fname);
					int file1LvlClusterNo = bitVectorPtr->getFirstFreeClusterNo(partition);//;)
					dirData->setIndexClusterNo(dirDataLastEntry + 1, file1LvlClusterNo);
					//bitVector->useBitVector(file1LvlClusterNo, partition);										//ovde bi mozda trebalo da upisem u memoriju i file1lvlIndex da bi bio inicijalizovan u memoriji sa nulama 
					Index* file1Lvl = new Index();
					file->myImpl = new KernelFile(file1LvlClusterNo, 0);

					this->pushInThreadVectors(fname, mode, file);
					openedFiles.emplace_back(fname, file, mode);
					
					this->partition->writeCluster(dir1Lvl->table[dir1LvlLastEntry], (char*) dir2Lvl);
					this->partition->writeCluster(dir2Lvl->table[dir2LvlLastEntry], (char*) dirData);
					this->partition->writeCluster(file1LvlClusterNo, (char*) file1Lvl);

					delete file1Lvl;
				}
			}
			else {																			//nalazimo slobodan klaster za index prvog nivoa fajla, upisujemo ga u odgovarajuce polje data direktorijuma i zauzimamo ga u bitVectoru
				dirData->setFullFileName(dirDataLastEntry + 1, fname);
				int file1LvlClusterNo = bitVectorPtr->getFirstFreeClusterNo(partition);//;)
				dirData->setIndexClusterNo(dirDataLastEntry + 1, file1LvlClusterNo);
				//bitVector->useBitVector(file1LvlClusterNo, partition);
				Index* file1Lvl = new Index();
				file->myImpl = new KernelFile(file1LvlClusterNo, 0);

				this->partition->writeCluster(dir2Lvl->table[dir2LvlLastEntry], (char*) dirData);
				this->partition->writeCluster(file1LvlClusterNo, (char*) file1Lvl);

				this->pushInThreadVectors(fname, mode, file);
				openedFiles.emplace_back(fname, file, mode);

				delete file1Lvl;
			}

			delete dir2Lvl;
			delete dirData;
		}
		//ooothis->partition->writeCluster(0, (char*) bitVectorPtr);
		//delete bitVector;
		//3333333333333333333delete dir1Lvl;
		return file;
	}
	break;
	case 'a':
	{
		///////////////////////////////////////

		//Moramo zastititi ovaj deo od simultanog ulazenja vise tredova, da ne vide 2 treda da isti objekat nije otvorenpa posle da oba krenu da citaju sa
		//diska, nego ako vec nije otvoren, da se onda 1 thred pomuci i potrazi ga sa diska, a 2. tred koji ce posle da udje ce ga naci kao otvoren
		//myWait(mutexMonitor); ///mislim da ovo ne mora, jer svaki fajl ima svoj mutex, najbolje prepisi sa kdp-a
		
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		bool fileExist = false;

		std::vector<std::tuple<char*, File*, char>>::iterator it;
		for (it = openedFiles.begin(); it != openedFiles.end(); it++) {
			if (memcmp(std::get<0>(*it), fname, strlen(fname)) == 0) {
				fileExist = true;


				//*********************************************************************************************************
				//OVDE BI TREBALO ODRADITI NEKU SINHRONIZACIJU DA SE CEKA AKO JE DATI FAJL KOJI SE BRISE OTVOREN

				//*********************************************************************************************************
				break;
			}
		}
		if (fileExist) {		//to znaci da smo ga nasli medju otvorenima, ali da ne moze da se otvori odmah zato sto ga je neko otvorio za citanje ili pisanje
			this->cvCanOpen.wait(lock,
				[&]() {
					std::vector<std::tuple<char*, File*, char>>::iterator it;
					for (it = openedFiles.begin(); it != openedFiles.end(); it++) {
						if (memcmp(std::get<0>(*it), fname, strlen(std::get<0>(*it))) == 0) {
							return false;		//ako i dalje postoji medju otvorenima, i dalje se ceka
						}
					}
					return true;				//ako vise nije medju otvorenima, mozemo da nastavimo dalje i da ga obrisemo ukoliko je samo na disku
				});
		}



		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		//Ako ne postoji u otvorenim, moramo traziti u hard disku
		FileFamilyTree* fft = new FileFamilyTree();
		if (this->doesExist(fname, fft)) { //&clusterNo, &size)) {
			File* file = new File();
			file->myImpl = new KernelFile(fft->file1LvlCluster, fft->size);

			this->pushInThreadVectors(fname, mode, file, fft->size);//file->getFileSize());	//ovo je isto sto i ovo ispod, napravio sam funkciju za to, izbrisi kad proveris lepo

			//ubacimo ga u vektor otvorenih fajlova
			openedFiles.emplace_back(fname, file, mode); /////////(std::pair<char*, File*>(fname, file));
			//******************signal(mutexMonitor);

			delete fft;
			return file;
		}
		else {
			delete fft;
			return nullptr;
		}
	}
	break;
	default:
		break;
	}
	return nullptr;
}

char KernelFS::deleteFile(char* fname, FileFamilyTree* fft) {
	bool fileExisist = true;
	if (fft == nullptr) {
		fft = new FileFamilyTree();
		fileExisist = this->doesExist(fname, fft);
	}

	if (fileExisist) {
		//BitVector bitVector = BitVector();
		//this->partition->readCluster(0, (char*)& bitVector);
		Index file1LvlCluster = Index();
		this->partition->readCluster(fft->file1LvlCluster, (char*)&file1LvlCluster);

		for (int i = 0; i < 512; i++) {
			if (file1LvlCluster.table[i] == 0)
				break;
			int lvl2Entry = file1LvlCluster.table[i];
			Index file2LvlCluster = Index();
			this->partition->readCluster(lvl2Entry, (char*)& file2LvlCluster);

			for (int j = 0; j < 512; j++) {
				if (file2LvlCluster.table[j] == 0)
					break;
				else {
					bitVectorPtr->freeBitVector(file2LvlCluster.table[j]);					//oslobodio sam sve data fajl klastere koji su bili zauzeti
				}
			}
			bitVectorPtr->freeBitVector(lvl2Entry);
		}
		bitVectorPtr->freeBitVector(fft->file1LvlCluster);

		//defragmentacija bitna zato sto po tabelama pretrazujem sve dok ne naidjem na ulaz koji je prazan - vise o tome ispod, skapiraces kad procitas

		//ovo gore je bilo brisanje fajla, sada sledi brisanje ulaza za taj fajl, defragmentacija, i potencijalno oslobadjanje celog indexa direktorijuma
		Data dirData = Data();
		this->partition->readCluster(fft->dirDataCluster, (char*)& dirData);
		int lastUsedEntry = dirData.getLastEntryNo();
		int fileEntry = dirData.findEntry(fft->file1LvlCluster);				//imamo broj klastera data direktorijuma, ali ne znamo koji je broj ulaza u tabeli zapisa o fajlovima, to trazimo pa cemo da oslobodimo taj ulaz i defragmentujemo(stavimo ga na zadnje mesto, da ne bude praznih ulaza u sredini
		Data::Entry tmp = dirData.table[fileEntry];				
		dirData.table[fileEntry] = dirData.table[lastUsedEntry];
		dirData.table[lastUsedEntry] = tmp;
		dirData.table[lastUsedEntry].name[0] = 0;
		if (lastUsedEntry == 0) {											//ako je zadnji entry koji je imao neki zapis u tabeli, i sam ovaj sto se brisao, to znaci da je tabela sad prazna i da treba da se oslobodi ceo klaster i to radimo u bitVektoru
			bitVectorPtr->freeBitVector(fft->dirDataCluster);
		}
			
		//ovo gore sam updateovao deo sa podacima direktorijuma, sada cu da update index 2. nivoa direktorijuma, po istom principu samo sa Index placeholderom
		Index dir2Lvl = Index();
		this->partition->readCluster(fft->dir2LvlCluster, (char*)& dir2Lvl);
		int lastUsedEntry2 = dir2Lvl.getLastEntryNo();
		int fileEntry2 = dir2Lvl.findEntry(fft->dirDataCluster);
		int tmp2 = dir2Lvl.table[lastUsedEntry2];
		dir2Lvl.table[fileEntry2] = tmp2;
		dir2Lvl.table[lastUsedEntry2] = 0;
		if (lastUsedEntry2 == 0) {
			bitVectorPtr->freeBitVector(fft->dir2LvlCluster);
		}

		//ovo gore je update indexa 2. nivoa direktorijuma, sada ide update 1. nivoa, on se oslobadja ukoliko se oslobodio ceo ovaj gore klaster
		//3333333333333333333Index dir1Lvl = Index();
		//3333333333333333333this->partition->readCluster(1, (char*)& dir1Lvl);
		if (lastUsedEntry2 == 0) {											//ako se oslobodio ceo klaster 2. nivoa, ulaz u prvom nivou koji je "pokazivao" na njega mora da se stavi na 0 i da se klaster defragmentuje
			int lastUsedEntry3 = dir1Lvl->getLastEntryNo();
			int fileEntry3 = dir1Lvl->findEntry(fft->dirDataCluster);
			int tmp3 = dir1Lvl->table[lastUsedEntry3];
			dir2Lvl.table[fileEntry3] = tmp2;
			dir2Lvl.table[lastUsedEntry3] = 0;
		}

		//posto sam citao sve ove klastere koje sam menjao, sada treba da ih vratim u hard disk, da bi podaci bili konzistentni
		//ooothis->partition->writeCluster(1, (char*) dir1Lvl);
		this->partition->writeCluster(fft->dir2LvlCluster, (char*)& dir2Lvl);
		this->partition->writeCluster(fft->dirDataCluster, (char*)& dirData);
		//ooothis->partition->writeCluster(0, (char*) bitVectorPtr);
		
		//sad treba da obrisem fajlove iz vektora procesa, i potencijalno iz vektora otvorenih fajlova
		unsigned long threadId = GetCurrentThreadId();
		unsigned long fileReferences = 0;												//povecava se ukoliko postoji jos neki proces osim ovog koji je otvorio fajl
		for (auto it = this->threads.begin(); it != this->threads.end(); it++) {
			auto it2 = std::find_if(it->second->begin(), it->second->end(), 
				[&fname](ProcesFCB& procFCB) {
					return strcmp(procFCB.name, fname) == 0;
				});

			if (it2 != it->second->end() && it->first != threadId) {
				fileReferences++;														//povecava se ukoliko postoji jos neki proces osim ovog koji je otvorio fajl
			}

			if (it->first == threadId && it2 != it->second->end())  {
				it->second->erase(it2);
			}
		}

		if (fileReferences < 1) {
			auto it = std::find_if(openedFiles.begin(), openedFiles.end(),
				[&fname](std::tuple<char*, File*, char> elem) {
					return strcmp(std::get<0>(elem), fname) == 0;
				});

			if (it != openedFiles.end()) {
				openedFiles.erase(it);
			}
		}
	}
	
	delete fft;
	return 0;
}

int KernelFS::getOpenedFilesNo() {
	return openedFiles.size();
}
