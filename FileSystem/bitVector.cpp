#include "bitVector.h"

std::mutex BitVector::m;

HANDLE BitVector::mutex = CreateSemaphore(NULL, 1, 32, NULL);;

BitVector::BitVector() {
	this->bitVector.set();												//setuje sve bite na 1, jer su svi klasteri slobodni
	this->bitVector[0] = USED;											//setuje bit 0 na 0(used) jer je klaster 0 popunjen bas bitVektorom
	this->bitVector[1] = USED;											//setuje bit 1 na 0(used) jer je klaster 1 popunjen indeksom korenog direktorijuma prvog nivoa
}


BitVector::~BitVector()
{
}

void BitVector::useBitVector(int clusterNo, Partition* partition) { //setBitVectorUsed
	//std::unique_lock<std::mutex> lock(m);
	//myWait(mutex);
	bitVector[clusterNo] = USED;
	//ooopartition->writeCluster(0, (char*) this);
	//mySignal(mutex);
}

void BitVector::freeBitVector(int clusterNo) {
	//std::unique_lock<std::mutex> lock(m);
	myWait(mutex);
	bitVector[clusterNo] = FREE;
	mySignal(mutex);
}

int BitVector::getFreeClusterNo() {
	//std::unique_lock<std::mutex> lock(m);
	myWait(mutex);
	int ret = bitVector.size() - bitVector.count();
	mySignal(mutex);
	return ret;
}

int BitVector::getUsedClusterNo() {
	//std::unique_lock<std::mutex> lock(m);
	myWait(mutex);
	int ret = bitVector.count();
	mySignal(mutex);
	return ret;
}

int BitVector::getFirstFreeClusterNo(Partition* partition) {
	std::unique_lock<std::mutex> lock(m);
	//myWait(mutex);
	//std::unique_lock<std::mutex> lock(m);
	for (int i = 0; i < bitVector.size(); i++) {
		if (bitVector[i] == FREE) { 
			//mySignal(mutex);
			///////////////////////////////////
			bitVector[i] = USED;
			//@@@@@@@@@@@@@@@@@partition->writeCluster(0, (char*)this);
			///////////////////////////////////
			return i; 
		}
	}
}

bool BitVector::isFree(int clusterNo) {
	//std::unique_lock<std::mutex> lock(m);
	myWait(mutex);
	if (bitVector[clusterNo] == FREE) {
		mySignal(mutex);
		return true;
	}
	else {
		mySignal(mutex);
		return false;
	}
}
