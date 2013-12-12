/**
 * Zecora – An minimal Emacs-clone intended for use in emergencies
 * 
 * Copyright © 2013  Mattias Andrée (maandree@member.fsf.org)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ZECORA_H__
#define __ZECORA_H__


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "frames.h"
#include "types.h"


/**
 * Program does not function with a narrower terminal
 */
#ifndef MINIMUM_COLS
#define MINIMUM_COLS  20
#endif

/**
 * Program does not function with a shorter terminal
 */
#ifndef MINIMUM_ROWS
#define MINIMUM_ROWS  10
#endif


#ifdef DEBUG
#  define xfork()  ((pid_t)-1)
#else
#  define xfork()  fork()
#  error fail
#endif


static void jump(char* command);
static void create_screen(dimm_t rows, dimm_t cols);
static void read_input(dimm_t cols);


#endif

