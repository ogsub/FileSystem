#define _CRT_SECURE_NO_WARNINGS
#include "Data.h"

std::mutex Data::m;

Data::Data()
{
}


Data::~Data()
{
}

bool Data::compareFileName(char* fullFileName, int entry) {
	std::unique_lock<std::mutex> lock(m);
	if (strlen(fullFileName) > 12)														//max velicina naziva je 12 = 8(samo naziv) + '.' + 3(ekstenzija)
		return false;
	char* name = this->getFName(entry);
	char* extension = this->getFExtension(entry);
	char fullName[20];
	memcpy(fullName, name, strlen(name));
	memcpy(fullName + strlen(name), ".", 1);
	memcpy(fullName + strlen(name) + 1, extension, strlen(extension));
	memcpy(fullName + strlen(name) + 1 + strlen(extension), "\0", 1);
	if (memcmp(fullFileName, fullName, strlen(fullName)) == 0) { 
		return true; 
	}
	return false;
}


char* Data::getFName(int entry) {
	return table[entry].name;
}

char* Data::getFExtension(int entry) {
	return table[entry].extension;
}

int Data::getIndexClusterNo(int entry) {
	return table[entry].indexClusterNo;
}

int Data::getLastEntryNo() {
	for (int i = 0; i < 64; i++) {
		if (isEntryEmpty(i))
			return i - 1;
	}
	return 63;
}

int Data::getFileSize(int entry) {
	return table[entry].fileSize;
}

bool Data::isEntryEmpty(int entry) {
	char c = 0;
	if (std::memcmp(&c, this->getFName(entry), 1) == 0) return true;
	return false;
}

void Data::setFullFileName(int entry, char* fullFileName) {
	if (strlen(fullFileName) > 12)
		return;

	//koristim tmp umesto fullFileName da ne bih "iskasapio" fullFileName, jer ga ovaj strtok() isece na 2 dela pa samo 1. deo ostane
	char* tmp = new char[strlen(fullFileName) + 1];
	strcpy(tmp, fullFileName);
	tmp[strlen(fullFileName)] = '\0';

	if (tmp[0] == '/') {
		std::memcpy(tmp, tmp + 1, strlen(tmp) - 1);
		tmp[strlen(tmp) - 1] = '\0';
	}

	char* tok;
	tok = std::strtok(tmp, ".");
	std::memcpy(table[entry].name, tok, strlen(tok));
	for (int i = strlen(tok) + 1; i < 8; i++) {
		table[entry].name[i] = ' ';
	}
	tok = std::strtok(NULL, "");
	std::memcpy(table[entry].extension, tok, strlen(tok));
	for (int i = strlen(tok) + 1; i < 3; i++) {
		table[entry].extension[i] = ' ';
	}
	return;
}

void Data::setIndexClusterNo(int entry, int indexClusterNo) {
	table[entry].indexClusterNo = indexClusterNo;
}

void Data::setFileSize(int entry, int fileSize) {
	table[entry].fileSize = fileSize;
}

int Data::findEntry(int indexClusterNo) {
	for (int i = 0; i < 64; i++) {
		if (table[i].indexClusterNo == indexClusterNo)
			return i;
	}
	return -1;
}
