/*

#define _CRT_SECURE_NO_WARNINGS
#include "part.h"
#include <iostream>

#include "part.h"
#include<bitset>
#include "bitVector.h"
#include "Cluster.h"
#include "Data.h"
#include "Index.h"
#include <cstring>
#include <vector>

void fja(int& a) {
	a = 5;
}

struct ProcesFCB2 {
	int cursor;
	const char * name;
	char mode;
	File* file;
	ProcesFCB2() {
		std::cout << "poziva se\n";
	}
	ProcesFCB2(const char* name, char mode, File* file) {
		std::cout << "poziva se2\n";
		this->cursor = 0;
		this->name = name;
		this->mode = mode;
		this->file = file;
	}
};



int main() {
	char fullName[20];
	fullName[0] = 'a';
	fullName[1] = 'b';
	std::memcpy(fullName + 2, "\0", 1);
	std::cout << strlen(fullName) << std::endl;
	fullName[2] = '\0';
	std::cout << strlen(fullName) << "____________________" <<std::endl;
	const char* name = "nek";
	const char* extension = "ako";
	memcpy(fullName, name, strlen(name));
	memcpy(fullName+strlen(name), ".", 1);
	memcpy(fullName + strlen(name) + 1, extension, strlen(extension));
	const char* q = "nek.ako";
	if (memcmp(fullName, q, strlen(q)) == 0) std::cout << "AAAAKOKOK";
	for (int i = 0; i < strlen(fullName); i++) {
		std::cout << fullName[i];
	}

	char m[] = "asda";
	Partition* p = new Partition(m);
	char fullName[] = "/nesto.hej";
	if (fullName[0] == '/') {
		memcpy(fullName, fullName + 1, strlen(fullName) - 1);
		fullName[strlen(fullName) - 1] = '\0';
	}
	char *s = strtok(fullName, ".");
	int i = strlen(s);
	char *s2 = strtok(NULL, "");
	int a = 298;
	char* b = (char*)&a;

}

*/
#define _CRT_SECURE_NO_WARNINGS
#include"testprimer.h"

using namespace std;

HANDLE nit1, nit2;
DWORD ThreadID;

HANDLE semMain = CreateSemaphore(NULL, 0, 32, NULL);
HANDLE sem12 = CreateSemaphore(NULL, 0, 32, NULL);
HANDLE sem21 = CreateSemaphore(NULL, 0, 32, NULL);
HANDLE mutex = CreateSemaphore(NULL, 1, 32, NULL);

Partition* partition;

char* ulazBuffer;
int ulazSize;

int main() {
	clock_t startTime, endTime;
	cout << "Pocetak testa!" << endl;
	startTime = clock();//pocni merenje vremena

	{//ucitavamo ulazni fajl u bafer, da bi nit 1 i 2 mogle paralelno da citaju
		FILE* f = fopen("ulaz.dat", "rb");
		if (f == 0) {
			cout << "GRESKA: Nije nadjen ulazni fajl 'ulaz.dat' u os domacinu!" << endl;
			system("PAUSE");
			return 0;//exit program
		}
		ulazBuffer = new char[32 * 1024 * 1024];//32MB
		ulazSize = fread(ulazBuffer, 1, 32 * 1024 * 1024, f);
		fclose(f);
	}

	nit1 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)nit1run, NULL, 0, &ThreadID); //kreira i startuje niti
	nit2 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)nit2run, NULL, 0, &ThreadID);

	for (int i = 0; i < 2; i++) wait(semMain);//cekamo da se niti zavrse
	delete[] ulazBuffer;
	endTime = clock();
	cout << "Kraj test primera!" << endl;
	cout << "Vreme izvrsavanja: " << ((double)(endTime - startTime) / ((double)CLOCKS_PER_SEC / 1000.0)) << "ms!" << endl;
	int i;
	std::cin >> i;
	CloseHandle(mutex);
	CloseHandle(semMain);
	CloseHandle(sem12);
	CloseHandle(sem21);
	CloseHandle(nit1);
	CloseHandle(nit2);
	return 0;
}