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
#ifndef __FRAMES_H__
#define __FRAMES_H__


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

#include "types.h"



/**
 * The file has been modified but not saved
 */
#define  FLAG_MODIFIED  1

/**
 * The mark has been set
 */
#define  FLAG_MARK_SET  2

/**
 * The mark is active
 */
#define  FLAG_MARK_ACTIVE  4



/**
 * Frame line buffer information structure
 */
typedef struct line_buffer
{
  /**
   * The number of used characters in the line
   */
  pos_t used;
  
  /**
   * The number of allocated characters for the line
   */
  pos_t allocated;
  
  /**
   * The content of the line
   */
  char_t* line;
  
} line_buffer_t;


/**
 * Frame information structure
 */
typedef struct frame
{
  /**
   * The current row of the point in the frame
   */
  pos_t row;
  
  /**
   * The current column of the point in the frame, may exceed the number of columns in the active line
   */
  pos_t column;
  
  /**
   * The current row of the mark in the frame
   */
  pos_t mark_row;
  
  /**
   * The current column of the mark in the frame
   */
  pos_t mark_column;
  
  /**
   * The first visible row in the frame
   */
  pos_t first_row;
  
  /**
   * The first visible column on the current row of the point in the frame
   */
  pos_t first_column;
  
  /**
   * The flags for the frame
   */
  int_least8_t flags;
  
  /**
   * The file of the frame, `NULL` if none
   */
  char* file;
  
  /**
   * The alert of the frame, `NULL` if none
   */
  char* alert;
  
  /**
   * The number of lines in the frame
   */
  pos_t line_count;
  
  /**
   * The line buffes in the frame
   */
  line_buffer_t* line_buffers;
  
} frame_t;



/**
 * Prepare the frame buffer to hold one more frame
 */
void prepare_frame_buffer(void);


/**
 * Create an empty document that is not yet associated with a file
 */
void create_scratch(void);


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
long open_file(char* filename);


/**
 * Find the frame that contains a specific file
 * 
 * @param   filename  The name of the file
 * @return  >=0       The index of the frame
 * @return  -1        No frame contains the file
 */
pos_t find_file(char* filename) __attribute__((pure));


/**
 * Adds an alert to the current frame
 * 
 * @param  message  The message
 */
void alert(char* message);


/**
 * Makes a jump in the current frame
 * 
 * @param  row  The line to jump to, negative to keep the current
 * @parma  col  The column to jump to, negative to keep the current position if row is unchanged and beginning otherwise
 */
void apply_jump(pos_t row, pos_t col);


/**
 * Free all frame resources
 */
void free_frames(void);


#endif

