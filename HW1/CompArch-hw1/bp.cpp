/* 046267 Computer Architecture - Winter 20/21 - HW #1                  */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <bitset>
#include <iostream>
#include <cmath>

enum prediction
{
	SNT = 0,
	WNT = 1,
	WT = 2,
	ST = 3
};

enum ShareType
{
	none = 0,
	lsb = 1,
	mid = 2
};

typedef struct
{
	unsigned tag = 0;
	unsigned *lHistory = nullptr;
	uint32_t targetPc = 0;
	bool Valid = false;
	prediction *pt = nullptr;

} BtbEntry;

typedef struct
{
	unsigned btbsize;
	unsigned historysize;
	unsigned tagsize;
	prediction defaultp;
	bool IsGlobalHistory;
	bool IsGlobalTable;
	ShareType share;

	unsigned psize = 0;
	prediction *GlobalPt = nullptr;
	unsigned GHistory = 0;
	BtbEntry *Btable = nullptr;

	SIM_stats finalResult;
} BP;

static BP *BPW;

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared)
{ /////////*Checking the input*//////
	if (btbSize != 1 && btbSize != 2 && btbSize != 4 && btbSize != 8 && btbSize != 16 && btbSize != 32)
	{
		return -1;
	}
	if (historySize > 8 || historySize < 1)
	{
		return -1;
	}
	if ((btbSize == 1 && (tagSize < 0 || tagSize > 30 - 0)) ||
		(btbSize == 2 && (tagSize < 0 || tagSize > 30 - 1)) ||
		(btbSize == 4 && (tagSize < 0 || tagSize > 30 - 2)) ||
		(btbSize == 8 && (tagSize < 0 || tagSize > 30 - 3)) ||
		(btbSize == 16 && (tagSize < 0 || tagSize > 30 - 4)) ||
		(btbSize == 32 && (tagSize < 0 || tagSize > 30 - 5)))
	{
		return -1;
	}
	if (fsmState > 3 || fsmState < 0)
	{
		return -1;
	}
	if (isGlobalTable && (Shared > 2 || Shared < 0))
	{
		return -1;
	}
	//////////////////////////////////////////////////////
	BPW = new BP;
	BPW->btbsize = btbSize;
	BPW->tagsize = tagSize;
	BPW->historysize = historySize;
	BPW->defaultp = (prediction)fsmState;
	BPW->IsGlobalHistory = isGlobalHist;
	BPW->IsGlobalTable = isGlobalTable;
	BPW->share = (ShareType)Shared;

	BPW->finalResult.br_num = 0;
	BPW->finalResult.flush_num = 0;
	BPW->finalResult.size = 0;

	BPW->Btable = new BtbEntry[btbSize];
	for (unsigned i = 0; i < BPW->btbsize; i++)
	{
		BPW->Btable[i].Valid = false;
	}
	BPW->psize = (unsigned)std::pow(2, historySize); // size of prediction table is related to number of bits used for history

	if (isGlobalTable)
	{
		BPW->GlobalPt = new prediction[BPW->psize];
		for (unsigned i = 0; i < BPW->psize; i++)
			BPW->GlobalPt[i] = BPW->defaultp;
	}
	if (!BPW->IsGlobalTable)
	{
		BPW->share = none;
	}

	return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst)
{
	int indexBitNum = log2(BPW->btbsize);												// number of bits used for the index in branch table
	unsigned index = (pc >> 2) % (BPW->btbsize);										// actual index bits in the table (while ignoring the first two bits)
	unsigned pcTag = (pc >> (2 + indexBitNum)) % ((unsigned)std::pow(2, BPW->tagsize)); // extracting the tag of the pc with shifting and finding modulo
	if (BPW->Btable[index].Valid)
	{
		if (BPW->Btable[index].tag != pcTag)
		{
			*dst = pc + 4;
			return false;
		}

		if (BPW->share == none)
		{
			if (BPW->Btable[index].pt[(*BPW->Btable[index].lHistory)] == WT || BPW->Btable[index].pt[(*BPW->Btable[index].lHistory)] == ST)
			{ // using only history as index
				*dst = BPW->Btable[index].targetPc;
				return true;
			}
			else
			{
				*dst = pc + 4;
				return false;
			}
		}
		if (BPW->share == lsb)
		{ // bitwise xor between the history with pc shifted and adjusted bits
			if ((BPW->Btable[index].pt[(*BPW->Btable[index].lHistory) ^ ((pc >> 2) % ((unsigned)std::pow(2, BPW->historysize)))]) == WT ||
				(BPW->Btable[index].pt[(*BPW->Btable[index].lHistory) ^ ((pc >> 2) % ((unsigned)std::pow(2, BPW->historysize)))]) == ST)
			{
				*dst = BPW->Btable[index].targetPc;
				return true;
			}
			else
			{
				*dst = pc + 4;
				return false;
			}
		}
		else
		{
			if ((BPW->Btable[index].pt[(*BPW->Btable[index].lHistory) ^ ((pc >> 16) % ((unsigned)std::pow(2, BPW->historysize)))]) == WT ||
				(BPW->Btable[index].pt[(*BPW->Btable[index].lHistory) ^ ((pc >> 16) % ((unsigned)std::pow(2, BPW->historysize)))]) == ST)
			{
				*dst = BPW->Btable[index].targetPc;
				return true;
			}
			else
			{
				*dst = pc + 4;
				return false;
			}
		}
	}

	*dst = pc + 4;
	return false;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst)
{
	BPW->finalResult.br_num++;
	if ((!taken && pred_dst != (pc + 4)) || (taken && targetPc != pred_dst))
	{
		BPW->finalResult.flush_num++;
	}
	unsigned indexBitNum = log2(BPW->btbsize);
	unsigned index = (pc >> 2) % (BPW->btbsize);
	unsigned pcTag = (pc >> (2 + indexBitNum)) % ((unsigned)std::pow(2, BPW->tagsize));
	if (BPW->Btable[index].Valid) // In case of inserting into an existing entry
	{
		if (BPW->Btable[index].tag != pcTag) // Case: writing over existing entry
		{
			if (!BPW->IsGlobalHistory)
			{
				*BPW->Btable[index].lHistory = 0;
			}
			if (!BPW->IsGlobalTable)
			{
				for (unsigned i = 0; i < BPW->psize; i++)
					BPW->Btable[index].pt[i] = BPW->defaultp;
			}
			BPW->Btable[index].tag = pcTag;
		}
	}
	else
	{ // Case : New Entry
		if (BPW->IsGlobalHistory)
		{
			BPW->Btable[index].lHistory = &BPW->GHistory;
		}
		else
		{
			BPW->Btable[index].lHistory = new unsigned;
			*BPW->Btable[index].lHistory = 0;
		}
		if (BPW->IsGlobalTable)
		{
			BPW->Btable[index].pt = BPW->GlobalPt;
		}
		else
		{
			BPW->Btable[index].pt = new prediction[BPW->psize];
			for (unsigned i = 0; i < BPW->psize; i++)
				BPW->Btable[index].pt[i] = BPW->defaultp;
		}
		BPW->Btable[index].tag = pcTag;
		BPW->Btable[index].Valid = true;
	}
	BPW->Btable[index].targetPc = targetPc;

	unsigned PtIndex;
	switch (BPW->share) // finding the index in the prediction table, according to the share type
	{
	case none:
		PtIndex = *BPW->Btable[index].lHistory;
		break;
	case lsb:
		PtIndex = (*BPW->Btable[index].lHistory) ^ ((pc >> 2) % ((unsigned)std::pow(2, BPW->historysize)));
		break;
	case mid:
		PtIndex = (*BPW->Btable[index].lHistory) ^ ((pc >> 16) % ((unsigned)std::pow(2, BPW->historysize)));
		break;
	default:
		break;
	}

	// updating the relevant fsm state according to the actual result of the branch
	if (taken)
	{
		if (BPW->Btable[index].pt[PtIndex] != ST)
		{
			if (BPW->Btable[index].pt[PtIndex] == WT)
			{
				BPW->Btable[index].pt[PtIndex] = ST;
			}
			else if (BPW->Btable[index].pt[PtIndex] == WNT)
			{
				BPW->Btable[index].pt[PtIndex] = WT;
			}
			else if (BPW->Btable[index].pt[PtIndex] == SNT)
			{
				BPW->Btable[index].pt[PtIndex] = WNT;
			}
		}
		*BPW->Btable[index].lHistory = ((*BPW->Btable[index].lHistory << 1) + 1) % ((unsigned)std::pow(2, (unsigned)BPW->historysize));
	}
	else
	{
		if (BPW->Btable[index].pt[PtIndex] != SNT)
		{
			if (BPW->Btable[index].pt[PtIndex] == WNT)
			{
				BPW->Btable[index].pt[PtIndex] = SNT;
			}
			else if (BPW->Btable[index].pt[PtIndex] == WT)
			{
				BPW->Btable[index].pt[PtIndex] = WNT;
			}
			else if (BPW->Btable[index].pt[PtIndex] == ST)
			{
				BPW->Btable[index].pt[PtIndex] = WT;
			}
		}
		*BPW->Btable[index].lHistory = (*BPW->Btable[index].lHistory << 1) % ((unsigned)std::pow(2, (unsigned)BPW->historysize));
	}

	return;
}

void BP_GetStats(SIM_stats *curStats)
{
	curStats->br_num = BPW->finalResult.br_num;
	curStats->flush_num = BPW->finalResult.flush_num;
	curStats->size = 0;
	curStats->size += BPW->btbsize * (30 + BPW->tagsize + 1); // 32-target pc without first two bits,1-for valid bit
	if (BPW->IsGlobalTable)
	{
		curStats->size += 2 * (unsigned)std::pow(2, (unsigned)BPW->historysize);
	}
	else
	{
		curStats->size += BPW->btbsize * (2 * (unsigned)std::pow(2, (unsigned)BPW->historysize));
	}
	if (BPW->IsGlobalHistory)
	{
		curStats->size += BPW->historysize;
	}
	else
	{
		curStats->size += BPW->btbsize * BPW->historysize;
	}

	// freeing all the allocations
	for (unsigned i = 0; i < BPW->btbsize; i++)
	{
		if (BPW->Btable[i].Valid)
		{
			if (!BPW->IsGlobalTable)
			{
				delete[] BPW->Btable[i].pt;
			}
			if (!BPW->IsGlobalHistory)
			{
				delete BPW->Btable[i].lHistory;
			}
		}
	}
	if (BPW->IsGlobalTable)
	{
		delete[] BPW->GlobalPt;
	}
	delete[] BPW->Btable;
	delete BPW;
	return;
}
