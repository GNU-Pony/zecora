/**
 * Zecora – An minimal Emacs-clone intended for use in emergencies
 * 
 * Copyright © 2013, 2014  Mattias Andrée (maandree@member.fsf.org)
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
 * The number of opened frames
 */
static pos_t open_frames = 0;

/**
 * The number of frames that fits in `frames`
 */
static pos_t prepared_frames = 0;

/**
 * The index of the current frame
 */
static pos_t current_frame = -1;

/**
 * The opened frames
 */
static struct frame* frames = NULL;

/**
 * The currently active frame
 */
struct frame* cur_frame = NULL;



/**
 * Prepare the frame buffer to hold one more frame
 */
void prepare_frame_buffer(void)
{
  if (frames == NULL)
  {
    /* Prepare to hold 4 frames initially */
    frames = malloc((size_t)(prepared_frames = 4) * sizeof(struct frame));
  }
  else if (open_frames == prepared_frames)
  {
    /* When full, prepare to hold twice as much */
    frames = realloc(frames, (size_t)(prepared_frames <<= 1) * sizeof(struct frame));
    /* Revalidate the value of `cur_frame` as it would have been incorrect if `realloc` moves `frames` */
    cur_frame = frames + current_frame;
  }
}


/**
 * Create an empty document that is not yet associated with a file
 */
void create_scratch(void)
{
  /* Ensure that another frame can be held */
  prepare_frame_buffer();
  
  /* Create new frame */
  current_frame = open_frames++;
  cur_frame = frames + current_frame;
  
  /* Initialise frame */
  cur_frame->row = 0;
  cur_frame->column = 0;
  cur_frame->mark_row = 0;
  cur_frame->mark_column = 0;
  cur_frame->first_row = 0;
  cur_frame->first_column = 0;
  cur_frame->flags = 0;
  cur_frame->file = NULL;
  cur_frame->alert = NULL;
  cur_frame->line_count = 1;
  cur_frame->line_buffers = malloc(sizeof(struct line_buffer));
  
  /* Create one empty line */
  cur_frame->line_buffers->used = 0;
  cur_frame->line_buffers->allocated = 16;
  cur_frame->line_buffers->line = malloc(16 * sizeof(char_t));
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
  pos_t found = find_file(filename);
  if (found >= 0)
    return ~found;
  
  /* Verify that the file is a regular file or does not exist but can be created */
  int file_exists = 1;
  struct stat file_stats;
  if (stat(filename, &file_stats))
    {
      int error = errno;
      if ((error == ENOENT) && *filename)
	{
	  size_t namesize = 0;
	  while (*(filename + namesize++))
	    ;
	  char* dirname = malloc(namesize * sizeof(char));
	  for (size_t i = 0; i < namesize; i++)
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
      if (error)
	return error;
    }
  else if (S_ISREG(file_stats.st_mode) == 0)
    return 256;
  
  /* Get the optimal reading block size */
  size_t block_size = file_exists ? (size_t)(file_stats.st_blksize) : 0;
  /* Get the size of the file */
  size_t size = 0;
  size_t reported_size = (size_t)(file_stats.st_size);
  /* Buffer for the content file */
  int8_t* buffer = file_exists ? malloc(reported_size * sizeof(int8_t)) : NULL;
  
  /* Read file */
  size_t got;
  FILE* file = file_exists ? fopen(filename, "r") : NULL;
  if (file_exists)
    {
      for (;;)
	{
	  size_t read_block = reported_size - size;
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
  pos_t lines = 1;
  if (buffer)
    for (size_t i = 0; i < size; i++)
      if (*(buffer + i) == '\n')
	lines++;
  
  /* Copy filename so it later can be freed as well as get the real path */
  char* _filename = 0;
  if (buffer)
    {
      _filename = malloc(PATH_MAX * sizeof(char));
      if (realpath(filename, _filename) == NULL)
	return errno;
    }
  else
    {
      /* TODO get the realpath for the directroy */
      size_t n = 0;
      while (*(filename + n++))
	;
      _filename = malloc(n * sizeof(char));
      for (size_t i = 0; i < n; i++)
	*(_filename + i) = *(filename + i);
    }
  
  /* Ensure that another frame can be held */
  prepare_frame_buffer();
  
  /* Create new frame */
  current_frame = open_frames++;
  cur_frame = frames + current_frame;
  cur_frame->row = 0;
  cur_frame->column = 0;
  cur_frame->mark_row = 0;
  cur_frame->mark_column = 0;
  cur_frame->first_row = 0;
  cur_frame->first_column = 0;
  cur_frame->flags = 0;
  cur_frame->file = _filename;
  cur_frame->alert = NULL;
  cur_frame->line_count = lines;
  cur_frame->line_buffers = malloc((size_t)lines * sizeof(struct line_buffer));
  for (pos_t i = 0; i < lines; i++)
    {
      struct line_buffer* lbuf = cur_frame->line_buffers + i;
      lbuf->used = 0;
      lbuf->allocated = 4;
      lbuf->line = malloc(4 * sizeof(char_t));
    }
  
  if (buffer)
    {
      /* Populate lines */
      size_t bufptr = 0;
      for (pos_t i = 0; i < lines; i++)
	{
	  /* Get the span of the line */
	  size_t start = bufptr;
	  pos_t chars = 0;
	  while (bufptr < size)
	    if (*(buffer + bufptr) == '\n')
	      break;
	    else
	      if ((*(buffer + bufptr++) & 0xC0) != 0x80)
		chars++;
	  pos_t linesize = (pos_t)(bufptr - start);
	  bufptr = start;
	  
	  /* Create line buffer and fill it with metadata */
	  struct line_buffer* lbuf = cur_frame->line_buffers + i;
	  lbuf->used = chars;
	  if (lbuf->allocated < lbuf->used)
	    {
	      lbuf->allocated = lbuf->used;
	      lbuf->line = realloc(lbuf->line, (size_t)(lbuf->allocated) * sizeof(char_t));
	    }
	  
	  /* Fill the line with the data */
	  for (pos_t j = 0, k = -1; j < linesize; j++)
	    {
	      int8_t c = *(buffer + bufptr++);
	      if ((c & 0xC0) != 0x80)
		{
		  int8_t n = 0;
		  while (c & 0x80)
		    {
		      n++;
		      c <<= 1;
		    }
		  *(lbuf->line + ++k) = c >> n;
		}
	      else
		{
		  if (k < 0)
		    k = 0;
		  *(lbuf->line + k) <<= 6;
		  *(lbuf->line + k) |= c & 0x4F;
		}
	    }
	  
	  /* Jump over the \n at the end of the line so the following lines does not appear to be empty */
	  bufptr++;
	}
      free(buffer);
    }
  else
    {
      /* No file was actually opened, an empty document has been created, keep it */
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
pos_t find_file(char* filename)
{
  char* f;
  char* ff;
  for (pos_t i = 0; i < open_frames; i++)
    if ((ff = (frames + i)->file))
      {
	/* Check of the frames file matches the wanted file */
	f = filename;
	while ((*ff && *f) && (*ff == *f))
	  {
	    ff++;
	    f++;
	  }
	
	/* Report index of frame if it was match */
	if ((*ff | *f) == 0)
	  return (pos_t)i;
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
  if (cur_frame->alert)
    free(cur_frame->alert);
  cur_frame->alert = message;
}


/**
 * Makes a jump in the current frame
 * 
 * @param  row  The line to jump to, negative to keep the current
 * @parma  col  The column to jump to, negative to keep the current position if row is unchanged and beginning otherwise
 */
void apply_jump(pos_t row, pos_t col)
{
  if (row >= 0)
    {
      if (row >= cur_frame->line_count)
	row = cur_frame->line_count - 1;
      cur_frame->row = row;
    }
  if (col >= 0)
    cur_frame->column = col;
}


/**
 * Free all frame resources
 */
void free_frames(void)
{
  pos_t i, j, n;
  char* buf;
  
#define _p_(object)  ((long)(void*)(object))
  
  for (i = 0; i < open_frames; i++)
    {
      if ((buf = (frames + i)->file))
	free(buf);
      
      if ((buf = (frames + i)->alert))
	free(buf);
      
      n = (frames + i)->line_count;
      for (j = 0; j < n; j++)
	free((frames + i)->line_buffers[j].line);
      free((frames + i)->line_buffers);
    }
  free(frames);

#undef _p_
}

