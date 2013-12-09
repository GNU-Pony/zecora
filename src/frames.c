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
#include "frames.h"


/**
 * The number of bytes in the types: void*, long, size_t, off_t, &c
 */
#define  P  __SIZEOF_POINTER__


/**
 * The number of opened frames
 */
static long open_frames = 0;

/**
 * The number of frames that fits in `frames`
 */
static long prepared_frames = 0;

/**
 * The index of the current frame
 */
static long current_frame = -1;

/**
 * The opened frames
 */
static void** frames = 0;



/**
 * Prepare the frame buffer to hold one more frame
 */
void prepare_frame_buffer()
{
  if (frames == 0)
  {
    /* Prepare to hold 4 frames initially */
    prepared_frames = 4;
    frames = malloc(prepared_frames * sizeof(void*));
  }
  else if (open_frames == prepared_frames)
  {
    /* When full, prepare to hold twice as much */
    prepared_frames <<= 1;
    void** newFrames = malloc(prepared_frames * sizeof(void*));
    /* Copy old buffer to new buffer */
    for (int i = 0; i < open_frames; i++)
      *(newFrames + i) = *(frames + i);
    /* Free old buffer and apply new buffer */
    free(frames);
    frames = newFrames;
  }
}


/**
 * Create an empty document that is not yet associated with a file
 */
void create_scratch()
{
  /* Ensure that another frame can be held */
  prepare_frame_buffer();
  
  /* Create new frame */
  current_frame = open_frames++;
  void** frame = (void**)(*(frames + current_frame) = malloc(11 * sizeof(void*)));
  *(frame +  0) = 0;                            /* Point line           */
  *(frame +  1) = 0;                            /* Point column         */
  *(frame +  2) = 0;                            /* Mark line            */
  *(frame +  3) = 0;                            /* Mark column          */
  *(frame +  4) = 0;                            /* First visible line   */
  *(frame +  5) = 0;                            /* First visible column */
  *(frame +  6) = (void*)1;                     /* Number of lines      */
  *(frame +  7) = 0;                            /* Filename             */
  *(frame +  8) = 0;                            /* Message              */
  *(frame +  9) = (void*)malloc(sizeof(char*)); /* Buffer               */
  *(frame + 10) = 0;                            /* Flags                */
  
  /* Create one empty line */
  char* line0 = *(char**)*(frame + 9) = malloc(2 * P * sizeof(char)); /* used:P, allocated:P, line:* */
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
long open_file(char* filename)
{
  /* Return the ~index of the frame holding the file if it already exists */
  long found = find_file(filename);
  if (found >= 0)
    return ~found;
  
  /* Verify that the file is a regular file or does not exist but can be created */
  int file_exists = 1;
  struct stat file_stats;
  if (stat(filename, &file_stats))
    {
      int error = errno;
      if ((error == ENOENT) && (*filename != 0))
	{
	  int namesize = 0;
	  while (*(filename + namesize++))
	    ;
	  char* dirname = malloc(namesize * sizeof(char));
	  for (long i = 0; i < namesize; i++)
	    *(dirname + i) = *(filename + i);
	  *(dirname + --namesize) = 0;
	  while (namesize && (*(dirname + --namesize) == '/'))
	    ;
	  while (namesize && (*(dirname + --namesize) != '/'))
	    ;
	  *(dirname + namesize) = 0;
	  if ((*(dirname + namesize) == 0) || (!stat(dirname, &file_stats) && S_ISDIR(file_stats.st_mode)))
	    error = file_exists = 0;
	  free(dirname);
	}
      if (error != 0)
	return error;
    }
  else if (S_ISREG(file_stats.st_mode) == 0)
    return 256;
  
  /* Get the optimal reading block size */
  size_t block_size = file_exists ? (size_t)(file_stats.st_blksize) : 0;
  /* Get the size of the file */
  unsigned long size = 0;
  unsigned long reported_size = file_stats.st_size;
  /* Buffer for the content file */
  char* buffer = file_exists ? malloc(reported_size) : NULL;
  
  /* Read file */
  size_t got;
  FILE* file = file_exists ? fopen(filename, "r") : NULL;
  if (file_exists)
    {
      for (;;)
	{
	  unsigned long read_block = reported_size - size;
	  read_block = block_size < read_block ? block_size : read_block;
	  if (read_block == 0)
	    break;
	  if ((got = fread(buffer + size, 1, read_block, file)) != read_block)
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
	}
      fclose(file);
    }
  
  /* Count the number of lines */
  long lines = 1;
  if ((buffer))
    for (unsigned long i = 0; i < size; i++)
      if (*(buffer + i) == '\n')
	lines++;
  
  /* Copy filename so it later can be freed as well as get the real path */
  char* _filename = 0;
  if ((buffer))
    {
      _filename = malloc(PATH_MAX * sizeof(char));
      char* returned_realpath;
      if ((returned_realpath = realpath(filename, _filename)) == 0)
	return errno;
    }
  else
    {
      /* TODO get the realpath for the directroy */
      long n = 0;
      while (*(filename + n++))
	;
      _filename = malloc(n * sizeof(char));
      for (long i = 0; i < n; i++)
	*(_filename + i) = *(filename + i);
    }
  
  /* Ensure that another frame can be held */
  prepare_frame_buffer();
  
  /* Create new frame */
  current_frame = open_frames++;
  void** frame = (void**)(*(frames + current_frame) = malloc(11 * sizeof(void*)));
  *(frame +  0) = 0;                                    /* Point line           */
  *(frame +  1) = 0;                                    /* Point column         */
  *(frame +  2) = 0;                                    /* Mark line            */
  *(frame +  3) = 0;                                    /* Mark column          */
  *(frame +  4) = 0;                                    /* First visible line   */
  *(frame +  5) = 0;                                    /* First visible column */
  *(frame +  6) = (void*)lines;                         /* Number of lines      */
  *(frame +  7) = _filename;                            /* Filename             */
  *(frame +  8) = 0;                                    /* Message              */
  *(frame +  9) = (void*)malloc(lines * sizeof(char*)); /* Buffer               */
  *(frame + 10) = 0;                                    /* Flags                */
  
  if ((buffer))
    {
      /* Populate lines */
      long bufptr = 0;
      for (long i = 0; i < lines; i++)
	{
	  /* Get the span of the line */
	  long start = bufptr;
	  while ((unsigned long)bufptr < size)
	    {
	      if (*(buffer + bufptr) == '\n')
		break;
	      bufptr++;
	    }
	  long linesize = bufptr - start;
	  bufptr = start;
	  
	  /* Create line buffer and fill it with metadata */
	  char* line = *((char**)*(frame + 9) + i) = malloc((2 * P + linesize) * sizeof(char)); /* used:P, allocated:P, line:* */
	  for (int _ = 0; _ < 2; _++)
	    for (int j = P - 1; j >= 0; j--)
	      *line++ = (linesize >> (j * 8)) & 255;
	  
	  /* Fill the line with the data */
	  for (int j = 0; j < linesize; j++)
	    *(line + j) = *(buffer + bufptr++);
	  
	  /* Jump over the \n at the end of the line so the following lines does not appear to be empty */
	  bufptr++;
	}
      free(buffer);
    }
  else
    {
      /* No file was actually openned create an empty document */
      char* line0 = *(char**)*(frame + 9) = malloc(2 * P * sizeof(char)); /* used:P, allocated:P, line:* */
      for (int i = 0; i < 2 * P; i++)
	*(line0 + i) = 0;
    }
  
  /* Report that a new frame as been created */
  return 0;
}


/**
 * Find the frame that contains a specific file
 * 
 * @param   filename  The name of the file
 * @return  >=0       The index of the frame
 * @return  -1        No frame contains the file
 */
long find_file(char* filename)
{
  for (int i = 0; i < open_frames; i++)
    {
      char** frame = (char**)*(frames + i);
      char* frame_file = *(frame + 7);
      if (frame_file)
	{
	  /* Check of the frames file matches the wanted file */
	  char* f = filename;
	  while ((*frame_file && *f) && (*frame_file == *f))
	    {
	      frame_file++;
	      f++;
	    }
	  
	  /* Report index of frame if it was match */
	  if ((*frame_file | *f) == 0)
	    return i;
	}
    }
  
  /* No frame contains the file */
  return -1;
}


/**
 * Adds an alert to the current frame
 * 
 * @param  message  The message
 */
void alert(char* message)
{
  char** frame = (char**)*(frames + current_frame);
  char* current = *(frame + 8);
  if (current)
    free(current);
  *(frame + 8) = message;
}


/**
 * Makes a jump in the current frame
 * 
 * @param  row  The line to jump to, negative to keep the current
 * @parma  col  The column to jump to, negative to keep the current position if row is unchanged and beginning otherwise
 */
void apply_jump(long row, long col)
{
  long* frame = (long*)*(frames + current_frame);
  if (row >= *(frame + 6))
    row = *(frame + 6) - 1;
  char* line = *((char**)*(frame + 9) + (*frame = row));
  long end = 0;
  for (int i = 0; i < P; i++)
    end = (end << 8) | (*(line + i) & 255);
  *(frame + 1) = col <= end ? col : end;
}


/**
 * Gets the current row of the point in the current frame
 * 
 * @return  The current row of the point in the current frame
 */
long get_row()
{
  return *(long*)*(frames + current_frame);
}


/**
 * Gets the current column of the point in the current frame
 * 
 * @return  The current column of the point in the current frame
 */
long get_column()
{
  return *((long*)*(frames + current_frame) + 1);
}


/**
 * Gets the first visible row in the current frame
 * 
 * @return  The first visible row in the current frame
 */
long get_first_row()
{
  return *((long*)*(frames + current_frame) + 4);
}


/**
 * Gets the first visible column on the current row of the point in the current frame
 * 
 * @return  The first visible column on the current row of the point in the current frame
 */
long get_first_column()
{
  return *((long*)*(frames + current_frame) + 5);
}


/**
 * Gets the file of the current frame
 * 
 * @return  The file of the current frame, null if none
 */
char* get_file()
{
  return *((char**)*(frames + current_frame) + 7);
}


/**
 * Gets the alert of the current frame
 * 
 * @return  The alert of the current frame, null if none
 */
char* get_alert()
{
  return *((char**)*(frames + current_frame) + 8);
}


/**
 * Gets the flags for the current frame
 * 
 * @return  The flags for the current frame
 */
int get_flags()
{
  return (int)*((long*)*(frames + current_frame) + 10);
}


/**
 * Gets the number of lines in the current frame
 * 
 * @return  The number of lines in the current frame
 */
long get_line_count()
{
  return *((long*)*(frames + current_frame) + 6);
}


/**
 * Gets the line buffes in the current frame
 * 
 * @return  The line buffes in the current frame
 */
char** get_line_buffers()
{
  return *((char***)*(frames + current_frame) + 9);
}


/**
 * Gets the length of a line
 * 
 * @param   lineBuffer  The line buffer
 * @return              The length of a line
 */
long get_line_lenght(char* line_buffer)
{
  long rc = 0;
  for (int i = 0; i < P; i++)
    rc = (rc << 8) | (*(line_buffer + i) & 255);
  return rc;
}


/**
 * Gets the size of a line buffer
 * 
 * @param   lineBuffer  The line buffer
 * @return              The size of a line buffer
 */
long get_line_buffer_size(char* line_buffer)
{
  return get_line_lenght(line_buffer + P);
}


/**
 * Gets the line content of a line buffer
 * 
 * @param   lineBuffer  The line buffer
 * @return              The line content of a line buffer
 */
char* get_line_content(char* line_buffer)
{
  return line_buffer + 2 * P;
}


/**
 * Free all frame resources
 */
void free_frames()
{
  for (long i = 0; i < open_frames; i++)
    {
      void** frame = (void**)*(frames + i);
      long lines = (long)*(frame + 6);
      if (*(frame + 7))
	free(*(frame + 7));
      if (*(frame + 8))
	free(*(frame + 8));
      if (*(frame + 9))
	{
	  char** line_buffers = (char**)*(frame + 9);
	  for (long j = 0; j < lines; j++)
	    free(*(line_buffers + j));
	  free(*(frame + 9));
	}
      free(frame);
    }
  free(frames);
}

