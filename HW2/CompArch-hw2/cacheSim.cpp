
/* 046267 Computer Architecture - Winter 20/21 - HW #2 */

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

using std::cerr;
using std::cout;
using std::endl;
using std::FILE;
using std::ifstream;
using std::string;
using std::stringstream;

class Block // The block's metaData without the actual Data
{

public:
	bool valid;
	bool dirty;
	unsigned tag;

	Block() : valid(false), dirty(false), tag(0)
	{
	}

	~Block()
	{
	}
};

class Way // a single table/Way for storing the blocks
{
public:
	unsigned waySize;
	Block **wayTable;

	Way(unsigned waysize) : waySize(waysize)
	{
		wayTable = new Block *[waysize];
		for (int i = 0; i < waysize; i++)
		{
			wayTable[i] = new Block();
		}
	}

	void addBlock(unsigned index, unsigned tag)
	{
		wayTable[index]->valid = true;
		wayTable[index]->dirty = false;
		wayTable[index]->tag = tag;
	}

	~Way()
	{
		for (int i = 0; i < waySize; i++)
		{
			delete wayTable[i];
		}
		delete[] wayTable;
	}
};

class LRU // a table the indicates which Way was used last in accordance to the actual blocks in every Way
{
public:
	unsigned tableSize;
	unsigned numWays;
	unsigned **table;

	LRU(unsigned tableSize, unsigned numWays) : tableSize(tableSize), numWays(numWays)
	{
		table = new unsigned *[tableSize];
		for (int i = 0; i < tableSize; i++)
		{
			table[i] = new unsigned[numWays];
			for (int j = 0; j < numWays; j++)
				table[i][j] = j;
		}
	}

	void updateTable(unsigned index, unsigned way) // similar to the algorithim in the lecture
	{
		int x = table[index][way];
		table[index][way] = numWays - 1;
		for (int j = 0; j <= numWays - 1; j++)
			if ((j != way) && (table[index][j] >= x))
				table[index][j]--;
	}

	unsigned pickLRU(unsigned index) // choose Least recently used
	{
		for (int i = 0; i < numWays; i++)
		{
			if (table[index][i] == 0)
				return i;
		}
		return 0;
	}

	~LRU()
	{
		for (int i = 0; i < tableSize; ++i)
			delete[] table[i];
		delete[] table;
	}
};

class Cache // contains tables and their LRU tables in addition to its properties
{
public:
	unsigned Size;
	unsigned Assoc;
	unsigned Bsize;
	unsigned numWays;
	unsigned singleTableSize;

	unsigned accessCount;
	unsigned missCount;
	Way **NWays;
	LRU *lru;

	Cache(unsigned Size, unsigned Assoc, unsigned Bsize) : Size(Size), Assoc(Assoc), Bsize(Bsize)
	{
		accessCount = 0;
		missCount = 0;
		singleTableSize = (unsigned)std::pow(2, Size - Bsize - Assoc); // size of each Way
		numWays = (unsigned)std::pow(2, Assoc);

		NWays = new Way *[numWays];
		for (int i = 0; i < numWays; i++)
		{
			NWays[i] = new Way(singleTableSize);
		}
		lru = new LRU(singleTableSize, numWays);
	}

	unsigned search(unsigned index, unsigned tag) // returns Way if found else returns -1
	{
		for (int i = 0; i < numWays; i++)
		{
			if (NWays[i]->wayTable[index]->valid && NWays[i]->wayTable[index]->tag == tag)
			{
				return i;
			}
		}
		return -1;
	}

	~Cache()
	{
		for (int i = 0; i < numWays; i++)
		{
			delete NWays[i];
		}
		delete[] NWays;
		delete lru;
	}
};

unsigned
read(unsigned address, unsigned L1cyc, unsigned L2cyc, unsigned memcyc, Cache *L1, Cache *L2)
{
	unsigned readTime = 0;
	int L1way;
	int L2way;
	unsigned L1set = (address >> (L1->Bsize)) % (L1->singleTableSize); // Bsize is the same number of bits used for offset
	unsigned L1tag = address >> (L1->Size - L1->Assoc);				   // original: ( Bsize + Size - Bsize - Assoc)
	unsigned L2set = (address >> (L2->Bsize)) % (L2->singleTableSize);
	unsigned L2tag = address >> (L2->Size - L2->Assoc); // original: ( Bsize + Size - Bsize - Assoc)

	L1way = L1->search(L1set, L1tag);
	L1->accessCount++;
	readTime += L1cyc;

	if (L1way == -1)
	{ // L1 read miss
		L1->missCount++;

		L2way = L2->search(L2set, L2tag);
		L2->accessCount++;
		readTime += L2cyc;
		if (L2way == -1)
		{ // L2 read miss
			L2->missCount++;

			// bring to L2 from memory
			readTime += memcyc;
			unsigned wantedWay = L2->lru->pickLRU(L2set); // find place for it in L2
			//////////////////////////////////Remove from L1 if needed
			if (L2->NWays[wantedWay]->wayTable[L2set]->valid == true)
			{
				unsigned originalCutAdress = ((L2->NWays[wantedWay]->wayTable[L2set]->tag << (L2->Size - L2->Assoc - L2->Bsize)) + L2set); // without offset
				unsigned originalSet = originalCutAdress % (L1->singleTableSize);
				unsigned originalTag = originalCutAdress >> (L1->Size - L1->Assoc - L1->Bsize);
				// SEARCH for block in L1 for removal to save inclusion princible
				unsigned possibleL1way = L1->search(originalSet, originalTag);
				if (possibleL1way != -1)
				{
					L1->NWays[possibleL1way]->wayTable[originalSet]->valid = false;
					L1->NWays[possibleL1way]->wayTable[originalSet]->dirty = false;
				}
			}
			L2->NWays[wantedWay]->addBlock(L2set, L2tag);
		}
		L2way = L2->search(L2set, L2tag);
		L2->lru->updateTable(L2set, L2way);

		// bring to L1
		unsigned wantedWay = L1->lru->pickLRU(L1set);
		if (L1->NWays[wantedWay]->wayTable[L1set]->valid && L1->NWays[wantedWay]->wayTable[L1set]->dirty)							 // in case of valid and dirty we need to update it in L2
		{																															 // original = address of a dirty block coming from L1 to L2
			unsigned originalCutAdress = (L1->NWays[wantedWay]->wayTable[L1set]->tag << (L1->Size - L1->Assoc - L1->Bsize)) + L1set; // without offset
			unsigned originalSet = originalCutAdress % (L2->singleTableSize);
			unsigned originalTag = originalCutAdress >> ((L2->Size - L2->Assoc - L2->Bsize));
			L2way = L2->search(originalSet, originalTag);
			// Sanity check
			if (L2->NWays[L2way]->wayTable[originalSet]->tag != originalTag)
			{
				printf("congrats!,You just outdone yourself\n"); // inclusion princible isnt working XD
			}
			// copy from L1 to L2
			L2->NWays[L2way]->wayTable[originalSet]->dirty = true; // Because of WriteBack
			L2->lru->updateTable(originalSet, L2way);
		}
		L1->NWays[wantedWay]->addBlock(L1set, L1tag);
	}
	L1way = L1->search(L1set, L1tag);
	L1->lru->updateTable(L1set, L1way);
	return readTime;
}

unsigned
write(unsigned address, unsigned L1cyc, unsigned L2cyc, unsigned memcyc, bool writeAllocate, Cache *L1, Cache *L2)
{
	unsigned writeTime = 0;
	int L1way;
	int L2way;
	unsigned L1set = (address >> (L1->Bsize)) % (L1->singleTableSize);
	unsigned L1tag = address >> (L1->Size - L1->Assoc); // original: (Bsize + Size - Bsize - Assoc)
	unsigned L2set = (address >> (L2->Bsize)) % (L2->singleTableSize);
	unsigned L2tag = address >> (L2->Size - L2->Assoc); // original: (Bsize + Size - Bsize - Assoc)

	L1way = L1->search(L1set, L1tag);
	if (L1way == -1)
	{					   // Write miss in L1
		if (writeAllocate) // WRITE ALLOCATE
		{
			writeTime = read(address, L1cyc, L2cyc, memcyc, L1, L2); // same as read, the difference is changing the dirty bit
			L1way = L1->search(L1set, L1tag);
			L1->NWays[L1way]->wayTable[L1set]->dirty = true;
		}
		else
		{ // no write allocate
			writeTime = L1cyc + L2cyc;
			L1->accessCount++;
			L1->missCount++;
			L2->accessCount++;
			L2way = L2->search(L2set, L2tag);
			if (L2way == -1)
			{ // Write miss in L2
				L2->missCount++;
				writeTime += memcyc;
			}
			else
			{ // Write hit in L2
				L2->NWays[L2way]->wayTable[L2set]->dirty = true;
				L2->lru->updateTable(L2set, L2way);
			}
		}
	}
	else
	{ // WriteBack: no need to change other cache
	  // Write hit in L1
		L1->accessCount++;
		L1->NWays[L1way]->wayTable[L1set]->dirty = true;
		L1->lru->updateTable(L1set, L1way);
		writeTime += L1cyc;
	}

	return writeTime;
}

int main(int argc, char **argv)
{

	if (argc < 19)
	{
		cerr << "Not enough arguments" << endl;
		return 0;
	}

	// Get input arguments

	// File
	// Assuming it is the first argument
	char *fileString = argv[1];
	ifstream file(fileString); // input file stream
	string line;
	if (!file || !file.good())
	{
		// File doesn't exist or some other error
		cerr << "File not found" << endl;
		return 0;
	}

	unsigned MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
			 L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;

	for (int i = 2; i < 19; i += 2)
	{
		string s(argv[i]);
		if (s == "--mem-cyc")
		{
			MemCyc = atoi(argv[i + 1]);
		}
		else if (s == "--bsize")
		{
			BSize = atoi(argv[i + 1]);
		}
		else if (s == "--l1-size")
		{
			L1Size = atoi(argv[i + 1]);
		}
		else if (s == "--l2-size")
		{
			L2Size = atoi(argv[i + 1]);
		}
		else if (s == "--l1-cyc")
		{
			L1Cyc = atoi(argv[i + 1]);
		}
		else if (s == "--l2-cyc")
		{
			L2Cyc = atoi(argv[i + 1]);
		}
		else if (s == "--l1-assoc")
		{
			L1Assoc = atoi(argv[i + 1]);
		}
		else if (s == "--l2-assoc")
		{
			L2Assoc = atoi(argv[i + 1]);
		}
		else if (s == "--wr-alloc")
		{
			WrAlloc = atoi(argv[i + 1]);
		}
		else
		{
			cerr << "Error in arguments" << endl;
			return 0;
		}
	}

	Cache *L1 = new Cache(L1Size, L1Assoc, BSize);
	Cache *L2 = new Cache(L2Size, L2Assoc, BSize);
	unsigned lineCount = 0;
	unsigned Time = 0;

	while (getline(file, line))
	{
		stringstream ss(line);
		string address;
		char operation = 0; // read (R) or write (W)
		if (!(ss >> operation >> address))
		{
			// Operation appears in an Invalid format
			cout << "Command Format error" << endl;
			return 0;
		}

		// DEBUG - remove this line
		// cout << "operation: " << operation;

		string cutAddress = address.substr(2); // Removing the "0x" part of the address

		// DEBUG - remove this line
		// cout << ", address (hex)" << cutAddress;

		unsigned long int num = 0;
		num = strtoul(cutAddress.c_str(), NULL, 16);

		// DEBUG - remove this line
		// cout << " (dec) " << num << endl;

		////////////////////////////////////////////////////////////////
		lineCount += 1;
		if (operation == 'r')
		{
			Time += read(num, L1Cyc, L2Cyc, MemCyc, L1, L2);
		}
		else
		{
			if (operation == 'w')
				Time += write(num, L1Cyc, L2Cyc, MemCyc, WrAlloc, L1, L2);
		}
	}

	double L1MissRate = (double)((double)L1->missCount) / ((double)L1->accessCount);
	double L2MissRate = (double)((double)L2->missCount) / ((double)L2->accessCount);
	double avgAccTime = (double)((double)Time) / ((double)lineCount);

	printf("L1miss=%.03f ", L1MissRate);
	printf("L2miss=%.03f ", L2MissRate);
	printf("AccTimeAvg=%.03f\n", avgAccTime);

	delete L1;
	delete L2;

	return 0;
}
