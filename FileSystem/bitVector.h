#pragma once

#include <bitset>

#include <mutex>
#include "semaphoreMacros.h"

#include "part.h"

#define FREE 1
#define USED 0
#define NUMBEROFCLUSTERS 5200
#define NUMOFBITSINCLUSTER 16384

class BitVector
{
public:
	BitVector();
	~BitVector();

	static std::mutex m;
	static HANDLE mutex;

	void useBitVector(int clusterNo, Partition* partition);
	void freeBitVector(int clusterNo);
	int getFreeClusterNo();
	int getUsedClusterNo();
	int getFirstFreeClusterNo(Partition* partition);
	bool isFree(int clusterNo);

	void reset() {		//sve setuje na USED
		bitVector.reset();
	}
	void setAllFREE() {
		bitVector.set();
		bitVector[0] = 0;
		bitVector[1] = 0;
	}
private:
	std::bitset<NUMBEROFCLUSTERS> bitVector;
	char unused[1390];									//padding da dodje do 2KB kolika je velicina cluster-a, ne znam zasto nije 1398, ispadne onda 8 bajta vise
};

