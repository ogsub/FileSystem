#pragma once

struct Cluster {
	int table[512] = { 0 };											//koriscen kao "glup" podatak, broj ulaza u jednom klasteru. Tu se nalazi broj indexa 2. nivoa ako je u pitanju tabela indeksa prvog nivoa, ili klaster podataka ako je u pitanju tablea 2. nivoa
};