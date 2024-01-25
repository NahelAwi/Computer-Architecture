/* 046267 Computer Architecture - Winter 20/21 - HW #3               */
/* Implementation (skeleton)  for the dataflow statistics calculator */


#include "dflow_calc.h"
#include <vector>

#include <cstdlib>



typedef struct dependency {
    unsigned int dependencyRegister;
    int dependencyNumOfInsts;
} Dependency;

/// Instruction info required for dataflow calculations
/// This structure provides the opcode and the register file index of each source operand and the destination operand
typedef struct instData{
    unsigned int opcode;
    unsigned int opcodeDelay;
    dependency dependency1;
    dependency dependency2;
    unsigned int totalDelay;
    unsigned int finalRegister;
    //int dstIdx;
    //unsigned int src1Idx;
    //unsigned int src2Idx;
} InstData;

typedef struct dataflow {
    std::vector<InstData>* InstructionVector;
    int registersLastInst[32];
    unsigned int numberOfInsts; 
} Dataflow;


//this fuction analyzes dependencies and returns the "handle" to the struct containing analysis
ProgCtx analyzeProg(const unsigned int opsLatency[], const InstInfo progTrace[], unsigned int numOfInsts) {

    Dataflow* DF = new Dataflow;
    DF->numberOfInsts = numOfInsts;
    DF->InstructionVector = new std::vector<InstData>;
    DF->InstructionVector->reserve(numOfInsts);
    DF->InstructionVector->resize(numOfInsts);
    for (int i=0 ; i<32 ; i++){
        DF->registersLastInst[i] = -1;
    };
    
    for (int i = 0; i < numOfInsts ; i++){
        unsigned int opcode = progTrace[i].opcode;
        unsigned int opcodeDelay = opsLatency[progTrace[i].opcode];
        Dependency dependency1 = {progTrace[i].src1Idx , -1};
        Dependency dependency2 = {progTrace[i].src2Idx , -1};
        unsigned int totalDelay = 0;
        unsigned int finalRegister = progTrace[i].dstIdx;
        
        InstData inst_data = {opcode, opcodeDelay, dependency1, dependency2, totalDelay, finalRegister};
        (*(DF->InstructionVector))[i] = inst_data;
    }
    
    for(int i = 0; i < numOfInsts ; i++){
        InstData inst_data = (*(DF->InstructionVector))[i];
        
        int lastUse1 = DF->registersLastInst[inst_data.dependency1.dependencyRegister];
        (*(DF->InstructionVector))[i].dependency1.dependencyNumOfInsts = lastUse1;
        int lastUse2 = DF->registersLastInst[inst_data.dependency2.dependencyRegister];
        (*(DF->InstructionVector))[i].dependency2.dependencyNumOfInsts = lastUse2;
        
        unsigned int final_register = inst_data.finalRegister;
        DF->registersLastInst[final_register] = i;
        
        unsigned int first_delay = 0 ;
        unsigned int second_delay = 0;
        
        if (lastUse1 >= 0){
           first_delay = (*(DF->InstructionVector))[lastUse1].totalDelay; 
        }
        if (lastUse2 >= 0){
           second_delay = (*(DF->InstructionVector))[lastUse2].totalDelay; 
        }
        unsigned int max_prev_delay = (first_delay > second_delay) ? first_delay : second_delay;
        (*(DF->InstructionVector))[i].totalDelay = inst_data.opcodeDelay + max_prev_delay;
    }
    
    return DF;
}

//free when done with analysis and Sheilta
void freeProgCtx(ProgCtx ctx) {
    Dataflow *DF = static_cast<Dataflow*>(ctx);
    delete(DF->InstructionVector);
    delete(DF);
    return;
}

//this function will return the length in cycles of the longest path from Entry to given Instruction
int getInstDepth(ProgCtx ctx, unsigned int theInst) {
    Dataflow *DF = static_cast<Dataflow*>(ctx);
    return (*(DF->InstructionVector))[theInst].totalDelay - (*(DF->InstructionVector))[theInst].opcodeDelay ;
}

int getInstDeps(ProgCtx ctx, unsigned int theInst, int *src1DepInst, int *src2DepInst) {
    Dataflow *DF = static_cast<Dataflow*>(ctx);
    *src1DepInst = (*(DF->InstructionVector))[theInst].dependency1.dependencyNumOfInsts;
    *src2DepInst = (*(DF->InstructionVector))[theInst].dependency2.dependencyNumOfInsts;
    return 0;
}

int getProgDepth(ProgCtx ctx) {
    Dataflow *DF = static_cast<Dataflow*>(ctx);
    unsigned int maxDelay= 0;
    for(unsigned int i = 0; i < DF->numberOfInsts ; i++){
        if ((*(DF->InstructionVector))[i].totalDelay >= maxDelay){
            maxDelay = (*(DF->InstructionVector))[i].totalDelay;
        }
     }
     return maxDelay;
}


