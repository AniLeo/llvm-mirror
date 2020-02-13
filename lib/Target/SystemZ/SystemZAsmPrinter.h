//===-- SystemZAsmPrinter.h - SystemZ LLVM assembly printer ----*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYSTEMZ_SYSTEMZASMPRINTER_H
#define LLVM_LIB_TARGET_SYSTEMZ_SYSTEMZASMPRINTER_H

#include "SystemZTargetMachine.h"
#include "SystemZMCInstLower.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/Support/Compiler.h"

namespace llvm {
class MCStreamer;
class MachineBasicBlock;
class MachineInstr;
class Module;
class raw_ostream;

class LLVM_LIBRARY_VISIBILITY SystemZAsmPrinter : public AsmPrinter {
private:
  StackMaps SM;

public:
  SystemZAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)), SM(*this) {}

  // Override AsmPrinter.
  StringRef getPassName() const override { return "SystemZ Assembly Printer"; }
  void EmitInstruction(const MachineInstr *MI) override;
  void EmitMachineConstantPoolValue(MachineConstantPoolValue *MCPV) override;
  void emitEndOfAsmFile(Module &M) override;
  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override;
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &OS) override;

  bool doInitialization(Module &M) override {
    SM.reset();
    return AsmPrinter::doInitialization(M);
  }

private:
  void LowerFENTRY_CALL(const MachineInstr &MI, SystemZMCInstLower &MCIL);
  void LowerSTACKMAP(const MachineInstr &MI);
  void LowerPATCHPOINT(const MachineInstr &MI, SystemZMCInstLower &Lower);
};
} // end namespace llvm

#endif
