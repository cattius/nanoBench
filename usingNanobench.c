#define _GNU_SOURCE
#include "nanoBenchCat.h"

/* ===================================================
* NOTE: you cannot run this on its own!
* You must run it via sudo ./run.sh or it will fail
====================================================== */

int main(){

  //Configuration:
  //update lenIllegalInstruction with the length of your instruction (used if it causes an exception)
  //update instr with instruction to test as machine code bytes
  //update config with your counter configuration
  //=================================================================================================
  int instrLen = 2;
  char* instr = "\x0f\x0b";
  char* config = "configs/cfg_Broadwell_common.txt";
  //=================================================================================================
 
  profileInstr(instr, config, instrLen, false);
  //the fourth parameter controls whether or not we suppress the fixed counters (useful if you just want a single programmable counter to easily parse the results)

  printf("Done!\n");
  return 0;

}
