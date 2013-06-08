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
  
  printf("\033[?1049h"   /* Initialise subterminal, if using an xterm */
	 "\033[H\033[2J" /* Clear the terminal, subterminal, if initialised is already clean */
	 "\033[?8c");    /* Switch to block cursor, if using TTY */
  /* Apply the previous instruction */
  fflush(stdout);
  
  /* Create an empty buffer not yet associeted with a file */
  createScratch();
  
  /* Load files and apply jumps from the command line */
  int fileLoaded = 0;
  for (int i = 1; i < argc; i++)
    if (**(argv + i) != ':')
      {
	/* Load file */
	fileLoaded = openFile(*(argv + i));
      }
    else if (fileLoaded)
      {
	/* Jump in last opened file */
	jump(*(argv + i) + 1); /* TODO this does not work*/
	fileLoaded = 0;
      }
  
  /* Create the screen and start display the files */
  createScreen(rows, cols);
  /* Start interaction */
  readInput(cols);
  
  printf("\033[?2c"      /* Restore cursor to underline, if using TTY */
	 "\033[H\033[2J" /* Clear the terminal, useless if in subterminal and not TTY */
	 "\033[?1049l"   /* Terminate subterminal, if using an xterm */
	 );
  /* Apply the previous instruction */
  fflush(stdout);
  /* Return the terminal to its previous state */
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_stty);
  
  /* Release resources, exit and report success */
  freeFrames();
  return 0;
}


/**
 * Make a jump in the current frame
 * 
 * @param  command  The jump command, in the format `[%row][:%column]`
 */
void jump(char* command)
{
  int has = 0, state = 1;
  long row = 0, col = 0;
  
  /* Parse command */
  char c;
  while ((c = *command++))
    if (('0' <= c) && (c <= '9'))
      {
	has |= state;
	if (state == 1)
	  row = row * 10 - (c & 15);
	else
	  col = col * 10 - (c & 15);
      }
    else if (c == ':')
      {
	if ((++state == 3))
	  break;
      }
    else
      {
	state = 3;
	break;
      }
  
  if (state == 3)
    {
      /* Invalid format */
      char* msg = (char*)malloc(28 * sizeof(char*));
      char* _msg = "\033[31mInvalid jump format\033[m";
      alert(msg);
      while ((*msg++ = *_msg++))
	;
    }
  else
    {
      /* Apply jump */
      row = (has & 1) ? -row : -1;
      col = (has & 2) ? -col : -1;
      applyJump(row, col);
    }
}


void createScreen(int rows, int cols)
{
  /* Create a line of spaces as large as the screen */
  char* spaces = (char*)malloc((cols + 1) * sizeof(char));
  for (int i = 0; i < cols; i++)
    *(spaces + i) = ' ';
  *(spaces + cols) = 0;
  
  /* Create the screen with the current frame, but do not fill it */
  printf("\033[07m");
  printf(spaces);
  printf("\033[1;1H\033[01mZecora  \033[21mPress ESC three times for help");
  printf("\033[27m");
  printf("\033[%i;1H\033[07m", rows - 1);
  printf(spaces);
  printf("\033[%i;3H(%li,%li)  ", rows - 1, getRow() + 1, getColumn() + 1);
  if (getFlags() & FLAG_MODIFIED)
    printf("\033[41m");
  char* filename = getFile();
  if (filename)
    {
      long sep = 0;
      for (long i = 0; *(filename + i); i++)
	if (*(filename + i) == '/')
	  sep = i;
      *(filename + sep) = 0;
      printf("%s/\033[01m%s\033[21;27m\n", filename, filename + sep + 1);
      *(filename + sep) = '/';
    }
  else
    printf("\033[01m*scratch*\033[21;27m\n");
  char* frameAlert = getAlert();
  if (frameAlert)
    printf("%s", frameAlert);
  printf("\033[00m\033[2;1H");
  
  /* Fill the screen */
  long r = getRow();
  long n = getLineCount(), m = getFirstRow() + rows - 3;
  char** lines = getLineBuffers();
  n = n < m ? n : m;
  cols--;
  for (long i = getFirstRow(); i < n; i++)
    {
      m = getLineLenght(*(lines + i));
      long j = i == r ? getFirstColumn() : 0;
      m = m < (cols + j) ? m : (cols + j);
      char* line = getLineContent(*(lines + i));
      /* TODO add support for combining diacriticals */
      /* TODO colour comment lines */
      long col = 0;
      for (; (j <= m) && (col < cols); j++)
	{
	  char c = *(line + j);
	  if (j == m)
	    if ((c & 0xC0) == 0x80)
	      {
		printf("%c", c);
		m++;
	      }
	    else
	      break;
	  else if (c == '\t')
	    {
	      printf(" ");
	      col++;
	      while ((col < cols) && (col & 7))
		{
		  printf(" ");
		  col++;
		}
	    }
	  else if ((0 <= c) && (c < ' '))
	    {
	      printf("\033[32m%c\033[39m", c);
	      col++;
	    }
	  else
	    {
	      printf("%c", c);
	      if ((c & 0xC0) == 0x80)
		m++;
	      else
		col++;
	    }
	}
      printf("\033[00m\n");
    }
  cols++;
  
  /* TODO ensure that the point is visible */
  
  /* Move the cursor to the position of the point */
  printf("\033[%li;%liH", getFirstRow() - getRow() + 2, getFirstColumn() - getColumn() + 1);
  
  /* Flush the screen */
  fflush(stdout);
}


void readInput(int cols)
{
  getchar();
}

