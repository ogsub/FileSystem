#include "fs.h"
#include "KernelFS.h"
#include <mutex>

KernelFS * FS::myImpl = new KernelFS();

FS::FS() {
}

FS::~FS() {
}

char FS::mount(Partition * partition) {

	//myWait(myImpl->mutexMonitor);
	std::unique_lock<std::mutex> lock(myImpl->m);
	char ret = myImpl->mount(partition);
	//mySignal(myImpl->mutexMonitor);
	//lock.unlock();
	return ret;
}

char FS::unmount() {
	//wait(myImpl->mutexMonitor);
	//std::unique_lock<std::mutex> lock(myImpl->m);
	char ret = myImpl->unmount();
	//signal(myImpl->mutexMonitor);
	//lock.unlock();
	return ret;
}

char FS::format() {
	//myWait(myImpl->mutexMonitor);
	//std::unique_lock<std::mutex> lock(myImpl->m);
	char ret = myImpl->format();
	//mySignal(myImpl->mutexMonitor);
	//lock.unlock();
	return ret;
}

FileCnt FS::readRootDir() {
	//myWait(myImpl->mutexMonitor);
	std::unique_lock<std::mutex> lock(myImpl->m);
	FileCnt ret = myImpl->readRootDir();
	//mySignal(myImpl->mutexMonitor);
	lock.unlock();
	return ret;
}

char FS::doesExist(char * fname) {
	//myWait(myImpl->mutexMonitor);
	std::unique_lock<std::mutex> lock(myImpl->m);
	char ret = myImpl->doesExist(fname);	//, nullptr, nullptr);//////////////////////////////
	//mySignal(myImpl->mutexMonitor);
	lock.unlock();
	return ret;    
}

File * FS::open(char * fname, char mode) {
	//myWait(myImpl->mutexMonitor);
	//std::unique_lock<std::mutex> lock(myImpl->m);
	File* ret = myImpl->open(fname, mode);
	//mySignal(myImpl->mutexMonitor);
	//lock.unlock();
	return ret;
}

char FS::deleteFile(char * fname)
{
	return 0;
}
