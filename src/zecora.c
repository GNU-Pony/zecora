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
#include "zecora.h"


/**
 * This is the mane entry point of Zecora
 * 
 * @param   argc  The number of elements in `argv`
 * @param   argv  The command line arguments
 * @return        Exit value, 0 on success
 */
int main(int argc, char** argv)
{
  /* Determine the size of the terminal */
  struct winsize win;
  ioctl(1, TIOCGWINSZ, (char*)&win);
  int rows = win.ws_row;
  int cols = win.ws_col;
  
  /* Check the size of the terminal */
  if ((rows < 10) || (cols < 20))
    {
      printf("You will need at least in 10 rows by 20 columns terminal!");
      return 1;
    }
  
  /* Disable signals from keystrokes, keystroke echoing and keystroke buffering */
  struct termios saved_stty;
  struct termios stty;
  tcgetattr(STDIN_FILENO, &saved_stty);
  tcgetattr(STDIN_FILENO, &stty);
  stty.c_lflag &= ~(ICANON | ECHO | ISIG);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &stty);
  
  printf("\033[?1048h"   /* Initialise subterminal, if using an xterm */
	 "\033[H\033[2J" /* Clear the terminal, subterminal, if initialised is already clean */
	 "\033[?8c");    /* Switch to block cursor, if using TTY */
  /* Apply the previous instruction */
  fflush(stdout);
  
  /* Load files and apply jumps from the command line */
  int fileLoaded = 0;
  for (int i = 1; i < argc; i++)
    if (**(argv + i) != ':')
      /* Load file */
      fileLoaded = openFile(*(argv + i));
    else if (fileLoaded)
      {
	/* Jump in last openned file */
	jump(*(argv + i) + 1);
	fileLoaded = 0;
      }
  
  /* Create the screen and start display the files */
  createScreen(rows, cols);
  /* Start interaction */
  readInput(cols);
  
  
  printf("\033[?2c"      /* Restore cursor to underline, if using TTY */
	 "\033[H\033[2J" /* Clear the terminal, useless if in subterminal and not TTY */
	 "\033[?1048l"); /* Terminate subterminal, if using an xterm */
  /* Apply the previous instruction */
  fflush(stdout);
  /* Return the terminal to its previous state */
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_stty);
  
  /* Exit and report success */
  return 0;
}


int openFile(char* filename)
{
  return 1;
}

void jump(char* command)
{
  /**/
}

void createScreen(int rows, int cols)
{
  /**/
}

void readInput(int cols)
{
  /**/
}

