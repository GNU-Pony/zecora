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
 * The number of opened frames
 */
static long openFrames = 0;

/**
 * The number of frames the fits in `frames`
 */
static long preparedFrames = 0;

/**
 * The index of the current frame
 */
static long currentFrame = -1;

/**
 * The opened frames
 */
static void** frames = 0;



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
	 "\033[?1049l"); /* Terminate subterminal, if using an xterm */
  /* Apply the previous instruction */
  fflush(stdout);
  /* Return the terminal to its previous state */
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_stty);
  
  /* Exit and report success */
  return 0;
}


/**
 * Prepare the frame buffer to hold one more frame
 */
void prepareFrameBuffer()
{
  if (frames == 0)
  {
    preparedFrames = 4;
    frames = (void**)malloc(preparedFrames * sizeof(void*));
  }
  else if (openFrames == preparedFrames)
  {
    preparedFrames <<= 1;
    void** newFrames = (void**)malloc(preparedFrames * sizeof(void*));
    for (int i = 0; i < openFrames; i++)
      *(newFrames + i) = *(frames + i);
    free(frames);
    frames = newFrames;
  }
}


/**
 * Create an empty document that is not yet associated with a file
 */
void createScratch()
{
  prepareFrameBuffer();
  currentFrame = openFrames++;
  void** frame = (void**)(*(frames + currentFrame) = (void*)malloc(8 * sizeof(void*)));
  *(frame + 0) = 0;                            /* Point line */
  *(frame + 1) = 0;                            /* Point column */
  *(frame + 2) = 0;                            /* Mark line */
  *(frame + 3) = 0;                            /* Mark column */
  *(frame + 4) = 0;                            /* First line */
  *(frame + 5) = (void*)1;                     /* Number of lines */
  *(frame + 6) = 0;                            /* Filename */
  *(frame + 7) = 0;                            /* Message */
  *(frame + 8) = (void*)malloc(sizeof(char*)); /* Buffer */
  
  /* Create one empty line */
  *(char**)*(frame + 8) = (char*)malloc(9 * sizeof(char)); /* line:* prepared:8 */
  for (int i = 0; i < 9; i++)
    *(*(char**)*(frame + 8) + i) = 0;
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
  getchar();
}

