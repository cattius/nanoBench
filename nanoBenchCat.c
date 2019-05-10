// nanoBench
//
// Copyright (C) 2019 Andreas Abel
//
// This program is free software: you can redistribute it and/or modify it under the terms of version 3 of the GNU Affero General Public License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include "nanoBenchCat.h"
#define USR 1 //count performance events when processor is operating at a privilege level > 0
#define OS 0  //don't count performance events when processor is operating at privilege level 0

sigjmp_buf buf;
struct sigaction handler;
int sigHandledOnce = 0;

#if __x86_64__
	#define IP REG_RIP 
#else
	#define IP REG_EIP 
#endif

void signalHandler(int sig, siginfo_t* siginfo, void* context){
	if(!sigHandledOnce){
		printf("SIG %d\n", sig);
		sigHandledOnce = 1;
	}
	if(sig != 5) ((ucontext_t*)context)->uc_mcontext.gregs[IP] += code_length;
}


size_t mmap_file(char* filename, char** content) {
    int fd = open(filename, O_RDONLY);
    size_t len = lseek(fd, 0, SEEK_END);
    *content = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (*content == MAP_FAILED) {
        fprintf(stderr, "Error reading %s\n", filename);
        exit(1);
    }
    close(fd);
    return len;
}

int profileInstr(char *instr, char *configFileName, int instrLen, bool beQuiet) {

  //giving handler its own stack allows it to continue handling even in case stack pointer has ended up wildly out
  stack_t sigstack;
  sigstack.ss_sp = malloc(SIGSTKSZ);
  if (sigstack.ss_sp == NULL) {
    perror("malloc");
    return 1;
  }
  sigstack.ss_size = SIGSTKSZ;
  sigstack.ss_flags = 0;
  if (sigaltstack(&sigstack, NULL) == -1) {
    perror("sigaltstack");
    return 1;
  }

  memset(&handler, 0, sizeof(handler));
  handler.sa_flags = SA_SIGINFO | SA_ONSTACK;
  handler.sa_sigaction = signalHandler;
  if (  sigaction(SIGILL, &handler, NULL) < 0   || \
        sigaction(SIGFPE, &handler, NULL) < 0   || \
        sigaction(SIGSEGV, &handler, NULL) < 0  || \
        sigaction(SIGBUS, &handler, NULL) < 0   || \
        sigaction(SIGTRAP, &handler, NULL) < 0 ) {
          perror("sigaction");
          return 1;
  }


    char* config_file_name = NULL;
    int usr = 1;
    int os = 1; //must be enabled to avoid negative counts when exception handling. unfortunate overhead for valid instrs.

    struct option long_opts[] = {
        {"code", required_argument, 0, 'c'},
        {"code_init", required_argument, 0, 'i'},
        {"config", required_argument, 0, 'f'},
        {"n_measurements", required_argument, 0, 'n'},
        {"unroll_count", required_argument, 0, 'u'},
        {"loop_count", required_argument, 0, 'l'},
        {"warm_up_count", required_argument, 0, 'w'},
        {"initial_warm_up_count", required_argument, 0, 'a'},
        {"avg", no_argument, &aggregate_function, AVG_20_80},
        {"median", no_argument, &aggregate_function, MED},
        {"min", no_argument, &aggregate_function, MIN},
        {"basic_mode", no_argument, &basic_mode, 1},
        {"no_mem", no_argument, &no_mem, 1},
        {"verbose", no_argument, &verbose, 1},
        {"cpu", required_argument, 0, 'p'},
        {"usr", required_argument, 0, 'r'},
        {"os", required_argument, 0, 's'},
        {"debug", no_argument, &debug, 1},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    code = (char*) malloc(sizeof(char)*instrLen);
    code = memcpy(code, instr, instrLen);
    code_length = instrLen;
    sigHandledOnce = 0; //only print out exception type from signal handler once per runtime
    //can't use strlen here as otherwise it skips 00 bytes

    /*************************************
     * Check CPUID and parse config file
     ************************************/
    if (check_cpuid()) {
        return 1;
    }

    char* config_mmap;
    size_t len = mmap_file(configFileName, &config_mmap);
    pfc_config_file_content = calloc(len+1, sizeof(char));
    memcpy(pfc_config_file_content, config_mmap, len);
    parse_counter_configs();

    /*************************************
     * Pin thread to CPU
     ************************************/
    if (cpu == -1) {
        cpu = sched_getcpu();
    }

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);

    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        fprintf(stderr, "Error: Could not pin thread to core %d\n", cpu);
        return 1;
    }

    /*************************************
     * Allocate memory
     ************************************/
    size_t runtime_code_length = code_init_length + (unroll_count)*code_length*2 + 10000;
    size_t cleanup_code_length = 200;	//only need 146 but allowing spare in case some templates are longer
    posix_memalign((void**)&runtime_code, sysconf(_SC_PAGESIZE), runtime_code_length);
    if (!runtime_code) {
        fprintf(stderr, "Error: Failed to allocate memory for runtime_code\n");
        return 1;
    }
    if (mprotect(runtime_code, runtime_code_length, (PROT_READ | PROT_WRITE |PROT_EXEC))) {
        fprintf(stderr, "Error: mprotect failed\n");
        return 1;
    }
    posix_memalign((void**)&cleanup_code, sysconf(_SC_PAGESIZE), cleanup_code_length);
    if (!cleanup_code) {
        fprintf(stderr, "Error: Failed to allocate memory for cleanup_code\n");
        return 1;
    }
    if (mprotect(cleanup_code, cleanup_code_length, (PROT_READ | PROT_WRITE |PROT_EXEC))) {
        fprintf(stderr, "Error: mprotect failed\n");
        return 1;
    }

    posix_memalign((void**)&runtime_r14, sysconf(_SC_PAGESIZE), RUNTIME_R_SIZE);
    posix_memalign((void**)&runtime_rbp, sysconf(_SC_PAGESIZE), RUNTIME_R_SIZE);
    posix_memalign((void**)&runtime_rdi, sysconf(_SC_PAGESIZE), RUNTIME_R_SIZE);
    posix_memalign((void**)&runtime_rsi, sysconf(_SC_PAGESIZE), RUNTIME_R_SIZE);
    posix_memalign((void**)&runtime_rsp, sysconf(_SC_PAGESIZE), RUNTIME_R_SIZE);
    if (!runtime_r14 || !runtime_rbp || !runtime_rdi || !runtime_rsi || !runtime_rsp) {
        fprintf(stderr, "Error: Could not allocate memory for runtime_r*\n");
        return 1;
    }

    for (int i=0; i<MAX_PROGRAMMABLE_COUNTERS; i++) {
        measurement_results[i] = malloc(n_measurements*sizeof(int64_t));
        measurement_results_base[i] = malloc(n_measurements*sizeof(int64_t));
        if (!measurement_results[i] || !measurement_results_base[i]) {
            fprintf(stderr, "Error: Could not allocate memory for measurement_results\n");
            return 1;
        }
    }

    /*************************************
     * Fixed-function counters
     ************************************/

    long base_unroll_count = (basic_mode?0:unroll_count);
    long main_unroll_count = (basic_mode?unroll_count:2*unroll_count);
    long base_loop_count = (basic_mode?0:loop_count);
    long main_loop_count = loop_count;

    char buf[100];
    char* measurement_template;

    if(!beQuiet){

	    //intel lengths are 136 to 148

	    if (is_AMD_CPU) {
		if (no_mem) {
		    measurement_template = (char*)&measurement_RDTSC_template_noMem;
		} else {
		    measurement_template = (char*)&measurement_RDTSC_template;
		}
	    } else {
		if (no_mem) {
		    measurement_template = (char*)&measurement_FF_template_Intel_noMem;
		} else {
		    measurement_template = (char*)&measurement_FF_template_Intel;
		}
	    }

	    run_warmup_experiment(measurement_template);

	    if (is_AMD_CPU) {
		run_experiment(measurement_template, measurement_results_base, 1, base_unroll_count, base_loop_count);
		run_experiment(measurement_template, measurement_results, 1, main_unroll_count, main_loop_count);

		if (verbose) {
		    printf("\nRDTSC results (unroll_count=%ld, loop_count=%ld):\n\n", base_unroll_count, base_loop_count);
		    print_all_measurement_results(measurement_results_base, 1);
		    printf("RDTSC results (unroll_count=%ld, loop_count=%ld):\n\n", main_unroll_count, main_loop_count);
		    print_all_measurement_results(measurement_results, 1);
		}

		printf("%s", compute_result_str(buf, sizeof(buf), "RDTSC", 0));
	    } else {
		configure_perf_ctrs_FF(usr, os);

		run_experiment(measurement_template, measurement_results_base, 4, base_unroll_count, base_loop_count);
		run_experiment(measurement_template, measurement_results, 4, main_unroll_count, main_loop_count);

		if (verbose) {
		    printf("\nRDTSC and fixed-function counter results (unroll_count=%ld, loop_count=%ld):\n\n", base_unroll_count, base_loop_count);
		    print_all_measurement_results(measurement_results_base, 4);
		    printf("RDTSC and fixed-function counter results (unroll_count=%ld, loop_count=%ld):\n\n", main_unroll_count, main_loop_count);
		    print_all_measurement_results(measurement_results, 4);
		}

		printf("%s", compute_result_str(buf, sizeof(buf), "RDTSC", 0));
		printf("%s", compute_result_str(buf, sizeof(buf), "Instructions retired", 1));
		printf("%s", compute_result_str(buf, sizeof(buf), "Core cycles", 2));
		printf("%s", compute_result_str(buf, sizeof(buf), "Reference cycles", 3));
	    }
    }

    /*************************************
     * Programmable counters
     ************************************/
    if (is_AMD_CPU) {
        if (no_mem) {
            measurement_template = (char*)&measurement_template_AMD_noMem;
        } else {
            measurement_template = (char*)&measurement_template_AMD;
        }
    } else {
        if (no_mem) {
            measurement_template = (char*)&measurement_template_Intel_noMem;
        } else {
            measurement_template = (char*)&measurement_template_Intel;
        }
    }

    for (size_t i=0; i<n_pfc_configs; i+=n_programmable_counters) {
        size_t end = i + n_programmable_counters;
        if (end > n_pfc_configs) {
            end = n_pfc_configs;
        }

        configure_perf_ctrs_programmable(i, end, usr, os);

        run_experiment(measurement_template, measurement_results_base, n_programmable_counters, base_unroll_count, base_loop_count);
        run_experiment(measurement_template, measurement_results, n_programmable_counters, main_unroll_count, main_loop_count);

        if (verbose) {
            printf("\nProgrammable counter results (unroll_count=%ld, loop_count=%ld):\n\n", base_unroll_count, base_loop_count);
            print_all_measurement_results(measurement_results_base, n_programmable_counters);
            printf("Programmable counter results (unroll_count=%ld, loop_count=%ld):\n\n", main_unroll_count, main_loop_count);
            print_all_measurement_results(measurement_results, n_programmable_counters);
        }

        for (int c=0; c < n_programmable_counters && i + c < n_pfc_configs; c++) {
            if (!pfc_configs[i+c].invalid) printf("%s", compute_result_str(buf, sizeof(buf), pfc_configs[i+c].description, c));
        }
    }

    /*************************************
     * Cleanup
     ************************************/
    free(runtime_code);
    free(runtime_r14);
    free(runtime_rbp);
    free(runtime_rdi);
    free(runtime_rsi);
    free(runtime_rsp);
    free(sigstack.ss_sp);

    for (int i=0; i<MAX_PROGRAMMABLE_COUNTERS; i++) {
        free(measurement_results[i]);
        free(measurement_results_base[i]);
    }

    if (pfc_config_file_content) {
        free(pfc_config_file_content);
    }

    return 0;
}
