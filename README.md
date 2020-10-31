# LLVM Passes

This repository contains my implementations of certain LLVM passes.


## LoopInfo
This pass finds all loops and retrieves the following information for
each loop:

- Function: name of the function containing this loop.
- Loop depth: 0 if it is not nested in any loop; otherwise, it is 1
  more than that of the loop in which it is nested in.
- Whether the loop has any loops nested within it.
- The number of top-level basic blocks in the loop but not in any of
  its nested loops.
- The number of instructions in the loop including those in its nested
  loops.
- The number of atomic operations in the loop including those in its
  nested loops.
- The number of top-level branch instructions in the loop but not in
  any of its nested loops.

## LICM
The loop invariant code motion pass, or LICM for short, attempts to
hoist loop invariants from the loop body to the loop pre-header.

Each loop invariant has to be checked for side effects (i.e.
exceptions or traps) and dominance over all exit blocks.