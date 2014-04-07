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
#include "zecora.h"


/**
 * The currently active frame
 */
extern struct frame* cur_frame;



/**
 * This is the mane entry point of Zecora
 * 
 * @param   argc  The number of elements in `argv`
 * @param   argv  The command line arguments
 * @return        Exit value, 0 on success
 */
int main(int argc, char** argv)
{
  struct winsize win;
  size_t rows, cols;
  struct termios saved_stty;
  struct termios stty;
  bool_t file_loaded;
  pid_t pid;
  int i;
  int status;
  
#ifdef DEBUG
  rows = MINIMUM_ROWS;
  cols = MINIMUM_COLS;
  if (ttyname(STDOUT_FILENO) != NULL)
    {
#endif
      /* Determine the size of the terminal */
      ioctl(STDOUT_FILENO, TIOCGWINSZ, (char*)&win);
      rows = (size_t)(win.ws_row);
      cols = (size_t)(win.ws_col);
      
      /* Check the size of the terminal */
      if ((rows < MINIMUM_ROWS) || (cols < MINIMUM_COLS))
	{
	  fprintf(stderr, "You will need at least a %i rows by %i columns terminal!\n", MINIMUM_ROWS, MINIMUM_COLS);
	  return 1;
	}
#ifdef DEBUG
    }
#endif
  
  /* Disable signals from keystrokes, keystroke echoing and keystroke buffering */
  tcgetattr(STDIN_FILENO, &saved_stty);
  tcgetattr(STDIN_FILENO, &stty);
  stty.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &stty);
  
  printf("\033[?1049h"   /* Initialise subterminal, if using an xterm */
	 "\033[H\033[2J" /* Clear the terminal, subterminal, if initialised is already clean */
	 "\033[?8c");    /* Switch to block cursor, if using TTY */
  /* Apply the previous instruction */
  fflush(stdout);
  
  pid = xfork();
  if (pid && (pid != (pid_t)-1))
    {
      /* Parent should waid for fork to ensure that the terminal is properly restored */
      (void)waitpid(pid, &status, WUNTRACED);
    }
  else
    {
      /* Create an empty buffer not yet associeted with a file */
      create_scratch();
      
      /* Load files and apply jumps from the command line */
      file_loaded = 0;
      for (i = 1; i < argc; i++)
	if (**(argv + i) != ':')
	  {
	    /* Load file */
	    file_loaded = open_file(*(argv + i)) == 0;
	  }
	else if (file_loaded)
	  {
	    /* Jump in last opened file */
	    jump(*(argv + i) + 1);
	    file_loaded = 0;
	  }
      
      /* Create the screen and start display the files */
      create_screen(rows, cols);
      /* Start interaction */
      read_input(cols);
      
      /* Release resources */
      free_frames();
      
      /* Do not continue beyond this point if we managed to fork */
      if (pid == 0)
	return 0;
    }
  
  printf("\033[?0c"      /* Restore cursor to default, if using TTY */
	 "\033[H\033[2J" /* Clear the terminal, useless if in subterminal and not TTY */
	 "\033[?1049l"); /* Terminate subterminal, if using an xterm */
  /* Apply the previous instruction */
  fflush(stdout);
  /* Return the terminal to its previous state */
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_stty);
  
  /* Exit and report successfulness */
  if (pid && (pid != (pid_t)-1))
    {
      if (WIFSIGNALED(status))
	kill(getpid(), WTERMSIG(status));
      else
	return WEXITSTATUS(status);
      return 1;
    }
  return 0;
}


/**
 * Make a jump in the current frame
 * 
 * @param  command  The jump command, in the format `[%row][:%column]`
 */
static void jump(char* command)
{
  byte_t has = 0, state = 1;
  pos_t row = 0, col = 0;
  char c;
  char* msg;
  char* msg_;
  
  /* Parse command */
  while ((c = *command++))
    switch (c)
      {
      case '0' ... '9':
	has |= state;
	if (state == 1)
	  row = row * 10 - (c & 15);
	else
	  col = col * 10 - (c & 15);
	break;
	
      case ':':
	if ((++state == 3))
	  goto jump_parse_done;
	break;
	
      default:
	state = 3;
	goto jump_parse_done;
      }
 jump_parse_done:
  
  if (state == 3)
    {
      /* Invalid format */
      msg = malloc(28 * sizeof(char*));
      msg_ = "\033[31mInvalid jump format\033[m";
      alert(msg);
      while ((*msg++ = *msg_++))
	;
    }
  else
    {
      /* Apply jump */
      row = (has & 1) ? -(row + 1) : -1;
      col = (has & 2) ? -(col + 1) : -1;
      apply_jump(row, col);
    }
}


static void create_screen(size_t rows, size_t cols)
{
  char* spaces;
  size_t i;
  char* filename;
  
  /* Create a line of spaces as large as the screen */
  spaces = alloca((cols + 1) * sizeof(char));
  for (i = 0; i < cols; i++)
    *(spaces + i) = ' ';
  *(spaces + cols) = 0;
  
  /* Ensure that the point is visible */
  pos_t point_row = cur_frame->row;
  pos_t point_col = cur_frame->column;
  size_t point_cols = cur_frame->line_buffers[point_row].used;
  if (point_col > (pos_t)point_cols)
    point_col = (pos_t)point_cols;
  if (point_row < cur_frame->first_row)
    cur_frame->first_row = point_row;
  else if (point_row >= cur_frame->first_row + rows - 3)
    cur_frame->first_row = point_row;
  if (point_col < cur_frame->first_column)
    cur_frame->first_column = point_col;
  else if (point_col >= cur_frame->first_column + cols)
    cur_frame->first_column = point_col;
  
  /* Create the screen with the current frame, but do not fill it */
  printf("\033[07m"
	 "%s"
	 "\033[1;1H\033[01mZecora  \033[21mPress ESC three times for help"
	 "\033[27m"
	 "\033[%i;1H\033[07m"
	 "%s"
	 "\033[%i;3H(%li,%li)  ",
	 spaces, rows - 1, spaces, rows - 1, cur_frame->row + 1, point_col + 1);
  
  if (cur_frame->flags & FLAG_MODIFIED)
    printf("\033[41m");
  filename = cur_frame->file;
  if (filename)
    {
      long sep = 0, j;
      for (j = 0; *(filename + j); j++)
	if (*(filename + j) == '/')
	  sep = j;
      *(filename + sep) = 0;
      printf("%s/\033[01m%s\033[21;27m\n", filename, filename + sep + 1);
      *(filename + sep) = '/';
    }
  else
    printf("\033[01m*scratch*\033[21;27m\n");
  if (cur_frame->alert)
    printf("%s", cur_frame->alert);
  printf("\033[00m\033[2;1H");
  
  /* Fill the screen */
  size_t r = cur_frame->row;
  size_t n = cur_frame->line_count, m = cur_frame->first_row + rows - 3;
  struct line_buffer* lines = cur_frame->line_buffers;
  n = n < m ? n : m;
  cols--;
  static char ucs_decode_buffer[8];
  *(ucs_decode_buffer + 7) = 0;
  for (i = cur_frame->first_row; i < n; i++)
    {
      m = (lines + i)->used;
      size_t j = i == r ? cur_frame->first_column : 0;
      m = m < (cols + j) ? m : (cols + j);
      char_t* line = (lines + i)->line;
      /* TODO add support for combining diacriticals */
      /* TODO colour comment lines */
      long col = 0;
      for (; (j < m) && (col < cols); j++)
	{
	  char_t c = *(line + j);
	  if (c == (char_t)'\t')
	    {
	      printf(" ");
	      col++;
	      while ((col < cols) && (col & 7))
		{
		  printf(" ");
		  col++;
		}
	      continue;
	    }
	  col++;
	  if ((c & 0x7FFFFFFF) != c) /* should never happend: invalid ucs value */
	    printf("\033[41m.\033[00m");
	  else if ((0 <= c) && (c < (char_t)' '))
	    printf("\033[31m%c\033[00m", '@' + c);
	  else if (c == 0x2011) /* non-breaking hyphen */
	    printf("\033[35m-\033[00m");
	  else if (c == 0x2010) /* hyphen */
	    printf("\033[34m-\033[00m");
	  else if (c == 0x00A0) /* no-breaking space */
	    printf("\033[45m \033[00m");
	  else if (c == 0x00AD) /* soft hyphen */
	    printf("\033[31m-\033[00m");
	  else if (c < 0x80)
	    printf("%c", c);
	  else
	    {
	      long off = 7;
	      *ucs_decode_buffer = (int8_t)0x80;
	      while (c)
		{
		  *(ucs_decode_buffer + --off) = (c & 0x4F) | 0x80;
		  *ucs_decode_buffer |= (*ucs_decode_buffer) >> 1;
		  c >>= 6;
		}
	      if ((*ucs_decode_buffer) & *(ucs_decode_buffer + off))
		*(ucs_decode_buffer + --off) = (*ucs_decode_buffer) << 1;
	      else
		*(ucs_decode_buffer + off) |= (*ucs_decode_buffer) << 1;
	      printf("%s", ucs_decode_buffer + off);
	    }
	}
      printf("\033[00m\n");
    }
  cols++;
  
  /* Move the cursor to the position of the point */
  printf("\033[%li;%liH", point_row - cur_frame->first_row + 2, point_col - cur_frame->first_column + 1);
  
  /* Flush the screen */
  fflush(stdout);
}


static void read_input(size_t cols)
{
  getchar();
}

