#include "fs.h"
#include "KernelFS.h"
#include <mutex>

KernelFS * FS::myImpl = new KernelFS();

FS::FS() {
}

FS::~FS() {
}

char FS::mount(Partition * partition) {
	std::unique_lock<std::mutex> lock(myImpl->m);
	char ret = myImpl->mount(partition);
	return ret;
}

char FS::unmount() {
	char ret = myImpl->unmount();
	return ret;
}

char FS::format() {
	char ret = myImpl->format();
	return ret;
}

FileCnt FS::readRootDir() {
	std::unique_lock<std::mutex> lock(myImpl->m);
	FileCnt ret = myImpl->readRootDir();
	lock.unlock();
	return ret;
}

char FS::doesExist(char * fname) {
	std::unique_lock<std::mutex> lock(myImpl->m);
	char ret = myImpl->doesExist(fname);	
	lock.unlock();
	return ret;    
}

File * FS::open(char * fname, char mode) {
	File* ret = myImpl->open(fname, mode);
	return ret;
}

char FS::deleteFile(char * fname)
{
	return 0;
}
