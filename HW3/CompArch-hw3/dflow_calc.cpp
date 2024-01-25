/* 046267 Computer Architecture - HW #3 */
/* Implementation (skeleton)  for the dataflow statistics calculator */

#include "dflow_calc.h"
#include <stdio.h>
#define REGISTERS 32

class dependency
{
public:
    int registerNum;
    int instructionNum; // the instruction that last used this register

    dependency(unsigned int reg, unsigned int inst) : registerNum(reg), instructionNum(inst) {}
};

class instruction
{
public:
    int opNum;
    int opLatency;
    int dst;             // Register
    dependency *depsrc1; // Register + instruction
    dependency *depsrc2; // Register + instruction
    int totalLatency;    // including this instructions operation latency

    instruction(int op, unsigned int oplatency, int dst, int src1, int src2) : opNum(op), opLatency(oplatency), dst(dst)
    {
        depsrc1 = new dependency(src1, -1);
        depsrc2 = new dependency(src2, -1);
        totalLatency = opLatency;
    }
    ~instruction()
    {
        delete depsrc1;
        delete depsrc2;
    }
};

class DataFlowCalc
{
public:
    instruction **instructions;
    int instructionsNum;
    int registerUsage[REGISTERS]; // each index represents a register and inside has the number of instruction which last used this register
    int programLatency;

    DataFlowCalc(const unsigned int opsLatency[], const InstInfo progTrace[], unsigned int numOfInsts)
    {
        // building the instruction set according to the program
        instructions = new instruction *[numOfInsts];
        for (unsigned i = 0; i < numOfInsts; i++)
        {
            instructions[i] = new instruction(progTrace[i].opcode, opsLatency[progTrace[i].opcode], progTrace[i].dstIdx, progTrace[i].src1Idx, progTrace[i].src2Idx);
        }
        // initially the registers haven't been used with dependencies before the program
        for (int j = 0; j < REGISTERS; j++)
            registerUsage[j] = -1;

        instructionsNum = numOfInsts;
        programLatency = 0;
        // go over the instructions in order to find RAW dependencies
        // each write to a register indicates possibilty for a dependecy so we save the last instruction
        // which used this register therefor the next instruction to read said register knows who it depends on.
        for (unsigned int k = 0; k < numOfInsts; k++)
        {
            instructions[k]->depsrc1->instructionNum = registerUsage[instructions[k]->depsrc1->registerNum]; // instdep1
            instructions[k]->depsrc2->instructionNum = registerUsage[instructions[k]->depsrc2->registerNum]; // ibstdep2

            int src1Latency = 0;
            int src2Latency = 0;
            // checking the latency of the two Depnendencies and choosing the max
            if (instructions[k]->depsrc1->instructionNum >= 0 && instructions[k]->depsrc1->instructionNum < (int)numOfInsts)
                src1Latency = instructions[instructions[k]->depsrc1->instructionNum]->totalLatency;
            if (instructions[k]->depsrc2->instructionNum >= 0 && instructions[k]->depsrc2->instructionNum < (int)numOfInsts)
                src2Latency = instructions[instructions[k]->depsrc2->instructionNum]->totalLatency;

            int x = (src1Latency > src2Latency) ? src1Latency : src2Latency;
            instructions[k]->totalLatency += x;

            // writing that our instruction is the last that used register dst
            registerUsage[instructions[k]->dst] = k;
        }
    }

    // the program's latency is the max instruction latency of the instructions in the program
    void calcProgLatency()
    {
        programLatency = instructions[0]->totalLatency;
        for (int i = 1; i < instructionsNum; i++)
        {
            if (instructions[i]->totalLatency >= programLatency)
                programLatency = instructions[i]->totalLatency;
        }
    }

    ~DataFlowCalc()
    {
        for (int i = 0; i < instructionsNum; i++)
        {
            delete instructions[i];
        }
        delete[] instructions;
    }
};

ProgCtx analyzeProg(const unsigned int opsLatency[], const InstInfo progTrace[], unsigned int numOfInsts)
{
    DataFlowCalc *df = new DataFlowCalc(opsLatency, progTrace, numOfInsts);
    if (df == nullptr)
        return PROG_CTX_NULL;
    return df;
}

void freeProgCtx(ProgCtx ctx)
{
    DataFlowCalc *df = (DataFlowCalc *)(ctx);
    delete df;
}

int getInstDepth(ProgCtx ctx, unsigned int theInst)
{
    DataFlowCalc *df = (DataFlowCalc *)(ctx);
    if ((int)theInst > df->instructionsNum || (int)theInst < 0)
        return -1;
    return df->instructions[theInst]->totalLatency - df->instructions[theInst]->opLatency;
}

int getInstDeps(ProgCtx ctx, unsigned int theInst, int *src1DepInst, int *src2DepInst)
{
    DataFlowCalc *df = (DataFlowCalc *)(ctx);
    if ((int)theInst > df->instructionsNum || (int)theInst < 0)
        return -1;
    *src1DepInst = df->instructions[theInst]->depsrc1->instructionNum;
    *src2DepInst = df->instructions[theInst]->depsrc2->instructionNum;
    return 0;
}

int getProgDepth(ProgCtx ctx)
{
    DataFlowCalc *df = (DataFlowCalc *)(ctx);
    df->calcProgLatency();
    return df->programLatency;
}
