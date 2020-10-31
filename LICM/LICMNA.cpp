#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils.h"

using namespace llvm;

namespace {
class LICMNA : public LoopPass {
public:
  static char ID;

  LICMNA();
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnLoop(Loop *L, LPPassManager &LPM) override;

private:
  // Hoist all instructions to be hoisted and return true if the
  // number of instructions is greater than 0
  bool hoistInstructions(Loop *L,
                         SmallVectorImpl<Instruction *> &Instructions) const;

  // Check if a BasicBlock is contained in a subloop of the
  // given loop
  bool isInSubLoop(Loop *L, LoopInfo *LI, BasicBlock *BB) const;

  // Check if an instruction is a loop invariant in a given loop by
  // examining the instruction type and operands
  bool isLoopInvariant(Loop *L, const Instruction &I) const;

  // Check if an instruction's is of a type being hoisted
  bool checkInstructionType(const Instruction &I) const;

  // Check if an instruction's operands are loop invariants
  bool checkInstructionOperands(Loop *L, const Instruction &I) const;

  // Check if an instruction is safe to hoist by checking for side
  // effects or domination of exit blocks
  bool safeToHoist(Loop *L, DominatorTree *DT, const Instruction &I) const;

  // Check if an instruction dominates all the exit blocks of a loop
  bool dominatesExits(Loop *L, DominatorTree *DT, const Instruction &I) const;

  // Print all hoisted instructions
  void print(const SmallVectorImpl<Instruction *> &Instructions) const;
}; // end of class LICMNA

} // end of anonymous namespace

char LICMNA::ID = 0;

static RegisterPass<LICMNA> X("LICMNA", "LICMNA Pass",
                               false /* Only looks at CFG */,
                               false /* Analysis Pass */);

LICMNA::LICMNA() : LoopPass(ID) {}

void LICMNA::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addRequiredID(LoopSimplifyID);
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<DominatorTreeWrapperPass>();
}

bool LICMNA::runOnLoop(Loop *L, LPPassManager &LPM) {
  SmallVector<Instruction *, 16> Instructions;
  bool Modified = hoistInstructions(L, Instructions);
  print(Instructions);

  return Modified;
}

bool LICMNA::hoistInstructions(
    Loop *L, SmallVectorImpl<Instruction *> &Instructions) const {
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  Instruction *Destination = L->getLoopPreheader()->getTerminator();

  bool Modified = false;
  for (auto node = GraphTraits<DominatorTree *>::nodes_begin(DT);
       node != GraphTraits<DominatorTree *>::nodes_end(DT); ++node) {
    BasicBlock *BB = node->getBlock();
    if (!L->contains(BB) || isInSubLoop(L, LI, BB)) {
      continue;
    }

    for (auto it = BB->begin(); it != BB->end();) {
      // Increment after getting the instruction since the instruction
      // could potentially be moved to another BasicBlock
      Instruction &I = *it;
      ++it;

      if (isLoopInvariant(L, I) && safeToHoist(L, DT, I)) {
        Modified = true;
        I.moveBefore(Destination);
        Instructions.push_back(&I);
      }
    }
  }

  return Modified;
}

bool LICMNA::isInSubLoop(Loop *L, LoopInfo *LI, BasicBlock *BB) const {
  Loop *ParentLoop = LI->getLoopFor(BB);

  return L != ParentLoop;
}

bool LICMNA::isLoopInvariant(Loop *L, const Instruction &I) const {
  return checkInstructionType(I) && checkInstructionOperands(L, I);
}

bool LICMNA::checkInstructionType(const Instruction &I) const {
  return I.isBinaryOp() || I.isShift() || isa<SelectInst>(I) || I.isCast() ||
         isa<GetElementPtrInst>(I);
}

bool LICMNA::checkInstructionOperands(Loop *L, const Instruction &I) const {
  for (const Use &U : I.operands()) {
    Value *V = U.get();

    if (!isa<Constant>(V) && !L->isLoopInvariant(V)) {
      return false;
    }
  }
  return true;
}

bool LICMNA::safeToHoist(Loop *L, DominatorTree *DT,
                          const Instruction &I) const {
  return isSafeToSpeculativelyExecute(&I) || dominatesExits(L, DT, I);
}

bool LICMNA::dominatesExits(Loop *L, DominatorTree *DT,
                             const Instruction &I) const {
  SmallVector<BasicBlock *, 16> Exits;
  L->getUniqueExitBlocks(Exits);
  const BasicBlock *Parent = I.getParent();

  bool Dominates = false;
  for (BasicBlock *BB : Exits) {
    Dominates &= DT->dominates(Parent, BB);
  }

  return Dominates;
}

void LICMNA::print(const SmallVectorImpl<Instruction *> &Instructions) const {
  for (Instruction *I : Instructions) {
    I->print(errs());
    errs() << '\n';
  }
}