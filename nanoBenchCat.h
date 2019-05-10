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

#ifndef NANOBENCHCAT_H
#define NANOBENCHCAT_H

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#include <ucontext.h>
#include <stdbool.h>
#include "common/nanoBench.h"

int profileInstr(char *instr, char *configFileName, int instrLen, bool beQuiet);

#endif
