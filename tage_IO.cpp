#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>

/*#define DEBUG true*/
#define DEBUG false

#include "tage_types.h"

uint32_t u_clear_cnt;
uint32_t FH[FH_N_MAX][TN_MAX];
bool GHR[GHR_LENGTH];
uint8_t base_counter[BASE_ENTRY_NUM];
uint8_t tag_table[TN_MAX][TN_ENTRY_NUM];
uint8_t cnt_table[TN_MAX][TN_ENTRY_NUM];
uint8_t useful_table[TN_MAX][TN_ENTRY_NUM];

const uint32_t ghr_length[TN_MAX] = {8, 13, 32, 119};
const uint32_t fh_length[FH_N_MAX][TN_MAX] = {8, 11, 11, 11, 8, 8,
                                              8, 8,  7,  7,  7, 7};

void TAGE_update_HR(HR_IO *IO) {
  // update GHR
  for (int i = GHR_LENGTH - 1; i > 0; i--) {
    IO->GHR_new[i] = IO->GHR_old[i - 1];
  }
  IO->GHR_new[0] = IO->new_history;

  for (int k = 0; k < FH_N_MAX; k++) {
    for (int i = 0; i < TN_MAX; i++) {
      bool old_highest;
      old_highest = (IO->FH_old[k][i] >> (fh_length[k][i] - 1)) & (0x1);
      IO->FH_new[k][i] =
          (IO->FH_old[k][i] << 1) & ((0x1 << fh_length[k][i]) - 1);
      IO->FH_new[k][i] |= IO->new_history ^ old_highest;
      IO->FH_new[k][i] ^= IO->GHR_new[ghr_length[i]]
                          << (ghr_length[i] % fh_length[k][i]);
    }
  }
}

void TAGE_pred_1(pred_1_IO *IO) {
  uint32_t PC = IO->pc;
  IO->base_idx = PC % BASE_ENTRY_NUM; // PC[x:0]
  for (int i = 0; i < TN_MAX; i++) {
    IO->tag_pc[i] = IO->FH[1][i] ^ IO->FH[2][i] ^ (PC >> 2) & (0xff); // cal tag
  }
  for (int i = 0; i < TN_MAX; i++) {
    IO->index[i] = IO->FH[0][i] ^ (PC >> 2) & (0xfff); // cal index
  }
}

void TAGE_pred_2(pred_2_IO *IO) {
  uint8_t pcpn = TN_MAX;
  uint8_t altpcpn = TN_MAX;
  // Take the longest history entry
  for (int i = TN_MAX - 1; i >= 0; i--) {
    if (IO->tag_read[i] == IO->tag_pc[i]) {
      pcpn = i;
      break;
    }
  }
  // get the altpcpn info for updating policies
  for (int i = pcpn - 1; i >= 0; i--) {
    if (IO->tag_read[i] == IO->tag_pc[i]) {
      altpcpn = i;
      break;
    }
  }
  bool base_pred = IO->base_cnt >= 2 ? true : false;
  if (altpcpn >= TN_MAX) { // alt not found
    IO->altpred = base_pred;
  } else {
    if (IO->cnt_read[altpcpn] >= 4) {
      IO->altpred = true;
    } else {
      IO->altpred = false;
    }
  }
  if (pcpn >= TN_MAX) { // pcpn not found
    IO->pred = base_pred;
  } else if (IO->cnt_read[pcpn] >= 4) {
    IO->pred = true;
  } else
    IO->pred = false;

  IO->pcpn = pcpn;
  IO->altpcpn = altpcpn;
}

uint8_t bit_update_2(uint8_t data, bool is_inc) {
  uint8_t ret;
  if (is_inc) {
    if (data >= 3)
      ret = 3;
    else
      ret = data + 1;

  } else {
    if (data == 0)
      ret = 0;
    else
      ret = data - 1;
  }
  return ret;
}

uint8_t bit_update_3(uint8_t data, bool is_inc) {
  uint8_t ret;
  if (is_inc) {
    if (data >= 7)
      ret = 7;
    else
      ret = data + 1;

  } else {
    if (data == 0)
      ret = 0;
    else
      ret = data - 1;
  }
  return ret;
}

// ctrl : remain/inc/dec/alloc : 0123
void TAGE_do_update(update_IO *IO) {

  uint8_t pcpn = IO->pcpn;
  uint8_t altpcpn = IO->altpcpn;
  bool alt_pred = IO->alt_pred;
  bool pred_dir = IO->pred_dir;
  bool real_dir = IO->real_dir;
  // init all ctrl to prevent previous impact!
  for (int i = 0; i < TN_MAX; i++) {
    IO->useful_ctrl[i] = 0;
    IO->cnt_ctrl[i] = 0;
    IO->tag_ctrl[i] = 0;
    IO->base_ctrl = 0;
    IO->u_clear_ctrl = 0;
  }
  // 1. update 2-bit useful counter
  // pcpn found
  if (pcpn < TN_MAX) {
    if ((pred_dir != alt_pred)) {
      if (pred_dir == real_dir) {
        IO->useful_ctrl[pcpn] = 1;
        IO->useful_wdata[pcpn] = bit_update_2(IO->useful_read[pcpn], true);
      } else {
        IO->useful_ctrl[pcpn] = 2;
        IO->useful_wdata[pcpn] = bit_update_2(IO->useful_read[pcpn], false);
      }
    }

    // 2. update cnt
    if (real_dir == true) {
      IO->cnt_ctrl[pcpn] = 1;
      IO->cnt_wdata[pcpn] = bit_update_3(IO->cnt_read[pcpn], true);
    } else {
      IO->cnt_ctrl[pcpn] = 2;
      IO->cnt_wdata[pcpn] = bit_update_3(IO->cnt_read[pcpn], false);
    }
  }
  // pcpn not found, update base_counter
  else {
    if (real_dir == true) {
      IO->base_ctrl = 1;
      IO->base_wdata = bit_update_2(IO->base_read, true);
    } else {
      IO->base_ctrl = 2;
      IO->base_wdata = bit_update_2(IO->base_read, false);
    }
  }

  // 3. pred_dir != real_dir
  // If the provider component Ti is not the component using
  // the longest history (i.e., i < M) , we try to allocate an entry on a
  // predictor component Tk using a longer history than Ti (i.e., i < k < M)

  if (pred_dir != real_dir) {

    bool new_entry_found_j = false;
    int j_i;
    bool new_entry_found_k = false;
    int k_i;

    if (pcpn <= TN_MAX - 2 ||
        pcpn == TN_MAX) { // pcpn is NOT using the longest history or not found

      for (int i = pcpn == TN_MAX ? 0 : (pcpn + 1); i < TN_MAX; i++) {
        // try to find a useful==0
        uint8_t now_useful_i =
            IO->useful_ctrl[i] ? IO->useful_wdata[i] : IO->useful_read[i];
        if (now_useful_i == 0) {
          if (new_entry_found_j == false) {
            new_entry_found_j = true;
            j_i = i;
            continue;
          } else {
            new_entry_found_k = true;
            k_i = i;
            break;
          }
        }
      }
      if (new_entry_found_j == false) { // no new entry allocated
        for (int i = pcpn + 1; i < TN_MAX; i++) {
          if (IO->useful_ctrl[i] != 0) {
            IO->useful_wdata[i] = bit_update_2(
                IO->useful_wdata[i], false); // might already be updated !
          } else {
            IO->useful_wdata[i] = bit_update_2(IO->useful_read[i], false);
          }
          IO->useful_ctrl[i] = 2;
        }
      }
      // alocate new entry
      else {

        int random_pick = IO->lsfr % 3;
        if (new_entry_found_k == true && random_pick == 0) {
          IO->tag_ctrl[k_i] = 3;
          IO->tag_wdata[k_i] = IO->tag_pc[k_i];
          IO->cnt_ctrl[k_i] = 3;
          IO->cnt_wdata[k_i] = real_dir ? 4 : 3;
          IO->useful_ctrl[k_i] = 3;
          IO->useful_wdata[k_i] = 0;
        } else {
          IO->tag_ctrl[j_i] = 3;
          IO->tag_wdata[j_i] = IO->tag_pc[j_i];
          IO->cnt_ctrl[j_i] = 3;
          IO->cnt_wdata[j_i] = real_dir ? 4 : 3;
          IO->useful_ctrl[j_i] = 3;
          IO->useful_wdata[j_i] = 0;
        }
      }
    }
  }

  // 4. Periodically, the whole column of
  // most significant bits of the u counters is reset to zero, then whole column
  // of least significant bits are reset.
  uint32_t u_clear_cnt = IO->u_clear_cnt_read + 1;
  uint32_t u_cnt = u_clear_cnt & (0x7ff);
  uint32_t row_cnt = (u_clear_cnt >> 11) & (0xfff);
  bool u_msb_reset = ((u_clear_cnt) & 0x1) >> 23;

  IO->u_clear_cnt_wen = 1;
  IO->u_clear_cnt_wdata = u_clear_cnt;
  if (u_cnt == 0) {
    IO->u_clear_ctrl = 2;            // 0b10, do reset
    IO->u_clear_ctrl |= u_msb_reset; // msb or lsb
    IO->u_clear_idx = row_cnt;
  }
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
// this is only for C_SIM
// this is only for C_SIM
// this is only for C_SIM
pred_1_IO *pred_IO1;
pred_2_IO *pred_IO2;
update_IO *upd_IO;
HR_IO *hr_IO;

bool C_TAGE_do_pred(uint32_t pc) {

  pred_IO1->pc = pc;
  for (int k = 0; k < FH_N_MAX; k++) {
    for (int i = 0; i < TN_MAX; i++) {
      pred_IO1->FH[k][i] = FH[k][i];
    }
  }

  TAGE_pred_1(pred_IO1);

  pred_IO2->base_cnt = base_counter[pred_IO1->base_idx];
  for (int i = 0; i < TN_MAX; i++) {
    pred_IO2->cnt_read[i] = cnt_table[i][pred_IO1->index[i]];
    pred_IO2->tag_read[i] = tag_table[i][pred_IO1->index[i]];
    pred_IO2->tag_pc[i] = pred_IO1->tag_pc[i];
  }

  TAGE_pred_2(pred_IO2);

  return pred_IO2->pred;
}

void C_TAGE_do_update(uint32_t pc, bool real_dir, bool pred_dir) {
  // prepare Input
  upd_IO->pc = pc;
  upd_IO->real_dir = real_dir;
  upd_IO->pred_dir = pred_dir;
  upd_IO->alt_pred = pred_IO2->altpred;
  upd_IO->pcpn = pred_IO2->pcpn;
  upd_IO->altpcpn = pred_IO2->altpcpn;
  for (int i = 0; i < TN_MAX; i++) {
    upd_IO->useful_read[i] = useful_table[i][pred_IO1->index[i]];
    upd_IO->cnt_read[i] = cnt_table[i][pred_IO1->index[i]];
  }
  upd_IO->base_read = base_counter[pred_IO1->base_idx];
  upd_IO->lsfr = random();
  for (int i = 0; i < TN_MAX; i++) {
    upd_IO->tag_pc[i] = pred_IO1->tag_pc[i];
  }
  upd_IO->u_clear_cnt_read = u_clear_cnt;

  // do the logic
  TAGE_do_update(upd_IO);

  // access sequential components
  for (int i = 0; i < TN_MAX; i++) {
    if (upd_IO->useful_ctrl[i] != 0) {
      useful_table[i][pred_IO1->index[i]] = upd_IO->useful_wdata[i];
    }
    if (upd_IO->cnt_ctrl[i] != 0) {
      cnt_table[i][pred_IO1->index[i]] = upd_IO->cnt_wdata[i];
    }
    if (upd_IO->tag_ctrl[i] != 0) {
      tag_table[i][pred_IO1->index[i]] = upd_IO->tag_wdata[i];
    }
  }
  if (upd_IO->base_ctrl != 0) {
    base_counter[pred_IO1->base_idx] = upd_IO->base_wdata;
  }
  if (upd_IO->u_clear_ctrl != 0) {
    if ((upd_IO->u_clear_ctrl & 0x1) == 1) {
      for (int i = 0; i < TN_MAX; i++) {
        useful_table[i][upd_IO->u_clear_idx] &= 0x1;
      }
    } else {
      for (int i = 0; i < TN_MAX; i++) {
        useful_table[i][upd_IO->u_clear_idx] &= 0x2;
      }
    }
  }
  if (upd_IO->u_clear_cnt_wen) {
    u_clear_cnt = upd_IO->u_clear_cnt_wdata;
  }

  // update History regs
  for (int k = 0; k < FH_N_MAX; k++) {
    for (int i = 0; i < TN_MAX; i++) {
      hr_IO->FH_old[k][i] = FH[k][i];
    }
  }
  for (int i = 0; i < GHR_LENGTH; i++) {
    hr_IO->GHR_old[i] = GHR[i];
  }
  hr_IO->new_history = real_dir;

  // do the logic
  TAGE_update_HR(hr_IO);

  // access sequential components
  for (int k = 0; k < FH_N_MAX; k++) {
    for (int i = 0; i < TN_MAX; i++) {
      FH[k][i] = hr_IO->FH_new[k][i];
    }
  }
  for (int i = 0; i < GHR_LENGTH; i++) {
    GHR[i] = hr_IO->GHR_new[i];
  }
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
// this is only for TESTING
// this is only for TESTING
// this is only for TESTING
using namespace std;

ifstream log_file;

uint32_t log_pc;
bool log_bp;
bool show_details = false;

void log_read() {
  char pc_str[40];
  char branch_taken_str[5];
  log_file.getline(pc_str, 40);
  log_file.getline(branch_taken_str, 5);
  log_pc = 0;
  log_bp = false;
  for (int i = 0; i < 8; i++) {
    if (i != 0)
      log_pc = log_pc << 4;
    log_pc += pc_str[i] - (pc_str[i] >= 'a' ? ('a' - 10) : '0');
  }
  log_bp = branch_taken_str[0] - '0';
  if (show_details == true)
    printf("pc = %08x bp = %b ", log_pc, log_bp);
}
uint64_t inst_cnt = 0;
uint64_t bp_cnt = 0;
bool bp_dir;

void show_TAGE() {
  printf("base \n");
  for (int i = 0; i < BASE_ENTRY_NUM; i++) {
    if (base_counter[i] != 0)
      printf("index %3x base_counter %1x\n", i, base_counter[i]);
  }
  for (int i = 0; i < TN_MAX; i++) {
    printf("T %d\n", i);
    for (int j = 0; j < TN_ENTRY_NUM; j++) {
      if (tag_table[i][j] != 0 || cnt_table[i][j] != 0 ||
          useful_table[i][j] != 0)
        printf("index %3x tag %2x cnt %1x useful %1x\n", j, tag_table[i][j],
               cnt_table[i][j], useful_table[i][j]);
    }
  }
}

void show_HR() {
  printf("GHR\n");
  for (int i = 0; i < GHR_LENGTH; i++) {
    printf("%b", GHR[i]);
  }
  printf("\n");
  for (int i = 0; i < FH_N_MAX; i++) {
    for (int j = 0; j < TN_MAX; j++) {
      printf("FH%d%d %u\n", i, j, FH[i][j]);
    }
  }
}

int main() {
  srand(time(0));
  // log PC read
  log_file.open("../../rv-simu/log");
  int log_pc_max = DEBUG ? 10 : 1000000;

  while (log_pc_max--) {
    log_read();
    inst_cnt++;

    pred_1_IO IO1;
    pred_2_IO IO2;
    update_IO IO3;
    HR_IO IO4;

    pred_IO1 = &IO1;
    pred_IO2 = &IO2;
    upd_IO = &IO3;
    hr_IO = &IO4;

    bp_dir = C_TAGE_do_pred(log_pc);
    C_TAGE_do_update(log_pc, log_bp, bp_dir);
    if (show_details == true) {
      printf("TAGE_bp = %b", bp_dir);
      if (bp_dir == log_bp)
        printf("HIT%b", log_bp);
      printf("\n");
    }
    if (bp_dir == log_bp)
      bp_cnt++;
    /*show_TAGE();*/
  }
  log_file.close();

  double acc = (double)bp_cnt / inst_cnt;
  printf("[version: tage_IO] inst_cnt = %lu bp_cnt = %lu ACC = %.3f%%\n",
         inst_cnt, bp_cnt, acc * 100);
  /*show_TAGE();*/
  /*show_HR();*/
  return 0;
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
