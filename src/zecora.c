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


#define  P  __SIZEOF_POINTER__


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
    /* Prepare to hold 4 frames initially */
    preparedFrames = 4;
    frames = (void**)malloc(preparedFrames * sizeof(void*));
  }
  else if (openFrames == preparedFrames)
  {
    /* When full, prepare to hold twice as much */
    preparedFrames <<= 1;
    void** newFrames = (void**)malloc(preparedFrames * sizeof(void*));
    /* Copy old buffer to new buffer */
    for (int i = 0; i < openFrames; i++)
      *(newFrames + i) = *(frames + i);
    /* Free old buffer and apply new buffer */
    free(frames);
    frames = newFrames;
  }
}


/**
 * Create an empty document that is not yet associated with a file
 */
void createScratch()
{
  /* Ensure that another frame can be held */
  prepareFrameBuffer();
  
  /* Create new frame */
  currentFrame = openFrames++;
  void** frame = (void**)(*(frames + currentFrame) = (void*)malloc(8 * sizeof(void*)));
  *(frame + 0) = 0;                            /* Point line           */
  *(frame + 1) = 0;                            /* Point column         */
  *(frame + 2) = 0;                            /* Mark line            */
  *(frame + 3) = 0;                            /* Mark column          */
  *(frame + 4) = 0;                            /* First visible line   */
  *(frame + 5) = 0;                            /* First visible column */
  *(frame + 6) = (void*)1;                     /* Number of lines      */
  *(frame + 7) = 0;                            /* Filename             */
  *(frame + 8) = 0;                            /* Message              */
  *(frame + 9) = (void*)malloc(sizeof(char*)); /* Buffer               */
  
  /* Create one empty line */
  char* line0 = *(char**)*(frame + 2 * P) = (char*)malloc(2 * P * sizeof(char)); /* used:P, allocated:P, line:* */
  for (int i = 0; i < 2 * P; i++)
    *(line0 + i) = 0;
}


/**
 * Opens a new file
 * 
 * @param   filename      The filename of the file to open
 * @return  0             The file was successfully opened
 * @return  <0            The file is already opened in the frame whose index is bitwise negation of the returned integer
 * @return  >0            Failed to open the file, the returned integer is an error code.
 * 
 * @throws  EACCES        Search permission is denied for one of the directories in the path prefix of the path
 * @throws  EFAULT        Bad address
 * @throws  ELOOP         Too many symbolic links encountered while traversing the path.
 * @throws  ENAMETOOLONG  The path is too long
 * @throws  ENOENT        A component of the path does not exist, or the path is an empty string
 * @throws  ENOMEM        Out of memory (i.e., kernel memory)
 * @throws  ENOTDIR       A component of the path prefix of path is not a directory
 * @throws  EOVERFLOW     The file size, inode number, or number of block is too large for the system
 * @throws  256           The file is not a regular file
 * @throws  257           Failed to read file
 */
long openFile(char* filename)
{
  /* Verify that the file is a regular file or does not exist but can be created */
  int fileExists = 1;
  struct stat fileStats;
  if (stat(filename, &fileStats))
    {
      int error = errno;
      if ((error == ENOENT) && (*filename != 0))
	{
	  int namesize = 0;
	  while (*(filename + namesize++))
	    ;
	  char* dirname = (char*)malloc(namesize * sizeof(char));
	  *(dirname + --namesize) = 0;
	  while (*(dirname + --namesize) == '/')
	    ;
	  while (*(dirname + --namesize) != '/')
	    ;
	  *(dirname + namesize) = 0;
	  if (!stat(dirname, &fileStats))
	    if (S_ISDIR(fileStats.st_mode))
	      error = fileExists = 0;
	  free(dirname);
	}
      if (error != 0)
	return error;
    }
  else if (S_ISREG(fileStats.st_mode))
    return 256;
  
  /* Get the optimal reading block size */
  size_t blockSize = fileExists ? (size_t)(fileStats.st_blksize) : 0;
  /* Get the size of the file */
  unsigned long size = 0;
  /* Buffer for the content file */
  char* buffer = fileExists ? (char*)malloc(fileStats.st_size) : 0;
  
  /* Read file */
  size_t got;
  FILE* file = fileExists ? fopen(filename, "r") : 0;
  if (fileExists)
    {
      for (;;)
	if ((got = fread(buffer, 1, blockSize, file)) != blockSize)
	  {
	    if (feof(file))
	      {
		/* End of file */
		size += got;
		clearerr(file);
		break;
	      }
	    /* Failed to read the file */
	    free(buffer);
	    clearerr(file);
	    fclose(file);
	    return 257;
	  }
	else
	  size += got;
      fclose(file);
    }
  
  /* Count the number of lines */
  long lines = 1;
  for (long i = 0; *(buffer + i); i++)
    if (*(buffer + i) == '\n')
      lines++;
  
  /* Copy filename so it later can be freed */
  long namesize = 0;
  while (*(filename + namesize++))
    ;
  char* _filename = (char*)malloc(namesize * sizeof(char));
  for (int i = 0; i < namesize; i++)
    *(_filename + i) = *(filename + i);
  
  /* Ensure that another frame can be held */
  prepareFrameBuffer();
  
  /* Create new frame */
  currentFrame = openFrames++;
  void** frame = (void**)(*(frames + currentFrame) = (void*)malloc(8 * sizeof(void*)));
  *(frame + 0) = 0;                                    /* Point line           */
  *(frame + 1) = 0;                                    /* Point column         */
  *(frame + 2) = 0;                                    /* Mark line            */
  *(frame + 3) = 0;                                    /* Mark column          */
  *(frame + 4) = 0;                                    /* First visible line   */
  *(frame + 5) = 0;                                    /* First visible column */
  *(frame + 6) = (void*)lines;                         /* Number of lines      */
  *(frame + 7) = _filename;                            /* Filename             */
  *(frame + 8) = 0;                                    /* Message              */
  *(frame + 9) = (void*)malloc(lines * sizeof(char*)); /* Buffer               */
  
  /* Populate lines */
  long bufptr = 0;
  for (long i = 0; i < lines; i++)
    {
      /* Get the span of the line */
      long start = bufptr;
      while (bufptr < size)
	{
	  if (*(buffer + bufptr) == '\n')
	    break;
	  bufptr++;
	}
      long linesize = bufptr - start;
      bufptr = start;
      
      /* Create line buffer and fill it with metadata */
      char* line = *((char**)*(frame + 2 * P + linesize) + i) = (char*)malloc((2 * P + linesize) * sizeof(char)); /* used:P, allocated:P, line:* */
      for (int _ = 0; _ < 2; _++)
	for (int j = P - 1; j >= 0; j--)
	  *line++ = (linesize >> (j * 8)) & 255;
      
      /* Fill the line with the data */
      for (int j = 0; j < linesize; j++)
	*(line + j) = *(buffer + bufptr++);
    }
  free(buffer);
  
  /* Report that a new frame as been created */
  return 0;
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

