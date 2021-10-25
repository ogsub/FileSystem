#pragma once
#include "fs.h"
#include "part.h"
#include "semaphoreMacros.h"
#include "bitVector.h"
#include "Index.h"
#include <vector>
#include <map>
#include <utility>
#include "FileFamilyTree.h"
#include "file.h"

#include <mutex>
#include <condition_variable>

class Partition;

class KernelFS {
public:
	KernelFS();
	~KernelFS();

	char mount(Partition* partition);
	char unmount();
	char format();
	FileCnt readRootDir();
	char doesExist(char* fname, FileFamilyTree* fft = nullptr); //int* cluserNo, unsigned long* size);					//ako se proslede clusterNo i size, preko pokazivaca se upise vrednost klastera i velicine nadjenog fajla

	File* open(char* fname, char mode);
	char deleteFile(char* fname, FileFamilyTree* fft = nullptr);

	//HANDLE mutexOuter = CreateSemaphore(NULL, 1, 32, NULL);
	//HANDLE mutexInner = CreateSemaphore(NULL, 1, 32, NULL);
	HANDLE mutexMonitor;

	static std::mutex m;
	static std::condition_variable cvAllFilesClosed;
	static std::condition_variable cvCanOpen;

	struct ProcesFCB {
		unsigned long cursor;		//dokle je stigao odredjeni proces(nit) u fajlu(gde se nalazi cursor, vertilakna treptajuca linija)
		char *name;
		char mode;		//mod u kojem je nit otvorila ovaj fajl (r,w..)
		File *file;		//svaki proces koji je otvorio isti fajl mora da sadrzi isti pokazivac na taj faj, samo sa drugacijim modom i potencijalno kursorom

		ProcesFCB(char* name, char mode, File* file, int cursor = 0) {
			this->cursor = cursor;
			this->name = name;
			this->mode = mode;
			this->file = file;
		}
	};

	

	static std::map<DWORD, std::vector<ProcesFCB>*> threads;							//mapa svih procesa(niti) sa pokazivacima na vektor FCB-ova tog procesa
	static std::vector<std::tuple<char*, File*, char>> openedFiles;							//vektor otvorenih fajlova par naziv, pointer na fajl

	
	static int getOpenedFilesNo();
	static HANDLE openedFilesSem;
	static bool unmountingInProgress;

	static BitVector* bitVectorPtr;

	static Index* dir1Lvl;

	static Partition* partition; //= nullptr;
private:
	void pushInThreadVectors(char* fname, char mode, File* file, unsigned long cursor = 0);

	HANDLE mutexMountPartition;
	bool formatingPartition = false;
	
};

