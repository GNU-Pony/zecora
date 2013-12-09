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
  struct winsize win;
  dimm_t rows, cols;
  struct termios saved_stty;
  struct termios stty;
  bool_t file_loaded;
  int i;
  
  /* Determine the size of the terminal */
  ioctl(STDOUT_FILENO, TIOCGWINSZ, (char*)&win);
  rows = (dimm_t)(win.ws_row);
  cols = (dimm_t)(win.ws_col);
  
  /* Check the size of the terminal */
  if ((rows < MINIMUM_ROWS) || (cols < MINIMUM_COLS))
    {
      fprintf(stderr, "You will need at least a %i rows by %i columns terminal!\n", MINIMUM_ROWS, MINIMUM_COLS);
      return 1;
    }
  
  /* Disable signals from keystrokes, keystroke echoing and keystroke buffering */
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
  
  printf("\033[?0c"      /* Restore cursor to default, if using TTY */
	 "\033[H\033[2J" /* Clear the terminal, useless if in subterminal and not TTY */
	 "\033[?1049l"); /* Terminate subterminal, if using an xterm */
  /* Apply the previous instruction */
  fflush(stdout);
  /* Return the terminal to its previous state */
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_stty);
  
  /* Release resources, exit and report success */
  free_frames();
  return 0;
}


/**
 * Make a jump in the current frame
 * 
 * @param  command  The jump command, in the format `[%row][:%column]`
 */
void jump(char* command)
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
      row = (has & 1) ? -row : -1;
      col = (has & 2) ? -col : -1;
      apply_jump(row, col);
    }
}


void create_screen(dimm_t rows, dimm_t cols)
{
  char* spaces;
  dimm_t i;
  char* filename;
  char* frame_alert;
  
  /* Create a line of spaces as large as the screen */
  spaces = malloc((cols + 1) * sizeof(char));
  for (i = 0; i < cols; i++)
    *(spaces + i) = ' ';
  *(spaces + cols) = 0;
  
  /* Create the screen with the current frame, but do not fill it */
  printf("\033[07m"
	 "%s"
	 "\033[1;1H\033[01mZecora  \033[21mPress ESC three times for help"
	 "\033[27m"
	 "\033[%i;1H\033[07m"
	 "%s"
	 "\033[%i;3H(%li,%li)  ",
	 spaces, rows - 1, spaces, rows - 1, get_row() + 1, get_column() + 1);
  
  if (get_flags() & FLAG_MODIFIED)
    printf("\033[41m");
  filename = get_file();
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
  if ((frame_alert = get_alert()))
    printf("%s", frame_alert);
  printf("\033[00m\033[2;1H");
  
  /* Fill the screen */
  long r = get_row();
  long n = get_line_count(), m = get_first_row() + rows - 3;
  char** lines = get_line_buffers();
  n = n < m ? n : m;
  cols--;
  for (long i = get_first_row(); i < n; i++)
    {
      m = get_line_lenght(*(lines + i));
      long j = i == r ? get_first_column() : 0;
      m = m < (cols + j) ? m : (cols + j);
      char* line = get_line_content(*(lines + i));
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
  printf("\033[%li;%liH", get_first_row() - get_row() + 2, get_first_column() - get_column() + 1);
  
  /* Flush the screen */
  fflush(stdout);
}


void read_input(dimm_t cols)
{
  getchar();
}

