#include <string>

#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class LoopInfoNA : public LoopPass {
public:
  static char ID;

  LoopInfoNA();
  bool runOnLoop(Loop *L, LPPassManager &LPM) override;

private:
  // Global loop counter used as an ID
  int numLoops;

  // Get the name of the Function containing the loop
  StringRef getFunctionName(Loop *L) const;

  // Get the depth of a nested loop, 0 for a non-nested loop
  int getDepth(Loop *L) const;

  // Check if a loop contains any nested loops
  bool hasNestedLoops(Loop *L) const;

  // Get the total number of BasicBlocks in the loop, excluding
  // those in its subloops
  int getNumBlocks(Loop *L) const;

  // Get the total number of Instructions in the loop
  int getNumInstructions(Loop *L) const;

  // Get the number of atomic Instructions in the loop
  int getNumAtomics(Loop *L) const;

  // Check if the Instruction is atomic
  bool isAtomic(const Instruction &I) const;

  // Get the total number of branch instructions, excluding
  // those in its subloops
  int getNumBranches(Loop *L) const;

  // Check if a BasicBlock is contained in a subloop of the
  // given loop
  bool isInSubLoop(Loop *L, const BasicBlock *BB) const;

  // Check if an Instruction is a branch instruction,
  // specifically llvm::BranchInst, llvm::IndirectBrInst,
  // or llvm::SwitchInst
  bool isBranchInstruction(const Instruction &I) const;

  // Print the obtained loop information
  void print(StringRef function, int depth, bool subLoops, int blocks,
             int instructions, int atomics, int branches) const;

}; // end of class LoopInfoNA

} // end of anonymous namespace

char LoopInfoNA::ID = 0;

static RegisterPass<LoopInfoNA> X("LoopInfoNA", "LoopInfoNA Pass",
                               false /* Only looks at CFG */,
                               false /* Analysis Pass */);

LoopInfoNA::LoopInfoNA() : LoopPass(ID), numLoops(0) {}

bool LoopInfoNA::runOnLoop(Loop *L, LPPassManager &LPM) {
  StringRef function = getFunctionName(L);
  int depth = getDepth(L);
  bool hasNested = hasNestedLoops(L);
  int blocks = getNumBlocks(L);
  int instructions = getNumInstructions(L);
  int atomics = getNumAtomics(L);
  int branches = getNumBranches(L);

  print(function, depth, hasNested, blocks, instructions, atomics, branches);
  this->numLoops++;

  return false;
}

StringRef LoopInfoNA::getFunctionName(Loop *L) const {
  BasicBlock *header = L->getHeader();
  Function *F = header->getParent();

  return F->getName();
}

int LoopInfoNA::getDepth(Loop *L) const { return L->getLoopDepth() - 1; }

bool LoopInfoNA::hasNestedLoops(Loop *L) const {
  auto subLoops = L->getSubLoops();

  return subLoops.size() != 0;
}

int LoopInfoNA::getNumBlocks(Loop *L) const {
  int count = 0;
  for (const BasicBlock *BB : L->blocks()) {
    if (!isInSubLoop(L, BB)) {
      count++;
    }
  }

  return count;
}

int LoopInfoNA::getNumInstructions(Loop *L) const {
  int count = 0;
  for (const BasicBlock *bb : L->blocks()) {
    count += bb->size();
  }

  return count;
}

int LoopInfoNA::getNumAtomics(Loop *L) const {
  int count = 0;
  for (const BasicBlock *bb : L->blocks()) {
    for (const Instruction &instr : bb->getInstList()) {
      if (isAtomic(instr)) {
        count++;
      }
    }
  }

  return count;
}

bool LoopInfoNA::isAtomic(const Instruction &I) const {
  if (I.isAtomic()) {
    return true;
  }

  return false;
}

int LoopInfoNA::getNumBranches(Loop *L) const {
  int count = 0;
  for (const BasicBlock *bb : L->blocks()) {
    if (isInSubLoop(L, bb)) {
      continue;
    }

    for (const Instruction &I : bb->getInstList()) {
      if (isBranchInstruction(I)) {
        count++;
      }
    }
  }

  return count;
}

bool LoopInfoNA::isInSubLoop(Loop *L, const BasicBlock *BB) const {
  for (const Loop *SL : L->getSubLoops()) {
    if (SL->contains(BB)) {
      return true;
    }
  }

  return false;
}

bool LoopInfoNA::isBranchInstruction(const Instruction &I) const {
  switch (I.getOpcode()) {
  default:
    return false;
  case Instruction::Br:
    return true;
  case Instruction::IndirectBr:
    return true;
  case Instruction::Switch:
    return true;
  }
}

void LoopInfoNA::print(StringRef function, int depth, bool subLoops, int blocks,
                    int instructions, int atomics, int branches) const {
  errs() << this->numLoops << ": ";
  errs() << "func=" << function << ", ";
  errs() << "depth=" << depth << ", ";
  std::string sub = subLoops ? "true" : "false";
  errs() << "subLoops=" << sub << ", ";
  errs() << "BBs=" << blocks << ", ";
  errs() << "instrs=" << instructions << ", ";
  errs() << "atomics=" << atomics << ", ";
  errs() << "branches=" << branches << '\n';
}