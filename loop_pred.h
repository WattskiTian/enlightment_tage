#ifndef _LOOP_PRED_H_
#define _LOOP_PRED_H_

#include <cstdint>

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

bool loopPredict(uint32_t pc, bool cond_branch, BranchInfo *bi,
                 bool prev_pred_taken, unsigned instShiftAmt);

void loopUpdate(uint32_t pc, bool taken, BranchInfo *bi, bool tage_pred);

#endif
