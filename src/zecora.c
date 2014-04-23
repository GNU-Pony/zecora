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
extern frame_t* cur_frame;



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
  pos_t rows, cols;
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
      rows = (pos_t)(win.ws_row);
      cols = (pos_t)(win.ws_col);
      
      /* Check the size of the terminal */
      if ((rows < MINIMUM_ROWS) || (cols < MINIMUM_COLS))
	{
	  fprintf(stderr, "You will need at least a %i rows by %i columns terminal!\n",
		  MINIMUM_ROWS, MINIMUM_COLS);
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
      /* Parent should wait for the fork to ensure that the terminal is properly restored */
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
static void jump(const char* command)
{
  byte_t has = 0, state = 1;
  pos_t row = 0, col = 0;
  char c;
  char* msg;
  const char* msg_;
  
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


static void create_screen(pos_t rows, pos_t cols)
{
  char* spaces;
  pos_t i;
  char* filename;
  
  /* Create a line of spaces as large as the screen */
  spaces = malloc((size_t)(cols + 1) * sizeof(char));
  for (i = 0; i < cols; i++)
    *(spaces + i) = ' ';
  *(spaces + cols) = 0;
  
  /* Ensure that the point is visible */
  pos_t point_row = cur_frame->row;
  pos_t point_col = cur_frame->column;
  pos_t point_cols = cur_frame->line_buffers[point_row].used;
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
	 "\033[%li;1H\033[07m"
	 "%s"
	 "\033[%li;3H(%li,%li)  ",
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
  pos_t r = cur_frame->row;
  pos_t n = cur_frame->line_count, m = cur_frame->first_row + rows - 3;
  line_buffer_t* lines = cur_frame->line_buffers;
  n = n < m ? n : m;
  cols--;
  static char ucs_decode_buffer[8];
  *(ucs_decode_buffer + 7) = 0;
  for (i = cur_frame->first_row; i < n; i++)
    {
      m = (lines + i)->used;
      pos_t j = i == r ? cur_frame->first_column : 0;
      m = m < (cols + j) ? m : (cols + j);
      char_t* line = (lines + i)->line;
      /* TODO add support for combining diacriticals */
      pos_t col = 0;
      int is_comment = 0;
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
	    printf(is_comment ? "\033[41m.\033[00;31m" : "\033[41m.\033[00m");
	  else if ((0 <= c) && (c < (char_t)' '))
	    printf(is_comment ? "\033[01m%c\033[21m" : "\033[31m%c\033[00m", '@' + c);
	  else if (c == 0x2011) /* non-breaking hyphen */
	    printf(is_comment ? "\033[35m-\033[00m" : "\033[35m-\033[00m");
	  else if (c == 0x2010) /* hyphen */
	    printf(is_comment ? "\033[34m-\033[00m" : "\033[34m-\033[00m");
	  else if (c == 0x00A0) /* no-breaking space */
	    printf(is_comment ? "\033[45m \033[00;31m" : "\033[45m \033[00m");
	  else if (c == 0x00AD) /* soft hyphen */
	    printf(is_comment ? "\033[01m-\033[21m" : "\033[31m-\033[00m");
	  else if (c < 0x80)
	    {
	      if (c == '#')
		{
		  is_comment = 1;
		  printf("\033[31m");
		}
	      printf("%c", c);
	    }
	  else
	    {
	      long off = 7;
	      *ucs_decode_buffer = (int8_t)0x80;
	      if (c < 0)
		abort();
	      while (c)
		{
		  *(ucs_decode_buffer + --off) = (char)((c & 0x3F) | 0x80);
		  *ucs_decode_buffer |= (*ucs_decode_buffer) >> 1;
		  c >>= 6;
		}
	      if ((*ucs_decode_buffer) & (*(ucs_decode_buffer + off) & 0x3F))
		*(ucs_decode_buffer + --off) = (char)((*ucs_decode_buffer) << 1);
	      else
		*(ucs_decode_buffer + off) |= (char)((*ucs_decode_buffer) << 1);
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
  
  free(spaces);
}


static void read_input(pos_t cols)
{
#define CRTL(KEY)  (KEY - '@')
  
  int ctrl_x = 0;
  int meta = 0;
  ssize_t escape = -1;
  char escape_buffer[16];
  
  for (;;)
    {
      int c = getchar();
      if (escape >= 0)
	{
	  if (escape == sizeof(escape_buffer) / sizeof(char))
	    /* Too long, stop. */
	    escape = -1;
	  else
	    {
	      if ((('0' <= c) && (c <= '9')) || (c == ';'))
		escape_buffer[escape++] = c;
	      else
		{
		  switch (c)
		    {
		    case 'A':
		      /* up */
		      break;
		      
		    case 'B':
		      /* down */
		      break;
		      
		    case 'C':
		      /* right */
		      break;
		      
		    case 'D':
		      /* left */
		      break;
		      
		    case '~':
		      /* ... */
		      break;
		      
		    default:
		      /* not recognised */
		      break;
		    }
		  escape = -1;
		}
	      continue;
	    }
	}
      if (escape == -2) /* ESC O */
	{
	  switch (c)
	    {
	    case 'H':
	      /* home */
	      break;
	      
	    case 'F':
	      /* end */
	      break;
	      
	    default:
	      /* not recognised */
	      break;
	    }
	  escape = -1;
	}
      else if (c == '\033')
	{
	  meta += 1;
	  if (meta == 3)
	    {
	      meta = 0;
	      /* help */
	    }
	}
      else if (meta)
	{
	  if (ctrl_x == 0)
	    switch (c)
	      {
	      case 'O':
		escape = -2;
		break;
		
	      case 'g':
		/* jump */
		break;
		
	      case 'i':
		/* tab */
		break;
		
	      case 'l':
		/* lower case */
		break;
		
	      case 'u':
		/* upper case */
		break;
		
	      case 'v':
		/* page up */
		break;
		
	      case 'w':
		/* copy */
		break;
		
	      case 'y':
		/* cycle paste */
		break;
		
	      case '[':
		escape = 0;
		break;
		
	      default:
		/* not recognised */
		break;
	      }
	  ctrl_x = 0;
	  meta = 0;
	}
      else if (ctrl_x == 0)
	{
	  switch (c)
	    {
	    case CTRL('@'):
	      /* set mark */
	      break;
	      
	    case CTRL('A'):
	      /* home */
	      break;
	      
	    case CTRL('B'):
	      /* backwards */
	      break;
	      
	    case CTRL('D'):
	      /* delete */
	      break;
	      
	    case CTRL('E'):
	      /* end */
	      break;
	      
	    case CTRL('F'):
	      /* forward */
	      break;
	      
	    case CTRL('G'):
	      /* quit action */
	      break;
	    
	    case CTRL('K'):
	      /* kill */
	      break;
	      
	    case CTRL('L'):
	      /* recenter */
	      break;
	      
	    case CTRL('M'):
	      /* new line */
	      break;
	      
	    case CTRL('N'):
	      /* next line */
	      break;
	      
	    case CTRL('O'):
	      /* insert new line */
	      break;
	      
	    case CTRL('P'):
	      /* previous line */
	      break;
	      
	    case CTRL('Q'):
	      /* verbatim input */
	      break;
	    
	    case CTRL('R'):
	      /* search backwards */
	      break;
	      
	    case CTRL('S'):
	      /* search */
	      break;
	      
	    case CTRL('T'):
	      /* transpose */
	      break;
	      
	    case CTRL('V'):
	      /* page down */
	      break;
	      
	    case CTRL('W'):
	      /* cut */
	      break;
	      
	    case CTRL('X'):
	      ctrl_x = 1;
	      break;
	      
	    case CTRL('Y'):
	      /* paste */
	      break;
	      
	    case CTRL('_'):
	      /* undo */
	      break;
	      
	    case '\t':
	      /* tab */
	      break;
	      
	    case '\n':
	      /* new line */
	      break;
	      
	    case 127:
	    case CTRL('H'):
	      /* erase */
	      break;
	      
	    default:
	      /* charcter? */
	      break;
	    }
	}
      else
	{
	  switch (c)
	    {
	    case CTRL('C'):
	      /* exit */
	      return;
	      
	    case CTRL('F'):
	      /* find file */
	      break;
	      
	    case CTRL('I'):
	      /* indent */
	      break;
	      
	    case CTRL('K'):
	      /* kill buffer */
	      break;
	      
	    case CTRL('Q'):
	      /* toggle read-only mode */
	      break;
	      
	    case CTRL('R'):
	      /* find file, read-only */
	      break;
	      
	    case CTRL('S'):
	      /* save */
	      break;
	      
	    case CTRL('W'):
	      /* save as */
	      break;
	      
	    case CTRL('X'):
	      /* swap mark */
	      break;
	      
	    case CTRL('_'):
	      /* switch undo direction */
	      break;
	      
	    case 'k':
	      /* kill buffer */
	      break;
	      
	    case 'o':
	      /* next buffer */
	      break;
	      
	    case 's':
	      /* save all, ask */
	      break;
	      
	    default:
	      /* not recognised */
	      break;
	    }
	  ctrl_x = 0;
	}
    }
  
#undef CRTL
}

