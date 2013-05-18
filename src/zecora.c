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
  
  /* Disable signals from keystrokes, keystroke echoing and keystroke buffering */
  struct termios saved_stty;
  struct termios stty;
  tcgetattr(STDIN_FILENO, &saved_stty);
  tcgetattr(STDIN_FILENO, &stty);
  stty.c_lflag &= ~(ICANON | ECHO | ISIG);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &stty);
  /* Initialise subterminal, if using an xterm */
  printf("\033[?1048h");
  /* Clear the terminal, subterminal, if initialised is already clean */
  printf("\033[H\033[2J");
  /* Switch to block cursor, if using TTY */
  printf("\033[?8c");
  /* Apply the three previous instructions */
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
  
  /* Restore cursor to underline, if using TTY */
  printf("\033[?2c");
  /* Clear the terminal, useless if in subterminal and not TTY */
  printf("\033[H\033[2J");
  /* Terminate subterminal, if using an xterm */
  printf("\033[?1048l");
  /* Apply the three previous instructions */
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

