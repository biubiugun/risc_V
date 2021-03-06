#include "Process.h"

InstructionFetcher::InstructionFetcher()
:pc(0),inst(0),working(false){}

void InstructionFetcher::FetchInstruction(u_int8_t *memory, u_int32_t &_pc,bool &stop,bool ID_stall,bool jump,u_int32_t jump_to,bool isBubble) {
    if(ID_stall) {
        _pc -= 4;
    }
    if(!ID_stall && jump)_pc = jump_to;
    if(isBubble && ID_stall)_pc += 4;
    pc = _pc;
    memcpy(&inst,memory + pc,4);
    if(inst == 0x0ff00513) { stop = true;return; }
    _pc += 4;
    working = true;
}

InstructionDecoder::InstructionDecoder()
:working(false),immediate(0),rs1(),rs2(),rd(),stall(false){}

void InstructionDecoder::DecodeInstruction(u_int32_t _inst, u_int32_t &_pc,bool IF_working,int modify[],int rg[],bool &isBubble,u_int32_t IF_pc,BranchPredictor &predictor) {
    if(!IF_working)return;
    jump = false;
    if(isBubble){
        stall = true;
        IsBubble = true;
        isBubble = false;
        return;
    }
    OPType type = getType(_inst);
    op = getName(type,_inst);
    if(type == U){
        immediate = int32_t (_inst >> 12) << 12;
        rd = (_inst >> 7) & 0b11111u;
        modify[rd]++;
    }else if(type == J){
        immediate = int(_inst >> 31 << 20 | _inst << 1 >> 22 << 1 | _inst << 11 >> 31 << 11 | _inst << 12 >> 24 << 12) << 11 >> 11;
        rd = (_inst >> 7) & 0b11111u;
        modify[rd]++;
        jump = true;
        jump_to = IF_pc + immediate;
    }else if(type == I){
        rd = (_inst >> 7) & 0b11111u;
        rs1 = (_inst >> 15) & 0b11111u;
        immediate = int32_t (_inst >> 20) << 20 >> 20;
        if(modify[rs1] > 0){stall = true;}
        else {
            modify[rd]++;
        }
        if(op == JALR){
            if(!stall){
                jump = true,jump_to = (rg[rs1] + immediate) & ~1;
            }
        }
    }else if(type == B){
        rd = (_inst >> 7) & 0b11111u;
        rs1 = (_inst >> 15) & 0b11111u;
        rs2 = (_inst >> 20) & 0b11111u;
        immediate =  int(_inst >> 31 << 12 | _inst << 1 >> 26 << 5 | _inst << 20 >> 28 << 1 | _inst << 24 >> 31 << 11) << 19 >> 19;
        if(modify[rs1] > 0 || modify[rs2] > 0)stall = true;
        //add predictor
        if(!stall){
            predictor.total++;
            if(predictor.ifBranch(IF_pc)) { jump = true, jump_to = IF_pc + immediate; }
        }
    }else if(type == S){
        rs1 = (_inst >> 15) & 0b11111u;
        rs2 = (_inst >> 20) & 0b11111u;
        immediate = int(_inst >> 25 << 5 | _inst << 20 >> 27) << 20 >> 20;
        if(modify[rs1] > 0 || modify[rs2] > 0)stall = true;
     } else if(type == R){
        rd = (_inst >> 7) & 0b11111u;
        rs1 = (_inst >> 15) & 0b11111u;
        rs2 = (_inst >> 20) & 0b11111u;
        immediate = 0;
        if(modify[rs1] > 0 || modify[rs2] > 0)stall = true;
        else modify[rd]++;
    }
    pc = IF_pc;
    working = true;
}

Executer::Executer()
:working(false),rd(),rs1(),rs2(),immediate(),op(),result(),TarAddress(),pc(),stall(false) {}

void Executer::Execute(InstructionDecoder &x,u_int32_t &_pc,int32_t rg[],BranchPredictor &predictor) {
    if(!x.working)return;
    if(x.IsBubble)x.IsBubble = false;
    if(x.stall){
        stall = true;
        x.stall = false;
        return;
    }
    nextIsBubble = false;
    immediate = x.immediate;
    rs1 = x.rs1,rs2 = x.rs2,rd = x.rd;
    op = x.op;
    pc = x.pc;
    switch (op) {
        case ADD:
            result = rg[rs1] + rg[rs2];
            break;
        case SUB:
            result = rg[rs1] - rg[rs2];
            break;
        case SLL: {
            u_int32_t mov = rg[rs2] & 0b00011111;
            result = rg[rs1] << mov;
            break;
        }
        case SLT:
            if(rg[rs1] < rg[rs2])result = 1;
            else result = 0;
            break;
        case SLTU:
            if(u_int32_t(rg[rs1]) < u_int32_t(rg[rs2]))result = 1;
            else result = 0;
            break;
        case XOR:
            result = rg[rs1] ^ rg[rs2];
            break;
        case SRL: {
            u_int32_t mov = rg[rs2] & 0b00011111;
            result = rg[rs1] >> mov;
            break;
        }
        case SRA: {
            u_int32_t mov = rg[rs2] & 0b00011111;
            result = rg[rs1] >> mov;
            break;
        }
        case OR:
            result = rg[rs1] | rg[rs2];
            break;
        case AND:
            result = rg[rs1] & rg[rs2];
            break;
        case JALR: {
            result = pc;
//            if(rd == 0)rd = 1;
            pc = (rg[rs1] + immediate) & ~1;
            break;
        }
        case LB: {
            TarAddress = rg[rs1] + immediate;
            break;
        }
        case LH:
            TarAddress = rg[rs1] + immediate;
            break;
        case LW:
            TarAddress = rg[rs1] + immediate;
            break;
        case LBU:
            TarAddress = rg[rs1] + immediate;
            break;
        case LHU:
            TarAddress = rg[rs1] + immediate;
            break;
        case ADDI:
            result = rg[rs1] + immediate;
            break;
        case SLTI:
            if(rg[rs1] < immediate)result = 1;
            else result = 0;
            break;
        case SLTIU:
            if(u_int32_t(rg[rs1]) < u_int32_t(immediate))result = 1;
            else result = 0;
            break;
        case XORI:
            result = rg[rs1] ^ immediate;
            break;
        case ORI:
            result = rg[rs1] | immediate;
            break;
        case ANDI:
            result = rg[rs1] & immediate;
            break;
        case SLLI:
            result = rg[rs1] <<  (immediate & 0x1f);
            break;
        case SRAI:
            result = rg[rs1] >>  (immediate & 0x1f);
            break;
        case SRLI:
            result = (u_int32_t)rg[rs1] >> (immediate & 0x1f);
            break;
        case SB: {
            result = rg[rs2] & 0xff;
            TarAddress = rg[rs1] + immediate;
            break;
        }
        case SH:{
            result = rg[rs2] & 0xffff;
            TarAddress = rg[rs1] + immediate;
            break;
        }
        case SW:{
            result = rg[rs2];
            TarAddress = rg[rs1] + immediate;
            break;
        }
        case BEQ:
            if(rg[rs1] == rg[rs2]){
                if(predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                    pc += immediate;
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    pc += immediate;
                    _pc = pc;
                }
            }else{
                if(!predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    _pc = pc + 4;
                }
            }
            break;
        case BNE:
            if(rg[rs1] != rg[rs2]){
                if(predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                    pc += immediate;
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    pc += immediate;
                    _pc = pc;
                }
            }else{
                if(!predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    _pc = pc + 4;
                }
            }
            break;
        case BLT:
            if(rg[rs1] < rg[rs2]){
                if(predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                    pc += immediate;
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    pc += immediate;
                    _pc = pc;
                }
            }else{
                if(!predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    _pc = pc + 4;
                }
            }
            break;
        case BGE:
            if(rg[rs1] >= rg[rs2]){
                if(predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                    pc += immediate;
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    pc += immediate;
                    _pc = pc;
                }
            }else{
                if(!predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    _pc = pc + 4;
                }
            }
            break;
        case BLTU:
            if((u_int32_t)rg[rs1] < (u_int32_t)rg[rs2]){
                if(predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                    pc += immediate;
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    pc += immediate;
                    _pc = pc;
                }
            }else{
                if(!predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    _pc = pc + 4;
                }
            }
            break;
        case BGEU:
            if((u_int32_t)rg[rs1] >= (u_int32_t)rg[rs2]){
                if(predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                    pc += immediate;
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    pc += immediate;
                    _pc = pc;
                }
            }else{
                if(!predictor.ifBranch(pc)){
                    predictor.success++;
                    predictor.update(true,pc);
                }else{
                    predictor.update(false,pc);
                    nextIsBubble = true;
                    _pc = pc + 4;
                }
            }
            break;
        case LUI:
            result = immediate;
            break;
        case AUIPC:
            result = immediate + pc;
            break;
        case JAL:
            result = pc + 4;
            pc += immediate;
            break;
        case INVALID:
            break;
    }
    working = true;
}

MemoryAccessor::MemoryAccessor():working(false),op(),address(),result(),rd() {}

void MemoryAccessor::AccessMemory(Executer &x, u_int8_t *memory) {
    if(!x.working)return;
    working = true;
    if(x.stall){
        stall = true;
        x.stall = false;
        return;
    }
    op = x.op,address = x.TarAddress,result = x.result,rd = x.rd;
    switch (op) {
        case LB: {
            int8_t tmp;
            memcpy(&tmp,memory + address,sizeof(int8_t));
            result = tmp;
            break;
        }
        case LH:{
            int16_t tmp;
            memcpy(&tmp,memory + address,sizeof(int16_t));
            result = tmp;
            break;
        }
        case LW:{
            memcpy(&result,memory + address,sizeof(u_int32_t));
            break;
        }
        case LBU:{
            u_int8_t tmp;
            memcpy(&tmp,memory + address,sizeof(u_int8_t));
            result = tmp;
            break;
        }
        case LHU:{
            u_int16_t tmp;
            memcpy(&tmp,memory + address,sizeof(u_int16_t));
            result = tmp;
            break;
        }
        case SB: {
            u_int8_t tmp = result;
            memcpy(memory + address, &tmp, sizeof(u_int8_t));
            break;
        }
        case SH: {
            u_int16_t tmp = result;
            memcpy(memory + address, &tmp, sizeof(u_int16_t));
            break;
        }
        case SW: {
            u_int32_t tmp = result;
            memcpy(memory + address, &tmp, sizeof(u_int32_t));
            break;
        }
        default: break;
    }
}

Writer::Writer():op(),result(),tarReg(),stall(false){}

void Writer::WriteBack(MemoryAccessor &x, int32_t rg[],int modify[]) {
    if(!x.working)return;
    if(x.stall){
        x.stall = false;
        return;
    }
    rg[0] = 0;
    op = x.op;
    if(op == SB || op == SW || op == SH || op == BEQ || op == BNE || op == BGE || op == BLT || op == BLTU || op == BGEU)return;
    result = x.result;
    tarReg = x.rd;
    modify[tarReg]--;
    if(tarReg == 0)return;
    rg[tarReg] = result;
}