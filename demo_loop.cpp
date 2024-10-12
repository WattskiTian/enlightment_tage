#include "utils.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define logSizeLoopPred 6
int loopTableAgeBits = 3;
int loopTableConfidenceBits = 3;
int loopTableTagBits = 10;
int loopTableIterBits = 16;
int logLoopTableAssoc = 2;
int confidenceThreshold = (1 << loopTableConfidenceBits) - 1;
int loopTagMask = (1 << loopTableTagBits) - 1;
int loopNumIterMask = (1 << loopTableIterBits) - 1;
int loopSetMask = (1 << (logSizeLoopPred - logLoopTableAssoc)) - 1;
int loopUseCounter = -1;
int withLoopBits = 8;
bool useDirectionBit = true;
bool useSpeculation = false;
bool useHashing = false;
bool restrictAllocation = false;
int initialLoopIter = 0;
int initialLoopAge = 0;
bool optionalAgeReset = false;

struct LoopEntry {
  uint16_t tag = 0;
  uint16_t numIter = 0;
  uint8_t age = 0;
  uint8_t confidence = 0;
  uint16_t currentIter = 0;
  uint16_t currentIterSpec = 0;
  bool dir = false;
};

struct BranchInfo {
  int loopHit = -1;
  bool loopPredValid = false;
  int loopIndex = 0;
  int loopIndexB = 0;
  int loopTag = 0;
  bool loopPred = false;
  bool loopPredUsed = false;
  bool predTaken = false;
  uint16_t currentIter = 0;
};

LoopEntry ltable[1ULL << logSizeLoopPred];

void initLoopPredictor() {
  assert(loopTableTagBits <= 16);
  assert(loopTableIterBits <= 16);
  assert(logSizeLoopPred >= logLoopTableAssoc);
}

int lindex(uint64_t pc_in, unsigned instShiftAmt) {
  uint64_t pc = pc_in >> instShiftAmt;
  if (useHashing) {
    pc ^= pc_in;
  }
  return ((pc & loopSetMask) << logLoopTableAssoc);
}

int finallindex(int index, int lowPcBits, int way) {
  return (useHashing ? (index ^ ((lowPcBits >> way) << logLoopTableAssoc))
                     : index) +
         way;
}

bool calcConf(int index) {
  return ltable[index].confidence == confidenceThreshold;
}

bool getLoop(uint64_t pc, BranchInfo *bi, bool speculative,
             unsigned instShiftAmt) {
  bi->loopHit = -1;
  bi->loopPredValid = false;
  bi->loopIndex = lindex(pc, instShiftAmt);

  if (useHashing) {
    unsigned pcShift = logSizeLoopPred - logLoopTableAssoc;
    bi->loopIndexB = (pc >> pcShift) & loopSetMask;
    bi->loopTag = (pc >> pcShift) ^ (pc >> (pcShift + loopTableTagBits));
    bi->loopTag &= loopTagMask;
  } else {
    unsigned pcShift = instShiftAmt + logSizeLoopPred - logLoopTableAssoc;
    bi->loopTag = (pc >> pcShift) & loopTagMask;
  }

  for (int i = 0; i < (1 << logLoopTableAssoc); i++) {
    int idx = finallindex(bi->loopIndex, bi->loopIndexB, i);
    if (ltable[idx].tag == bi->loopTag) {
      bi->loopHit = i;
      bi->loopPredValid = calcConf(idx);

      uint16_t iter =
          speculative ? ltable[idx].currentIterSpec : ltable[idx].currentIter;

      if ((iter + 1) == ltable[idx].numIter) {
        return useDirectionBit ? !(ltable[idx].dir) : false;
      } else {
        return useDirectionBit ? (ltable[idx].dir) : true;
      }
    }
  }
  return false;
}

uint32_t useful_loop_cnt = 0;
uint32_t bad_loop_cnt = 0;

void loopUpdate(uint32_t pc, bool taken, BranchInfo *bi, bool tage_pred) {
  int idx = finallindex(bi->loopIndex, bi->loopIndexB, bi->loopHit);
  if (bi->loopHit >= 0) {
    // already a hit
    if (bi->loopPredValid) {
      if (taken != bi->loopPred) {
        /*printf("wops not useful at %08x\n", pc);*/
        bad_loop_cnt++;
        // free the entry
        ltable[idx].numIter = 0;
        ltable[idx].age = 0;
        ltable[idx].confidence = 0;
        ltable[idx].currentIter = 0;
        return;
      } else if (bi->loopPred != tage_pred) {
        /*printf("loop useful at %08x\n", pc);*/
        useful_loop_cnt++;
        ltable[idx].age = bit_update(ltable[idx].age, true, loopTableAgeBits);
      }
    }

    ltable[idx].currentIter = (ltable[idx].currentIter + 1) & loopNumIterMask;
    if (ltable[idx].currentIter > ltable[idx].numIter) {
      ltable[idx].confidence = 0;
      if (ltable[idx].numIter != 0) {
        // free the entry
        ltable[idx].numIter = 0;
        if (optionalAgeReset) {
          ltable[idx].age = 0;
        }
      }
    }

    if (taken != (useDirectionBit ? ltable[idx].dir : true)) {
      if (ltable[idx].currentIter == ltable[idx].numIter) {
        ltable[idx].confidence =
            bit_update(ltable[idx].confidence, true, loopTableConfidenceBits);
        // just do not predict when the loop count is 1 or 2
        if (ltable[idx].numIter < 3) {
          // free the entry
          ltable[idx].dir = taken; // ignored if no useDirectionBit
          ltable[idx].numIter = 0;
          ltable[idx].age = 0;
          ltable[idx].confidence = 0;
        }
      } else {
        if (ltable[idx].numIter == 0) {
          // first complete nest;
          ltable[idx].confidence = 0;
          ltable[idx].numIter = ltable[idx].currentIter;
        } else {
          // not the same number of iterations as last time: free the
          // entry
          ltable[idx].numIter = 0;
          if (optionalAgeReset) {
            ltable[idx].age = 0;
          }
          ltable[idx].confidence = 0;
        }
      }
      ltable[idx].currentIter = 0;
    }

  } else if (useDirectionBit ? (bi->predTaken != taken) : taken) {
    if ((random() & 3) == 0 || !restrictAllocation) {
      // try to allocate an entry on taken branch
      int nrand = random();
      for (int i = 0; i < (1 << logLoopTableAssoc); i++) {
        int loop_hit = (nrand + i) & ((1 << logLoopTableAssoc) - 1);
        idx = finallindex(bi->loopIndex, bi->loopIndexB, loop_hit);
        if (ltable[idx].age == 0) {
          ltable[idx].dir = !taken; // ignored if no useDirectionBit
          ltable[idx].tag = bi->loopTag;
          ltable[idx].numIter = 0;
          ltable[idx].age = initialLoopAge;
          ltable[idx].confidence = 0;
          ltable[idx].currentIter = initialLoopIter;
          break;

        } else {
          ltable[idx].age--;
        }
        if (restrictAllocation) {
          break;
        }
      }
    }
  }
}

void specLoopUpdate(bool taken, BranchInfo *bi) {
  if (bi->loopHit >= 0) {
    int index = finallindex(bi->loopIndex, bi->loopIndexB, bi->loopHit);
    if (taken != ltable[index].dir) {
      ltable[index].currentIterSpec = 0;
    } else {
      ltable[index].currentIterSpec =
          (ltable[index].currentIterSpec + 1) & loopNumIterMask;
    }
  }
}

bool loopPredict(uint32_t pc, bool cond_branch, BranchInfo *bi,
                 bool prev_pred_taken, unsigned instShiftAmt) {
  bool pred_taken = prev_pred_taken;
  if (cond_branch) {
    // printf("loop pred\n");
    bi->loopPred = getLoop(pc, bi, useSpeculation, instShiftAmt);
    if (loopUseCounter >= 0 && bi->loopPredValid) {
      pred_taken = bi->loopPred;
      bi->loopPredUsed = true;
    }
    if (useSpeculation) {
      specLoopUpdate(pred_taken, bi);
    }
  }
  return pred_taken;
}

void squashLoop(BranchInfo *bi) {
  if (bi->loopHit >= 0) {
    int idx = finallindex(bi->loopIndex, bi->loopIndexB, bi->loopHit);
    ltable[idx].currentIterSpec = bi->currentIter;
  }
}
