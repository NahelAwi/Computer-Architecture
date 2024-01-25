/* 046267 Computer Architecture - HW #4 */

#include "core_api.h"
#include "sim_api.h"

#include <stdio.h>

typedef struct
{
    int tid;     // Thread id
    bool isHalt; // is thread halted

    int currInstNum; // currnet pc in thread

    int Availability; // cycle in which thread is ready to run i.e. finishing LOAD/STORE

    tcontext context; // register file

} thread;

// counters
int BCycles = 0;
int FCycles = 0;
int BinstNum = 0;
int FinstNum = 0;
// Cores
int threadsNum = 0;
thread *Bthreads;
thread *FGthreads;

// gets next thread which didnt reach halt, and add bcycle upon thread change
bool BgetNextThread(int *currT, thread *threads)
{
    int counter = 0;
    int haltedCounter = 0;
    bool allHalted = false;
    int iterator = *currT;
    threadsNum = SIM_GetThreadsNum();
    do
    {
        if (threads[iterator].Availability <= BCycles && !threads[iterator].isHalt) // this means current thread is ready to run
        {
            if (*currT != iterator) // addition of cost in case of context switch
            {
                BCycles += SIM_GetSwitchCycles();
                *currT = iterator;
            }
            return true;
        }
        if (threads[iterator].isHalt)
        {
            haltedCounter++;
        }
        counter++;
        iterator = (iterator + 1) % threadsNum;
        if (counter == threadsNum) // Idle State , none of the threads are ready to run meaning an idle core cycle
        {
            BCycles++;
            counter = 0;
            if (haltedCounter == threadsNum) // Finished state , all of the threads halted (no idle core cycle)
            {
                allHalted = true;
                BCycles--;
            }
            haltedCounter = 0;
        }

    } while (!allHalted);

    return false;
}

// similar to the above ,only without context switch cost
bool FGgetNextThread(int *currT, thread *threads)
{
    int counter = 0;
    int haltedCounter = 0;
    bool allHalted = false;
    int iterator = *currT;
    threadsNum = SIM_GetThreadsNum();
    do
    {
        if (threads[iterator].Availability <= FCycles && !threads[iterator].isHalt)
        {
            *currT = iterator;
            return true;
        }
        if (threads[iterator].isHalt)
        {
            haltedCounter++;
        }
        counter++;
        iterator = (iterator + 1) % threadsNum;
        if (counter == threadsNum)
        { //////////// Idle State
            FCycles++;
            counter = 0;
            if (haltedCounter == threadsNum)
            { /// Finished state
                allHalted = true;
                FCycles--;
            }
            haltedCounter = 0;
        }

    } while (!allHalted);

    return false;
}

void CORE_BlockedMT()
{
    threadsNum = SIM_GetThreadsNum();
    Bthreads = new thread[threadsNum];
    for (int i = 0; i < threadsNum; i++) // initialize all threads to be ready to run
    {
        for (int j = 0; j < REGS_COUNT; j++)
        {
            Bthreads[i].context.reg[j] = 0;
        }
        Bthreads[i].tid = i;
        Bthreads[i].currInstNum = 0;
        Bthreads[i].Availability = 0;
        Bthreads[i].isHalt = false;
    }

    int currThread = 0;
    Instruction *currInst = new Instruction;
    while (BgetNextThread(&currThread, Bthreads))
    {
        SIM_MemInstRead(Bthreads[currThread].currInstNum, currInst, currThread);
        BCycles++;
        BinstNum++;
        switch (currInst->opcode)
        {
        case CMD_NOP: // NOP
            break;
        case CMD_ADDI:
            Bthreads[currThread].context.reg[currInst->dst_index] = Bthreads[currThread].context.reg[currInst->src1_index] + currInst->src2_index_imm;
            break;
        case CMD_SUBI:
            Bthreads[currThread].context.reg[currInst->dst_index] = Bthreads[currThread].context.reg[currInst->src1_index] - currInst->src2_index_imm;
            break;
        case CMD_ADD:
            Bthreads[currThread].context.reg[currInst->dst_index] = Bthreads[currThread].context.reg[currInst->src1_index] + Bthreads[currThread].context.reg[currInst->src2_index_imm];
            break;
        case CMD_SUB:
            Bthreads[currThread].context.reg[currInst->dst_index] = Bthreads[currThread].context.reg[currInst->src1_index] - Bthreads[currThread].context.reg[currInst->src2_index_imm];
            break;
        case CMD_LOAD:
            if (currInst->isSrc2Imm)
            {
                SIM_MemDataRead(Bthreads[currThread].context.reg[currInst->src1_index] + currInst->src2_index_imm, (&(Bthreads[currThread].context.reg[currInst->dst_index])));
            }
            else
            {
                SIM_MemDataRead(Bthreads[currThread].context.reg[currInst->src1_index] + Bthreads[currThread].context.reg[currInst->src2_index_imm], (&(Bthreads[currThread].context.reg[currInst->dst_index])));
            }
            Bthreads[currThread].Availability = BCycles + SIM_GetLoadLat(); // in case of load/store availabilty is delayed according to the latency
            break;
        case CMD_STORE:
            if (currInst->isSrc2Imm)
            {
                SIM_MemDataWrite(Bthreads[currThread].context.reg[currInst->dst_index] + currInst->src2_index_imm, Bthreads[currThread].context.reg[currInst->src1_index]);
            }
            else
            {
                SIM_MemDataWrite(Bthreads[currThread].context.reg[currInst->dst_index] + Bthreads[currThread].context.reg[currInst->src2_index_imm], Bthreads[currThread].context.reg[currInst->src1_index]);
            }
            Bthreads[currThread].Availability = BCycles + SIM_GetStoreLat();
            break;
        case CMD_HALT:
            Bthreads[currThread].isHalt = true;
            Bthreads[currThread].currInstNum--;
            break;
        default:
            break;
        }
        Bthreads[currThread].currInstNum++;
    }
    delete currInst;
}

// similar to the core above, only here we run an instruction per thread and get the next available thread in a round robin manner
void CORE_FinegrainedMT()
{
    threadsNum = SIM_GetThreadsNum();
    FGthreads = new thread[threadsNum];
    for (int i = 0; i < threadsNum; i++)
    {
        for (int j = 0; j < REGS_COUNT; j++)
        {
            FGthreads[i].context.reg[j] = 0;
        }
        FGthreads[i].tid = i;
        FGthreads[i].currInstNum = 0;
        FGthreads[i].Availability = 0;
        FGthreads[i].isHalt = false;
    }
    int currThread = 0;
    Instruction *currInst = new Instruction;
    while (FGgetNextThread(&currThread, FGthreads))
    {
        SIM_MemInstRead(FGthreads[currThread].currInstNum, currInst, currThread);
        FCycles++;
        FinstNum++;
        switch (currInst->opcode)
        {
        case CMD_NOP: // NOP
            break;
        case CMD_ADDI:
            FGthreads[currThread].context.reg[currInst->dst_index] = FGthreads[currThread].context.reg[currInst->src1_index] + currInst->src2_index_imm;
            break;
        case CMD_SUBI:
            FGthreads[currThread].context.reg[currInst->dst_index] = FGthreads[currThread].context.reg[currInst->src1_index] - currInst->src2_index_imm;
            break;
        case CMD_ADD:
            FGthreads[currThread].context.reg[currInst->dst_index] = FGthreads[currThread].context.reg[currInst->src1_index] + FGthreads[currThread].context.reg[currInst->src2_index_imm];
            break;
        case CMD_SUB:
            FGthreads[currThread].context.reg[currInst->dst_index] = FGthreads[currThread].context.reg[currInst->src1_index] - FGthreads[currThread].context.reg[currInst->src2_index_imm];
            break;
        case CMD_LOAD:
            if (currInst->isSrc2Imm)
            {
                SIM_MemDataRead(FGthreads[currThread].context.reg[currInst->src1_index] + currInst->src2_index_imm, (&(FGthreads[currThread].context.reg[currInst->dst_index])));
            }
            else
            {
                SIM_MemDataRead(FGthreads[currThread].context.reg[currInst->src1_index] + FGthreads[currThread].context.reg[currInst->src2_index_imm], (&(FGthreads[currThread].context.reg[currInst->dst_index])));
            }
            FGthreads[currThread].Availability = FCycles + SIM_GetLoadLat();
            break;
        case CMD_STORE:
            if (currInst->isSrc2Imm)
            {
                SIM_MemDataWrite(FGthreads[currThread].context.reg[currInst->dst_index] + currInst->src2_index_imm, FGthreads[currThread].context.reg[currInst->src1_index]);
            }
            else
            {
                SIM_MemDataWrite(FGthreads[currThread].context.reg[currInst->dst_index] + FGthreads[currThread].context.reg[currInst->src2_index_imm], FGthreads[currThread].context.reg[currInst->src1_index]);
            }
            FGthreads[currThread].Availability = FCycles + SIM_GetStoreLat();
            break;
        case CMD_HALT:
            FGthreads[currThread].isHalt = true;
            FGthreads[currThread].currInstNum--;

            break;
        default:
            break;
        }
        FGthreads[currThread].currInstNum++;
        currThread = (currThread + 1) % threadsNum; ////////////////NEXT THREAD
    }
    delete currInst;
    return;
}

double CORE_BlockedMT_CPI()
{
    delete[] Bthreads;
    if (BinstNum != 0)
    {
        return (double)((double)BCycles / (double)BinstNum);
    }
    else
    {
        return 0;
    }
}

double CORE_FinegrainedMT_CPI()
{
    delete[] FGthreads;
    if (BinstNum != 0)
    {
        return (double)((double)FCycles / (double)FinstNum);
    }
    else
    {
        return 0;
    }
}

void CORE_BlockedMT_CTX(tcontext *context, int threadid)
{
    for (int i = 0; i < REGS_COUNT; i++)
    {
        context[threadid].reg[i] = Bthreads[threadid].context.reg[i];
    }
    return;
}

void CORE_FinegrainedMT_CTX(tcontext *context, int threadid)
{
    for (int i = 0; i < REGS_COUNT; i++)
    {
        context[threadid].reg[i] = FGthreads[threadid].context.reg[i];
    }
    return;
}