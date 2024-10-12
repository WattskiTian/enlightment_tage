#ifndef _TAGE_TYPES_H_
#define _TAGE_TYPES_H_

#include <cmath>
#include <cstdint>

/*#define LOG2_SUM(x, y) (static_cast<int>(std::ceil((log2(x) + log2(y)))))*/

#define BASE_ENTRY_NUM 2048
#define GHR_LENGTH 256
#define TN_MAX 4 // 0-indexed, which means 0,1,2,3
#define TN_ENTRY_NUM 4096
#define FH_N_MAX 3 // how many different types of Folded history
/*#define USEFUL_RESET_VAL 262144 // 256K*/
#define uBitPeriod 2048
/*#define u_MSB_RESET_BIT LOG2_SUM(uBitPeriod, TN_ENTRY_NUM)*/

// cal tag + index for pred
struct pred_1_IO {
  // Input
  uint32_t pc;
  uint32_t FH[FH_N_MAX][TN_MAX];

  // Output
  uint32_t base_idx;
  uint8_t tag_pc[TN_MAX];
  uint32_t index[TN_MAX];
};

// pred + altpred + pcpn + altpcpn
struct pred_2_IO {
  // Input
  uint8_t base_cnt;         // base_table[base_idx]
  uint8_t tag_pc[TN_MAX];   //  pc tag
  uint8_t tag_read[TN_MAX]; // tag read from tag table
  uint8_t cnt_read[TN_MAX];

  // Output
  bool pred;
  bool altpred;
  uint8_t pcpn;
  uint8_t altpcpn;
};

// table ctrl signals
struct update_IO {
  // Input
  uint32_t pc;
  bool real_dir;
  bool pred_dir;
  bool alt_pred;
  uint8_t pcpn;
  uint8_t altpcpn;
  uint8_t useful_read[TN_MAX];
  uint8_t cnt_read[TN_MAX];
  uint8_t base_read;
  uint8_t lsfr;              // random value
  uint8_t tag_pc[TN_MAX];    // OLD TAG_VALUE
  uint32_t u_clear_cnt_read; // useful bit clear cnt
                             // ctrl : 1+12+11

  // Output
  // ctrl : remain/inc/dec/alloc : 0123
  // default idx : index[TN_MAX]
  uint8_t useful_ctrl[TN_MAX];
  uint8_t useful_wdata[TN_MAX]; // potential wdata
  uint8_t cnt_ctrl[TN_MAX];
  uint8_t cnt_wdata[TN_MAX]; // potential wdata
  uint8_t tag_ctrl[TN_MAX];
  uint8_t tag_wdata[TN_MAX]; // potential wdata
  uint8_t base_ctrl;
  uint8_t base_wdata;   // potential wdata
  uint8_t u_clear_ctrl; // do/not do , hi/lo
  uint32_t u_clear_idx;
  bool u_clear_cnt_wen;
  uint32_t u_clear_cnt_wdata;
};

// update Folded history FH + GHR
struct HR_IO {
  // Input
  uint32_t FH_old[FH_N_MAX][TN_MAX];
  bool GHR_old[GHR_LENGTH];
  bool new_history;

  // Output
  uint32_t FH_new[FH_N_MAX][TN_MAX];
  bool GHR_new[GHR_LENGTH];
};

#endif
