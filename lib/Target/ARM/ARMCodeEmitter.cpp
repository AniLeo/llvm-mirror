//===-- ARM/ARMCodeEmitter.cpp - Convert ARM code to machine code ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the pass that transforms the ARM machine instructions into
// relocatable machine code.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "jit"
#include "ARM.h"
#include "ARMAddressingModes.h"
#include "ARMConstantPoolValue.h"
#include "ARMInstrInfo.h"
#include "ARMRelocations.h"
#include "ARMSubtarget.h"
#include "ARMTargetMachine.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/PassManager.h"
#include "llvm/CodeGen/MachineCodeEmitter.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#ifndef NDEBUG
#include <iomanip>
#endif
using namespace llvm;

STATISTIC(NumEmitted, "Number of machine instructions emitted");

namespace {
  class VISIBILITY_HIDDEN ARMCodeEmitter : public MachineFunctionPass {
    ARMJITInfo                *JTI;
    const ARMInstrInfo        *II;
    const TargetData          *TD;
    TargetMachine             &TM;
    MachineCodeEmitter        &MCE;
    const std::vector<MachineConstantPoolEntry> *MCPEs;
    const std::vector<MachineJumpTableEntry> *MJTEs;
    bool IsPIC;

  public:
    static char ID;
    explicit ARMCodeEmitter(TargetMachine &tm, MachineCodeEmitter &mce)
      : MachineFunctionPass(&ID), JTI(0), II(0), TD(0), TM(tm),
      MCE(mce), MCPEs(0), MJTEs(0),
      IsPIC(TM.getRelocationModel() == Reloc::PIC_) {}
    ARMCodeEmitter(TargetMachine &tm, MachineCodeEmitter &mce,
            const ARMInstrInfo &ii, const TargetData &td)
      : MachineFunctionPass(&ID), JTI(0), II(&ii), TD(&td), TM(tm),
      MCE(mce), MCPEs(0), MJTEs(0),
      IsPIC(TM.getRelocationModel() == Reloc::PIC_) {}

    bool runOnMachineFunction(MachineFunction &MF);

    virtual const char *getPassName() const {
      return "ARM Machine Code Emitter";
    }

    void emitInstruction(const MachineInstr &MI);

  private:

    void emitWordLE(unsigned Binary);

    void emitConstPoolInstruction(const MachineInstr &MI);

    void emitMOVi2piecesInstruction(const MachineInstr &MI);

    void emitLEApcrelJTInstruction(const MachineInstr &MI);

    void addPCLabel(unsigned LabelID);

    void emitPseudoInstruction(const MachineInstr &MI);

    unsigned getMachineSoRegOpValue(const MachineInstr &MI,
                                    const TargetInstrDesc &TID,
                                    const MachineOperand &MO,
                                    unsigned OpIdx);

    unsigned getMachineSoImmOpValue(unsigned SoImm);

    unsigned getAddrModeSBit(const MachineInstr &MI,
                             const TargetInstrDesc &TID) const;

    void emitDataProcessingInstruction(const MachineInstr &MI,
                                       unsigned ImplicitRd = 0,
                                       unsigned ImplicitRn = 0);

    void emitLoadStoreInstruction(const MachineInstr &MI,
                                  unsigned ImplicitRd = 0,
                                  unsigned ImplicitRn = 0);

    void emitMiscLoadStoreInstruction(const MachineInstr &MI,
                                      unsigned ImplicitRn = 0);

    void emitLoadStoreMultipleInstruction(const MachineInstr &MI);

    void emitMulFrmInstruction(const MachineInstr &MI);

    void emitExtendInstruction(const MachineInstr &MI);

    void emitMiscArithInstruction(const MachineInstr &MI);

    void emitBranchInstruction(const MachineInstr &MI);

    void emitInlineJumpTable(unsigned JTIndex);

    void emitMiscBranchInstruction(const MachineInstr &MI);

    /// getBinaryCodeForInstr - This function, generated by the
    /// CodeEmitterGenerator using TableGen, produces the binary encoding for
    /// machine instructions.
    ///
    unsigned getBinaryCodeForInstr(const MachineInstr &MI);

    /// getMachineOpValue - Return binary encoding of operand. If the machine
    /// operand requires relocation, record the relocation and return zero.
    unsigned getMachineOpValue(const MachineInstr &MI,const MachineOperand &MO);
    unsigned getMachineOpValue(const MachineInstr &MI, unsigned OpIdx) {
      return getMachineOpValue(MI, MI.getOperand(OpIdx));
    }

    /// getShiftOp - Return the shift opcode (bit[6:5]) of the immediate value.
    ///
    unsigned getShiftOp(unsigned Imm) const ;

    /// Routines that handle operands which add machine relocations which are
    /// fixed up by the relocation stage.
    void emitGlobalAddress(GlobalValue *GV, unsigned Reloc,
                           bool NeedStub, intptr_t ACPV = 0);
    void emitExternalSymbolAddress(const char *ES, unsigned Reloc);
    void emitConstPoolAddress(unsigned CPI, unsigned Reloc);
    void emitJumpTableAddress(unsigned JTIndex, unsigned Reloc);
    void emitMachineBasicBlock(MachineBasicBlock *BB, unsigned Reloc,
                               intptr_t JTBase = 0);
  };
  char ARMCodeEmitter::ID = 0;
}

/// createARMCodeEmitterPass - Return a pass that emits the collected ARM code
/// to the specified MCE object.
FunctionPass *llvm::createARMCodeEmitterPass(ARMTargetMachine &TM,
                                             MachineCodeEmitter &MCE) {
  return new ARMCodeEmitter(TM, MCE);
}

bool ARMCodeEmitter::runOnMachineFunction(MachineFunction &MF) {
  assert((MF.getTarget().getRelocationModel() != Reloc::Default ||
          MF.getTarget().getRelocationModel() != Reloc::Static) &&
         "JIT relocation model must be set to static or default!");
  II = ((ARMTargetMachine&)MF.getTarget()).getInstrInfo();
  TD = ((ARMTargetMachine&)MF.getTarget()).getTargetData();
  JTI = ((ARMTargetMachine&)MF.getTarget()).getJITInfo();
  MCPEs = &MF.getConstantPool()->getConstants();
  MJTEs = &MF.getJumpTableInfo()->getJumpTables();
  IsPIC = TM.getRelocationModel() == Reloc::PIC_;
  JTI->Initialize(MF, IsPIC);

  do {
    DOUT << "JITTing function '" << MF.getFunction()->getName() << "'\n";
    MCE.startFunction(MF);
    for (MachineFunction::iterator MBB = MF.begin(), E = MF.end(); 
         MBB != E; ++MBB) {
      MCE.StartMachineBasicBlock(MBB);
      for (MachineBasicBlock::const_iterator I = MBB->begin(), E = MBB->end();
           I != E; ++I)
        emitInstruction(*I);
    }
  } while (MCE.finishFunction(MF));

  return false;
}

/// getShiftOp - Return the shift opcode (bit[6:5]) of the immediate value.
///
unsigned ARMCodeEmitter::getShiftOp(unsigned Imm) const {
  switch (ARM_AM::getAM2ShiftOpc(Imm)) {
  default: assert(0 && "Unknown shift opc!");
  case ARM_AM::asr: return 2;
  case ARM_AM::lsl: return 0;
  case ARM_AM::lsr: return 1;
  case ARM_AM::ror:
  case ARM_AM::rrx: return 3;
  }
  return 0;
}

/// getMachineOpValue - Return binary encoding of operand. If the machine
/// operand requires relocation, record the relocation and return zero.
unsigned ARMCodeEmitter::getMachineOpValue(const MachineInstr &MI,
                                           const MachineOperand &MO) {
  if (MO.isReg())
    return ARMRegisterInfo::getRegisterNumbering(MO.getReg());
  else if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  else if (MO.isGlobal())
    emitGlobalAddress(MO.getGlobal(), ARM::reloc_arm_branch, true);
  else if (MO.isSymbol())
    emitExternalSymbolAddress(MO.getSymbolName(), ARM::reloc_arm_branch);
  else if (MO.isCPI())
    emitConstPoolAddress(MO.getIndex(), ARM::reloc_arm_cp_entry);
  else if (MO.isJTI())
    emitJumpTableAddress(MO.getIndex(), ARM::reloc_arm_relative);
  else if (MO.isMBB())
    emitMachineBasicBlock(MO.getMBB(), ARM::reloc_arm_branch);
  else {
    cerr << "ERROR: Unknown type of MachineOperand: " << MO << "\n";
    abort();
  }
  return 0;
}

/// emitGlobalAddress - Emit the specified address to the code stream.
///
void ARMCodeEmitter::emitGlobalAddress(GlobalValue *GV, unsigned Reloc,
                                       bool NeedStub, intptr_t ACPV) {
  MCE.addRelocation(MachineRelocation::getGV(MCE.getCurrentPCOffset(),
                                             Reloc, GV, ACPV, NeedStub));
}

/// emitExternalSymbolAddress - Arrange for the address of an external symbol to
/// be emitted to the current location in the function, and allow it to be PC
/// relative.
void ARMCodeEmitter::emitExternalSymbolAddress(const char *ES, unsigned Reloc) {
  MCE.addRelocation(MachineRelocation::getExtSym(MCE.getCurrentPCOffset(),
                                                 Reloc, ES));
}

/// emitConstPoolAddress - Arrange for the address of an constant pool
/// to be emitted to the current location in the function, and allow it to be PC
/// relative.
void ARMCodeEmitter::emitConstPoolAddress(unsigned CPI, unsigned Reloc) {
  // Tell JIT emitter we'll resolve the address.
  MCE.addRelocation(MachineRelocation::getConstPool(MCE.getCurrentPCOffset(),
                                                    Reloc, CPI, 0, true));
}

/// emitJumpTableAddress - Arrange for the address of a jump table to
/// be emitted to the current location in the function, and allow it to be PC
/// relative.
void ARMCodeEmitter::emitJumpTableAddress(unsigned JTIndex, unsigned Reloc) {
  MCE.addRelocation(MachineRelocation::getJumpTable(MCE.getCurrentPCOffset(),
                                                    Reloc, JTIndex, 0, true));
}

/// emitMachineBasicBlock - Emit the specified address basic block.
void ARMCodeEmitter::emitMachineBasicBlock(MachineBasicBlock *BB,
                                           unsigned Reloc, intptr_t JTBase) {
  MCE.addRelocation(MachineRelocation::getBB(MCE.getCurrentPCOffset(),
                                             Reloc, BB, JTBase));
}

void ARMCodeEmitter::emitWordLE(unsigned Binary) {
#ifndef NDEBUG
  DOUT << "  0x" << std::hex << std::setw(8) << std::setfill('0')
       << Binary << std::dec << "\n";
#endif
  MCE.emitWordLE(Binary);
}

void ARMCodeEmitter::emitInstruction(const MachineInstr &MI) {
  DOUT << "JIT: " << (void*)MCE.getCurrentPCValue() << ":\t" << MI;

  NumEmitted++;  // Keep track of the # of mi's emitted
  switch (MI.getDesc().TSFlags & ARMII::FormMask) {
  default:
    assert(0 && "Unhandled instruction encoding format!");
    break;
  case ARMII::Pseudo:
    emitPseudoInstruction(MI);
    break;
  case ARMII::DPFrm:
  case ARMII::DPSoRegFrm:
    emitDataProcessingInstruction(MI);
    break;
  case ARMII::LdFrm:
  case ARMII::StFrm:
    emitLoadStoreInstruction(MI);
    break;
  case ARMII::LdMiscFrm:
  case ARMII::StMiscFrm:
    emitMiscLoadStoreInstruction(MI);
    break;
  case ARMII::LdMulFrm:
  case ARMII::StMulFrm:
    emitLoadStoreMultipleInstruction(MI);
    break;
  case ARMII::MulFrm:
    emitMulFrmInstruction(MI);
    break;
  case ARMII::ExtFrm:
    emitExtendInstruction(MI);
    break;
  case ARMII::ArithMiscFrm:
    emitMiscArithInstruction(MI);
    break;
  case ARMII::BrFrm:
    emitBranchInstruction(MI);
    break;
  case ARMII::BrMiscFrm:
    emitMiscBranchInstruction(MI);
    break;
  }
}

void ARMCodeEmitter::emitConstPoolInstruction(const MachineInstr &MI) {
  unsigned CPI = MI.getOperand(0).getImm();       // CP instruction index.
  unsigned CPIndex = MI.getOperand(1).getIndex(); // Actual cp entry index.
  const MachineConstantPoolEntry &MCPE = (*MCPEs)[CPIndex];
  
  // Remember the CONSTPOOL_ENTRY address for later relocation.
  JTI->addConstantPoolEntryAddr(CPI, MCE.getCurrentPCValue());

  // Emit constpool island entry. In most cases, the actual values will be
  // resolved and relocated after code emission.
  if (MCPE.isMachineConstantPoolEntry()) {
    ARMConstantPoolValue *ACPV =
      static_cast<ARMConstantPoolValue*>(MCPE.Val.MachineCPVal);

    DOUT << "  ** ARM constant pool #" << CPI << " @ "
         << (void*)MCE.getCurrentPCValue() << " " << *ACPV << '\n';

    GlobalValue *GV = ACPV->getGV();
    if (GV) {
      assert(!ACPV->isStub() && "Don't know how to deal this yet!");
      if (ACPV->isNonLazyPointer())
        MCE.addRelocation(MachineRelocation::getIndirectSymbol(
                  MCE.getCurrentPCOffset(), ARM::reloc_arm_machine_cp_entry, GV,
                  (intptr_t)ACPV, false));
      else 
        emitGlobalAddress(GV, ARM::reloc_arm_machine_cp_entry,
                          ACPV->isStub(), (intptr_t)ACPV);
     } else  {
      assert(!ACPV->isNonLazyPointer() && "Don't know how to deal this yet!");
      emitExternalSymbolAddress(ACPV->getSymbol(), ARM::reloc_arm_absolute);
    }
    emitWordLE(0);
  } else {
    Constant *CV = MCPE.Val.ConstVal;

    DOUT << "  ** Constant pool #" << CPI << " @ "
         << (void*)MCE.getCurrentPCValue() << " " << *CV << '\n';

    if (GlobalValue *GV = dyn_cast<GlobalValue>(CV)) {
      emitGlobalAddress(GV, ARM::reloc_arm_absolute, false);
      emitWordLE(0);
    } else {
      assert(CV->getType()->isInteger() &&
             "Not expecting non-integer constpool entries yet!");
      const ConstantInt *CI = dyn_cast<ConstantInt>(CV);
      uint32_t Val = *(uint32_t*)CI->getValue().getRawData();
      emitWordLE(Val);
    }
  }
}

void ARMCodeEmitter::emitMOVi2piecesInstruction(const MachineInstr &MI) {
  const MachineOperand &MO0 = MI.getOperand(0);
  const MachineOperand &MO1 = MI.getOperand(1);
  assert(MO1.isImm() && "Not a valid so_imm value!");
  unsigned V1 = ARM_AM::getSOImmTwoPartFirst(MO1.getImm());
  unsigned V2 = ARM_AM::getSOImmTwoPartSecond(MO1.getImm());

  // Emit the 'mov' instruction.
  unsigned Binary = 0xd << 21;  // mov: Insts{24-21} = 0b1101

  // Set the conditional execution predicate.
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  // Encode Rd.
  Binary |= getMachineOpValue(MI, MO0) << ARMII::RegRdShift;

  // Encode so_imm.
  // Set bit I(25) to identify this is the immediate form of <shifter_op>
  Binary |= 1 << ARMII::I_BitShift;
  Binary |= getMachineSoImmOpValue(ARM_AM::getSOImmVal(V1));
  emitWordLE(Binary);

  // Now the 'orr' instruction.
  Binary = 0xc << 21;  // orr: Insts{24-21} = 0b1100

  // Set the conditional execution predicate.
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  // Encode Rd.
  Binary |= getMachineOpValue(MI, MO0) << ARMII::RegRdShift;

  // Encode Rn.
  Binary |= getMachineOpValue(MI, MO0) << ARMII::RegRnShift;

  // Encode so_imm.
  // Set bit I(25) to identify this is the immediate form of <shifter_op>
  Binary |= 1 << ARMII::I_BitShift;
  Binary |= getMachineSoImmOpValue(ARM_AM::getSOImmVal(V2));
  emitWordLE(Binary);
}

void ARMCodeEmitter::emitLEApcrelJTInstruction(const MachineInstr &MI) {
  // It's basically add r, pc, (LJTI - $+8)
  
  const TargetInstrDesc &TID = MI.getDesc();

  // Emit the 'add' instruction.
  unsigned Binary = 0x4 << 21;  // add: Insts{24-31} = 0b0100

  // Set the conditional execution predicate
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  // Encode S bit if MI modifies CPSR.
  Binary |= getAddrModeSBit(MI, TID);

  // Encode Rd.
  Binary |= getMachineOpValue(MI, 0) << ARMII::RegRdShift;

  // Encode Rn which is PC.
  Binary |= ARMRegisterInfo::getRegisterNumbering(ARM::PC) << ARMII::RegRnShift;

  // Encode the displacement.
  // Set bit I(25) to identify this is the immediate form of <shifter_op>.
  Binary |= 1 << ARMII::I_BitShift;
  emitJumpTableAddress(MI.getOperand(1).getIndex(), ARM::reloc_arm_jt_base);

  emitWordLE(Binary);
}

void ARMCodeEmitter::addPCLabel(unsigned LabelID) {
  DOUT << "  ** LPC" << LabelID << " @ "
       << (void*)MCE.getCurrentPCValue() << '\n';
  JTI->addPCLabelAddr(LabelID, MCE.getCurrentPCValue());
}

void ARMCodeEmitter::emitPseudoInstruction(const MachineInstr &MI) {
  unsigned Opcode = MI.getDesc().Opcode;
  switch (Opcode) {
  default:
    abort(); // FIXME:
  case ARM::CONSTPOOL_ENTRY:
    emitConstPoolInstruction(MI);
    break;
  case ARM::PICADD: {
    // Remember of the address of the PC label for relocation later.
    addPCLabel(MI.getOperand(2).getImm());
    // PICADD is just an add instruction that implicitly read pc.
    emitDataProcessingInstruction(MI, 0, ARM::PC);
    break;
  }
  case ARM::PICLDR:
  case ARM::PICLDRB:
  case ARM::PICSTR:
  case ARM::PICSTRB: {
    // Remember of the address of the PC label for relocation later.
    addPCLabel(MI.getOperand(2).getImm());
    // These are just load / store instructions that implicitly read pc.
    emitLoadStoreInstruction(MI, 0, ARM::PC);
    break;
  }
  case ARM::PICLDRH:
  case ARM::PICLDRSH:
  case ARM::PICLDRSB:
  case ARM::PICSTRH: {
    // Remember of the address of the PC label for relocation later.
    addPCLabel(MI.getOperand(2).getImm());
    // These are just load / store instructions that implicitly read pc.
    emitMiscLoadStoreInstruction(MI, ARM::PC);
    break;
  }
  case ARM::MOVi2pieces:
    // Two instructions to materialize a constant.
    emitMOVi2piecesInstruction(MI);
    break;
  case ARM::LEApcrelJT:
    // Materialize jumptable address.
    emitLEApcrelJTInstruction(MI);
    break;
  }
}


unsigned ARMCodeEmitter::getMachineSoRegOpValue(const MachineInstr &MI,
                                                const TargetInstrDesc &TID,
                                                const MachineOperand &MO,
                                                unsigned OpIdx) {
  unsigned Binary = getMachineOpValue(MI, MO);

  const MachineOperand &MO1 = MI.getOperand(OpIdx + 1);
  const MachineOperand &MO2 = MI.getOperand(OpIdx + 2);
  ARM_AM::ShiftOpc SOpc = ARM_AM::getSORegShOp(MO2.getImm());

  // Encode the shift opcode.
  unsigned SBits = 0;
  unsigned Rs = MO1.getReg();
  if (Rs) {
    // Set shift operand (bit[7:4]).
    // LSL - 0001
    // LSR - 0011
    // ASR - 0101
    // ROR - 0111
    // RRX - 0110 and bit[11:8] clear.
    switch (SOpc) {
    default: assert(0 && "Unknown shift opc!");
    case ARM_AM::lsl: SBits = 0x1; break;
    case ARM_AM::lsr: SBits = 0x3; break;
    case ARM_AM::asr: SBits = 0x5; break;
    case ARM_AM::ror: SBits = 0x7; break;
    case ARM_AM::rrx: SBits = 0x6; break;
    }
  } else {
    // Set shift operand (bit[6:4]).
    // LSL - 000
    // LSR - 010
    // ASR - 100
    // ROR - 110
    switch (SOpc) {
    default: assert(0 && "Unknown shift opc!");
    case ARM_AM::lsl: SBits = 0x0; break;
    case ARM_AM::lsr: SBits = 0x2; break;
    case ARM_AM::asr: SBits = 0x4; break;
    case ARM_AM::ror: SBits = 0x6; break;
    }
  }
  Binary |= SBits << 4;
  if (SOpc == ARM_AM::rrx)
    return Binary;

  // Encode the shift operation Rs or shift_imm (except rrx).
  if (Rs) {
    // Encode Rs bit[11:8].
    assert(ARM_AM::getSORegOffset(MO2.getImm()) == 0);
    return Binary |
      (ARMRegisterInfo::getRegisterNumbering(Rs) << ARMII::RegRsShift);
  }

  // Encode shift_imm bit[11:7].
  return Binary | ARM_AM::getSORegOffset(MO2.getImm()) << 7;
}

unsigned ARMCodeEmitter::getMachineSoImmOpValue(unsigned SoImm) {
  // Encode rotate_imm.
  unsigned Binary = (ARM_AM::getSOImmValRot(SoImm) >> 1)
    << ARMII::SoRotImmShift;

  // Encode immed_8.
  Binary |= ARM_AM::getSOImmValImm(SoImm);
  return Binary;
}

unsigned ARMCodeEmitter::getAddrModeSBit(const MachineInstr &MI,
                                         const TargetInstrDesc &TID) const {
  for (unsigned i = MI.getNumOperands(), e = TID.getNumOperands(); i != e; --i){
    const MachineOperand &MO = MI.getOperand(i-1);
    if (MO.isReg() && MO.isDef() && MO.getReg() == ARM::CPSR)
      return 1 << ARMII::S_BitShift;
  }
  return 0;
}

void ARMCodeEmitter::emitDataProcessingInstruction(const MachineInstr &MI,
                                                   unsigned ImplicitRd,
                                                   unsigned ImplicitRn) {
  const TargetInstrDesc &TID = MI.getDesc();

  // Part of binary is determined by TableGn.
  unsigned Binary = getBinaryCodeForInstr(MI);

  // Set the conditional execution predicate
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  // Encode S bit if MI modifies CPSR.
  Binary |= getAddrModeSBit(MI, TID);

  // Encode register def if there is one.
  unsigned NumDefs = TID.getNumDefs();
  unsigned OpIdx = 0;
  if (NumDefs)
    Binary |= getMachineOpValue(MI, OpIdx++) << ARMII::RegRdShift;
  else if (ImplicitRd)
    // Special handling for implicit use (e.g. PC).
    Binary |= (ARMRegisterInfo::getRegisterNumbering(ImplicitRd)
               << ARMII::RegRdShift);

  // If this is a two-address operand, skip it. e.g. MOVCCr operand 1.
  if (TID.getOperandConstraint(OpIdx, TOI::TIED_TO) != -1)
    ++OpIdx;

  // Encode first non-shifter register operand if there is one.
  bool isUnary = TID.TSFlags & ARMII::UnaryDP;
  if (!isUnary) {
    if (ImplicitRn)
      // Special handling for implicit use (e.g. PC).
      Binary |= (ARMRegisterInfo::getRegisterNumbering(ImplicitRn)
                 << ARMII::RegRnShift);
    else {
      Binary |= getMachineOpValue(MI, OpIdx) << ARMII::RegRnShift;
      ++OpIdx;
    }
  }

  // Encode shifter operand.
  const MachineOperand &MO = MI.getOperand(OpIdx);
  if ((TID.TSFlags & ARMII::FormMask) == ARMII::DPSoRegFrm) {
    // Encode SoReg.
    emitWordLE(Binary | getMachineSoRegOpValue(MI, TID, MO, OpIdx));
    return;
  }

  if (MO.isReg()) {
    // Encode register Rm.
    emitWordLE(Binary | ARMRegisterInfo::getRegisterNumbering(MO.getReg()));
    return;
  }

  // Encode so_imm.
  // Set bit I(25) to identify this is the immediate form of <shifter_op>.
  Binary |= 1 << ARMII::I_BitShift;
  Binary |= getMachineSoImmOpValue(MO.getImm());

  emitWordLE(Binary);
}

void ARMCodeEmitter::emitLoadStoreInstruction(const MachineInstr &MI,
                                              unsigned ImplicitRd,
                                              unsigned ImplicitRn) {
  const TargetInstrDesc &TID = MI.getDesc();

  // Part of binary is determined by TableGn.
  unsigned Binary = getBinaryCodeForInstr(MI);

  // Set the conditional execution predicate
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  // Set first operand
  unsigned OpIdx = 0;
  if (ImplicitRd)
    // Special handling for implicit use (e.g. PC).
    Binary |= (ARMRegisterInfo::getRegisterNumbering(ImplicitRd)
               << ARMII::RegRdShift);
  else
    Binary |= getMachineOpValue(MI, OpIdx++) << ARMII::RegRdShift;

  // Set second operand
  if (ImplicitRn)
    // Special handling for implicit use (e.g. PC).
    Binary |= (ARMRegisterInfo::getRegisterNumbering(ImplicitRn)
               << ARMII::RegRnShift);
  else
    Binary |= getMachineOpValue(MI, OpIdx++) << ARMII::RegRnShift;

  // If this is a two-address operand, skip it. e.g. LDR_PRE.
  if (TID.getOperandConstraint(OpIdx, TOI::TIED_TO) != -1)
    ++OpIdx;

  const MachineOperand &MO2 = MI.getOperand(OpIdx);
  unsigned AM2Opc = (ImplicitRn == ARM::PC)
    ? 0 : MI.getOperand(OpIdx+1).getImm();

  // Set bit U(23) according to sign of immed value (positive or negative).
  Binary |= ((ARM_AM::getAM2Op(AM2Opc) == ARM_AM::add ? 1 : 0) <<
             ARMII::U_BitShift);
  if (!MO2.getReg()) { // is immediate
    if (ARM_AM::getAM2Offset(AM2Opc))
      // Set the value of offset_12 field
      Binary |= ARM_AM::getAM2Offset(AM2Opc);
    emitWordLE(Binary);
    return;
  }

  // Set bit I(25), because this is not in immediate enconding.
  Binary |= 1 << ARMII::I_BitShift;
  assert(TargetRegisterInfo::isPhysicalRegister(MO2.getReg()));
  // Set bit[3:0] to the corresponding Rm register
  Binary |= ARMRegisterInfo::getRegisterNumbering(MO2.getReg());

  // if this instr is in scaled register offset/index instruction, set
  // shift_immed(bit[11:7]) and shift(bit[6:5]) fields.
  if (unsigned ShImm = ARM_AM::getAM2Offset(AM2Opc)) {
    Binary |= getShiftOp(AM2Opc) << 5;  // shift
    Binary |= ShImm              << 7;  // shift_immed
  }

  emitWordLE(Binary);
}

void ARMCodeEmitter::emitMiscLoadStoreInstruction(const MachineInstr &MI,
                                                  unsigned ImplicitRn) {
  const TargetInstrDesc &TID = MI.getDesc();

  // Part of binary is determined by TableGn.
  unsigned Binary = getBinaryCodeForInstr(MI);

  // Set the conditional execution predicate
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  // Set first operand
  Binary |= getMachineOpValue(MI, 0) << ARMII::RegRdShift;

  // Set second operand
  unsigned OpIdx = 1;
  if (ImplicitRn)
    // Special handling for implicit use (e.g. PC).
    Binary |= (ARMRegisterInfo::getRegisterNumbering(ImplicitRn)
               << ARMII::RegRnShift);
  else
    Binary |= getMachineOpValue(MI, OpIdx++) << ARMII::RegRnShift;

  // If this is a two-address operand, skip it. e.g. LDRH_POST.
  if (TID.getOperandConstraint(OpIdx, TOI::TIED_TO) != -1)
    ++OpIdx;

  const MachineOperand &MO2 = MI.getOperand(OpIdx);
  unsigned AM3Opc = (ImplicitRn == ARM::PC)
    ? 0 : MI.getOperand(OpIdx+1).getImm();

  // Set bit U(23) according to sign of immed value (positive or negative)
  Binary |= ((ARM_AM::getAM3Op(AM3Opc) == ARM_AM::add ? 1 : 0) <<
             ARMII::U_BitShift);

  // If this instr is in register offset/index encoding, set bit[3:0]
  // to the corresponding Rm register.
  if (MO2.getReg()) {
    Binary |= ARMRegisterInfo::getRegisterNumbering(MO2.getReg());
    emitWordLE(Binary);
    return;
  }

  // This instr is in immediate offset/index encoding, set bit 22 to 1.
  Binary |= 1 << ARMII::AM3_I_BitShift;
  if (unsigned ImmOffs = ARM_AM::getAM3Offset(AM3Opc)) {
    // Set operands
    Binary |= (ImmOffs >> 4) << 8;  // immedH
    Binary |= (ImmOffs & ~0xF);     // immedL
  }

  emitWordLE(Binary);
}

void ARMCodeEmitter::emitLoadStoreMultipleInstruction(const MachineInstr &MI) {
  // Part of binary is determined by TableGn.
  unsigned Binary = getBinaryCodeForInstr(MI);

  // Set the conditional execution predicate
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  // Set first operand
  Binary |= getMachineOpValue(MI, 0) << ARMII::RegRnShift;

  // Set addressing mode by modifying bits U(23) and P(24)
  // IA - Increment after  - bit U = 1 and bit P = 0
  // IB - Increment before - bit U = 1 and bit P = 1
  // DA - Decrement after  - bit U = 0 and bit P = 0
  // DB - Decrement before - bit U = 0 and bit P = 1
  const MachineOperand &MO = MI.getOperand(1);
  ARM_AM::AMSubMode Mode = ARM_AM::getAM4SubMode(MO.getImm());
  switch (Mode) {
  default: assert(0 && "Unknown addressing sub-mode!");
  case ARM_AM::da:                      break;
  case ARM_AM::db: Binary |= 0x1 << ARMII::P_BitShift; break;
  case ARM_AM::ia: Binary |= 0x1 << ARMII::U_BitShift; break;
  case ARM_AM::ib: Binary |= 0x3 << ARMII::U_BitShift; break;
  }

  // Set bit W(21)
  if (ARM_AM::getAM4WBFlag(MO.getImm()))
    Binary |= 0x1 << ARMII::W_BitShift;

  // Set registers
  for (unsigned i = 4, e = MI.getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI.getOperand(i);
    if (MO.isReg() && MO.isImplicit())
      continue;
    unsigned RegNum = ARMRegisterInfo::getRegisterNumbering(MO.getReg());
    assert(TargetRegisterInfo::isPhysicalRegister(MO.getReg()) &&
           RegNum < 16);
    Binary |= 0x1 << RegNum;
  }

  emitWordLE(Binary);
}

void ARMCodeEmitter::emitMulFrmInstruction(const MachineInstr &MI) {
  const TargetInstrDesc &TID = MI.getDesc();

  // Part of binary is determined by TableGn.
  unsigned Binary = getBinaryCodeForInstr(MI);

  // Set the conditional execution predicate
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  // Encode S bit if MI modifies CPSR.
  Binary |= getAddrModeSBit(MI, TID);

  // 32x32->64bit operations have two destination registers. The number
  // of register definitions will tell us if that's what we're dealing with.
  unsigned OpIdx = 0;
  if (TID.getNumDefs() == 2)
    Binary |= getMachineOpValue (MI, OpIdx++) << ARMII::RegRdLoShift;

  // Encode Rd
  Binary |= getMachineOpValue(MI, OpIdx++) << ARMII::RegRdHiShift;

  // Encode Rm
  Binary |= getMachineOpValue(MI, OpIdx++);

  // Encode Rs
  Binary |= getMachineOpValue(MI, OpIdx++) << ARMII::RegRsShift;

  // Many multiple instructions (e.g. MLA) have three src operands. Encode
  // it as Rn (for multiply, that's in the same offset as RdLo.
  if (TID.getNumOperands() > OpIdx &&
      !TID.OpInfo[OpIdx].isPredicate() &&
      !TID.OpInfo[OpIdx].isOptionalDef())
    Binary |= getMachineOpValue(MI, OpIdx) << ARMII::RegRdLoShift;

  emitWordLE(Binary);
}

void ARMCodeEmitter::emitExtendInstruction(const MachineInstr &MI) {
  const TargetInstrDesc &TID = MI.getDesc();

  // Part of binary is determined by TableGn.
  unsigned Binary = getBinaryCodeForInstr(MI);

  // Set the conditional execution predicate
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  unsigned OpIdx = 0;

  // Encode Rd
  Binary |= getMachineOpValue(MI, OpIdx++) << ARMII::RegRdShift;

  const MachineOperand &MO1 = MI.getOperand(OpIdx++);
  const MachineOperand &MO2 = MI.getOperand(OpIdx);
  if (MO2.isReg()) {
    // Two register operand form.
    // Encode Rn.
    Binary |= getMachineOpValue(MI, MO1) << ARMII::RegRnShift;

    // Encode Rm.
    Binary |= getMachineOpValue(MI, MO2);
    ++OpIdx;
  } else {
    Binary |= getMachineOpValue(MI, MO1);
  }

  // Encode rot imm (0, 8, 16, or 24) if it has a rotate immediate operand.
  if (MI.getOperand(OpIdx).isImm() &&
      !TID.OpInfo[OpIdx].isPredicate() &&
      !TID.OpInfo[OpIdx].isOptionalDef())
    Binary |= (getMachineOpValue(MI, OpIdx) / 8) << ARMII::ExtRotImmShift;

  emitWordLE(Binary);
}

void ARMCodeEmitter::emitMiscArithInstruction(const MachineInstr &MI) {
  const TargetInstrDesc &TID = MI.getDesc();

  // Part of binary is determined by TableGn.
  unsigned Binary = getBinaryCodeForInstr(MI);

  // Set the conditional execution predicate
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  unsigned OpIdx = 0;

  // Encode Rd
  Binary |= getMachineOpValue(MI, OpIdx++) << ARMII::RegRdShift;

  const MachineOperand &MO = MI.getOperand(OpIdx++);
  if (OpIdx == TID.getNumOperands() ||
      TID.OpInfo[OpIdx].isPredicate() ||
      TID.OpInfo[OpIdx].isOptionalDef()) {
    // Encode Rm and it's done.
    Binary |= getMachineOpValue(MI, MO);
    emitWordLE(Binary);
    return;
  }

  // Encode Rn.
  Binary |= getMachineOpValue(MI, MO) << ARMII::RegRnShift;

  // Encode Rm.
  Binary |= getMachineOpValue(MI, OpIdx++);

  // Encode shift_imm.
  unsigned ShiftAmt = MI.getOperand(OpIdx).getImm();
  assert(ShiftAmt < 32 && "shift_imm range is 0 to 31!");
  Binary |= ShiftAmt << ARMII::ShiftShift;
  
  emitWordLE(Binary);
}

void ARMCodeEmitter::emitBranchInstruction(const MachineInstr &MI) {
  const TargetInstrDesc &TID = MI.getDesc();

  if (TID.Opcode == ARM::TPsoft)
    abort(); // FIXME

  // Part of binary is determined by TableGn.
  unsigned Binary = getBinaryCodeForInstr(MI);

  // Set the conditional execution predicate
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  // Set signed_immed_24 field
  Binary |= getMachineOpValue(MI, 0);

  emitWordLE(Binary);
}

void ARMCodeEmitter::emitInlineJumpTable(unsigned JTIndex) {
  // Remember the base address of the inline jump table.
  intptr_t JTBase = MCE.getCurrentPCValue();
  JTI->addJumpTableBaseAddr(JTIndex, JTBase);
  DOUT << "  ** Jump Table #" << JTIndex << " @ " << (void*)JTBase << '\n';

  // Now emit the jump table entries.
  const std::vector<MachineBasicBlock*> &MBBs = (*MJTEs)[JTIndex].MBBs;
  for (unsigned i = 0, e = MBBs.size(); i != e; ++i) {
    if (IsPIC)
      // DestBB address - JT base.
      emitMachineBasicBlock(MBBs[i], ARM::reloc_arm_pic_jt, JTBase);
    else
      // Absolute DestBB address.
      emitMachineBasicBlock(MBBs[i], ARM::reloc_arm_absolute);
    emitWordLE(0);
  }
}

void ARMCodeEmitter::emitMiscBranchInstruction(const MachineInstr &MI) {
  const TargetInstrDesc &TID = MI.getDesc();

  // Handle jump tables.
  if (TID.Opcode == ARM::BR_JTr || TID.Opcode == ARM::BR_JTadd) {
    // First emit a ldr pc, [] instruction.
    emitDataProcessingInstruction(MI, ARM::PC);

    // Then emit the inline jump table.
    unsigned JTIndex = (TID.Opcode == ARM::BR_JTr)
      ? MI.getOperand(1).getIndex() : MI.getOperand(2).getIndex();
    emitInlineJumpTable(JTIndex);
    return;
  } else if (TID.Opcode == ARM::BR_JTm) {
    // First emit a ldr pc, [] instruction.
    emitLoadStoreInstruction(MI, ARM::PC);

    // Then emit the inline jump table.
    emitInlineJumpTable(MI.getOperand(3).getIndex());
    return;
  }

  // Part of binary is determined by TableGn.
  unsigned Binary = getBinaryCodeForInstr(MI);

  // Set the conditional execution predicate
  Binary |= II->getPredicate(&MI) << ARMII::CondShift;

  if (TID.Opcode == ARM::BX_RET)
    // The return register is LR.
    Binary |= ARMRegisterInfo::getRegisterNumbering(ARM::LR);
  else 
    // otherwise, set the return register
    Binary |= getMachineOpValue(MI, 0);

  emitWordLE(Binary);
}

#include "ARMGenCodeEmitter.inc"
