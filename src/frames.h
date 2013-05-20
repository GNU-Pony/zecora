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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>



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
 * Prepare the frame buffer to hold one more frame
 */
void prepareFrameBuffer();


/**
 * Create an empty document that is not yet associated with a file
 */
void createScratch();


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
long openFile(char* filename);


/**
 * Find the frame that contains a specific file
 * 
 * @param   filename  The name of the file
 * @return  >=0       The index of the frame
 * @return  -1        No frame contains the file
 */
long findFile(char* filename);


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
void applyJump(long row, long col);


/**
 * Gets the current row of the point in the current frame
 * 
 * @return  The current row of the point in the current frame
 */
long getRow();


/**
 * Gets the current column of the point in the current frame
 * 
 * @return  The current column of the point in the current frame
 */
long getColumn();


/**
 * Gets the first visible row in the current frame
 * 
 * @return  The first visible row in the current frame
 */
long getFirstRow();


/**
 * Gets the first visible column on the current row of the point in the current frame
 * 
 * @return  The first visible column on the current row of the point in the current frame
 */
long getFirstColumn();


/**
 * Gets the file of the current frame
 * 
 * @return  The file of the current frame, null if none
 */
char* getFile();


/**
 * Gets the alert of the current frame
 * 
 * @return  The alert of the current frame, null if none
 */
char* getAlert();


/**
 * Get the flags for the current frame
 * 
 * @return  The flags for the current frame
 */
int getFlags();

