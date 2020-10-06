#include "file.h"
#include "KernelFile.h"
File::File() {
}

File::~File() {										//ne zaboravi da signaliziras openedFileSem u KernelFS 
	//std::unique_lock<std::mutex> lock(this->myImpl->m);
	myWait(this->myImpl->mutexRW);
	this->myImpl->setFileToCloseAddr(this);
	delete myImpl;
	mySignal(this->myImpl->mutexRW);
}

char File::write(BytesCnt bytesCnt, char * buffer) {
	//std::unique_lock<std::mutex> lock(this->myImpl->m);
	myWait(myImpl->mutexRW);
	char ret =  this->myImpl->write(bytesCnt, buffer, this);
	mySignal(myImpl->mutexRW);
	//lock.unlock();
	return ret;
}

BytesCnt File::read(BytesCnt byteNo, char * buffer) {
	//std::unique_lock<std::mutex> lock(this->myImpl->m);
	//myWait(myImpl->mutexRW);
	BytesCnt ret = this->myImpl->read(byteNo, buffer, this);
	//mySignal(myImpl->mutexRW);
	//lock.unlock();
	return ret;
}

char File::seek(BytesCnt byteNo) {
	//std::unique_lock<std::mutex> lock(this->myImpl->m);
	myWait(myImpl->mutexRW);
	char ret = this->myImpl->seek(byteNo, this);
	mySignal(myImpl->mutexRW);
	return ret;
}

BytesCnt File::filePos() {
	std::unique_lock<std::mutex> lock(this->myImpl->m);
	return this->myImpl->filePos(this);
}

char File::eof() {
	//std::unique_lock<std::mutex> lock(this->myImpl->m);
	//myWait(myImpl->mutexRW);
	char ret =  this->myImpl->eof(this);
	//mySignal(myImpl->mutexRW);
	return ret;
}

BytesCnt File::getFileSize() {
	std::unique_lock<std::mutex> lock(this->myImpl->m);
	return myImpl->getFileSize(this);
}

char File::truncate() {
	std::unique_lock<std::mutex> lock(this->myImpl->m);
	return this->myImpl->truncate(this);
}

