/*
MIT License

Copyright (c) 2019 Andre Seidelt <superilu@yahoo.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <bios.h>
#include <conio.h>
#include <keys.h>
#include <pc.h>

#include "DOjS.h"
#include "dialog.h"
#include "edit.h"
#include "lines.h"
#include "util.h"

/************
** defines **
************/
#define EDI_TEMPLATE BOOT_DIR "template.txt"  //!< filename for template file
#define EDI_HELPFILE BOOT_DIR "help.txt"      //!< filename for help file

#define EDI_CNP_SIZE 4096  //!< initial size of Copy&Paste buffer

/***********************
** external variables **
***********************/
extern syntax_t edi_wordlist[];

/************************
** function prototypes **
************************/
static char* edi_load(edi_t* edi, char* fname);
static char* edi_save(edi_t* edi, char* fname);
static bool edi_do_up(edi_t* edi);
static bool edi_do_down(edi_t* edi);
static void edi_do_left(edi_t* edi);
static void edi_do_right(edi_t* edi);
static void edi_do_bs(edi_t* edi);
static void edi_do_del(edi_t* edi);
static void edi_do_enter(edi_t* edi, bool autoindent);
static void edi_do_wordright(edi_t* edi);
static void edi_do_wordleft(edi_t* edi);
static void edi_do_tab(edi_t* edi);
static void edi_do_save(edi_t* edi);
static void edi_do_goto_line(edi_t* edi);
static bool edi_cmp(char* find, char* txt, int remainder);
static void edi_do_find(edi_t* edi, char find_buffer[80]);
static void edi_do_start(edi_t* edi);
static void edi_do_end(edi_t* edi);
static void edi_do_pageup(edi_t* edi);
static void edi_do_pagedown(edi_t* edi);
static void edi_do_insert_char(edi_t* edi, char ch);
static void edi_clear_cnp(edi_t* edi);
static void edi_copy_line(edi_t* edi, line_t* l, int start, int end, bool newline);
static void edi_do_copy(edi_t* edi);
static void edi_do_cut(edi_t* edi);
static void edi_do_paste(edi_t* edi);
static void edi_do_del_sel(edi_t* edi);
static edi_exit_t edi_loop(edi_t* edi);
static bool edi_colcmp(syntax_t* sy, char* txt, int remainder);
static int edi_syntax(line_t* l, int pos);
static void edi_get_cnp(edi_t* edi, cnp_t* cnp);
static void edi_sel_color(edi_t* edi, int x, int y);
static void edi_draw_line(edi_t* edi, line_t** l, int y, int offset, int line_num);
static void edi_redraw(edi_t* edi);
static void edi_draw_status(edi_t* edi);
static void edi_draw_commands(edi_t* edi);
static edi_t* edi_init(char* fname);
static void edi_shutdown(edi_t* edi);
static void edi_start_selection(edi_t* edi);
static void edi_goto_line(edi_t* edi, line_t* l, int line_num);
static void edi_do_del_line(edi_t* edi);
static void edi_do_backtab(edi_t* edi);

#define EDI_NUM_COMMANDS 10  //!< number of commands

//! array with command texts
char* edi_f_keys[] = {"Help", "", "Save", "Run", "", "", "Find", "", "Log", "Exit"};

/*********************
** static functions **
*********************/
/**
 * @brief load file into edi at current line
 *
 * @param edi the edi system to work on.
 * @param fname name of the file to load.
 *
 * @return an error message or NULL if all went well.
 */
static char* edi_load(edi_t* edi, char* fname) {
    assert(edi);
    assert(fname);

    FILE* f = fopen(fname, "r");
    if (!f) {
        return strerror(errno);
    }

    int ch;
    while ((ch = getc(f)) != EOF) {
        if (ch == '\n') {
            // new line
            edi->current->newline = true;
            line_t* l = lin_newline();
            if (l) {
                lin_insertline(edi, edi->current, l);  // append a new line
                edi->current = l;
            } else {
                fclose(f);
                return "Out of memory!";
            }
        } else if (ch == '\t') {
            lin_appendch(edi, edi->current, ' ');
            while (edi->current->length % EDI_TABSIZE != 0) {
                lin_appendch(edi, edi->current, ' ');  // fill with spaces 'till next tabstop
            }
        } else if (ch != '\r') {
            lin_appendch(edi, edi->current, ch);
        }
    }

    fclose(f);
    edi->top = edi->current = edi->first;
    edi->x = 0;
    edi->y = 0;
    edi->changed = false;
    edi->name = fname;
    return NULL;
}

/**
 * @brief save all lines from edi to file
 *
 * @param edi the edi system to work on.
 * @param fname name to use for saving.
 *
 * @return an error message or NULL if all went well.
 */
static char* edi_save(edi_t* edi, char* fname) {
    FILE* f = fopen(fname, "w");
    if (!f) {
        return strerror(errno);
    }

    line_t* l = edi->first;
    while (l) {
        fwrite(l->txt, l->length, 1, f);
        if (l->newline) {
            fputc('\n', f);
        }
        l = l->next;
    }

    fclose(f);
    edi->changed = false;

    return NULL;
}

/**
 * @brief editor command: do UP.
 *
 * @param edi the edi system to work on.
 */
static bool edi_do_up(edi_t* edi) {
    if (edi->current->prev) {
        edi->current = edi->current->prev;
        if (edi->y > 0) {
            edi->y--;
        } else {
            edi->top = edi->top->prev;
        }
        if (edi->x > edi->current->length) {
            edi->x = edi->current->length;
        }
        edi->num--;
        return true;
    } else {
        return false;
    }
}

/**
 * @brief editor command: do DOWN.
 *
 * @param edi the edi system to work on.
 */
static bool edi_do_down(edi_t* edi) {
    if (edi->current->next) {
        edi->current = edi->current->next;
        if (edi->y < edi->height - 2) {
            edi->y++;
        } else {
            edi->top = edi->top->next;
        }
        if (edi->x > edi->current->length) {
            edi->x = edi->current->length;
        }
        edi->num++;
        return true;
    } else {
        return false;
    }
}

/**
 * @brief editor command: do LEFT.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_left(edi_t* edi) {
    if (edi->x > 0) {
        edi->x--;
    } else if (edi->current->prev) {
        if (edi_do_up(edi)) {
            edi->x = edi->current->length;
        }
    }
}

/**
 * @brief editor command: do RIGHT.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_right(edi_t* edi) {
    if (edi->x < edi->current->length) {
        edi->x++;
    } else if (edi->current->next) {
        if (edi_do_down(edi)) {
            edi->x = 0;
        }
    }
}

/**
 * @brief editor command: do BACKSPACE.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_bs(edi_t* edi) {
    if (edi->x == 0 && edi->current->prev) {
        edi_do_up(edi);
        int x = edi->current->length;
        lin_joinnext(edi, edi->current);
        edi->x = x;
    } else {
        if (edi->current->length > 0 && edi->x > 0) {
            lin_delch_left(edi, edi->current, edi->x);
            edi_do_left(edi);
        }
    }
}

/**
 * @brief editor command: do DELETE.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_del(edi_t* edi) {
    if (edi->x >= edi->current->length && edi->current->next) {
        lin_joinnext(edi, edi->current);
    } else {
        if (edi->current->length > 0 && edi->x < edi->current->length) {
            lin_delch_right(edi, edi->current, edi->x);
        }
    }
}

/**
 * @brief delete current line.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_del_line(edi_t* edi) {
    lin_removeline(edi, edi->current);
    if (edi->x > edi->current->length) {
        edi->x = edi->current->length;
    }
}

/**
 * @brief editor command: do NEWLINE.
 *
 * @param edi the edi system to work on.
 * @param autoindent true for autoindentation, false to just insert a new line.
 */
static void edi_do_enter(edi_t* edi, bool autoindent) {
    int leading_spaces = 0;
    while ((leading_spaces < edi->current->length) && (edi->current->txt[leading_spaces] == ' ')) {
        leading_spaces++;
    }

    if (edi->x == edi->current->length) {
        line_t* l = lin_newline();
        if (l) {
            l->newline = true;
            lin_insertline(edi, edi->current, l);
        }
    } else {
        lin_splitline(edi, edi->current, edi->x);
    }
    edi->x = 0;
    edi_do_down(edi);
    if (autoindent) {
        for (int i = 0; i < leading_spaces; i++) {
            edi_do_insert_char(edi, ' ');
        }
    }
}

/**
 * @brief editor command: jump one word right.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_wordright(edi_t* edi) {
    int pre;
    do {
        pre = edi->x;
        edi_do_right(edi);
    } while (isalnum(edi->current->txt[edi->x]) && pre != edi->x);
    while (isspace(edi->current->txt[edi->x])) {
        edi_do_right(edi);
    }
}

/**
 * @brief editor command: jump one word left.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_wordleft(edi_t* edi) {
    int pre;
    do {
        pre = edi->x;
        edi_do_left(edi);
    } while (isalnum(edi->current->txt[edi->x]) && pre != edi->x);
}

/**
 * @brief editor command: insert TAB.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_tab(edi_t* edi) {
    lin_insertch(edi, edi->current, edi->x, ' ');
    edi_do_right(edi);
    while (edi->x % EDI_TABSIZE != 0) {
        lin_insertch(edi, edi->current, edi->x, ' ');
        edi_do_right(edi);
    }
}

/**
 * @brief remove up to 4 leading spaces.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_backtab(edi_t* edi) {
    int oldX = edi->x;
    edi->x = 0;
    int i = 0;
    while ((i < EDI_TABSIZE) && (edi->current->txt[0] == ' ')) {
        edi_do_del(edi);
        i++;
    }
    edi->x = oldX - i;
}

/**
 * @brief editor command: save the file.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_save(edi_t* edi) {
    char* error = edi_save(edi, edi->name);
    if (error) {
        dia_show_message(edi, error);
    }
}

/**
 * @brief go to line nr in file.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_goto_line(edi_t* edi) {
    char buff[DIA_ASK_SIZE];
    buff[0] = 0;
    dia_ask_text(edi, buff, "1234567890", "<Enter Line number, ENTER or ESC>");
    int line = atoi(buff) - 1;
    line_t* l = lin_find(edi, line);

    if (l) {
        edi_goto_line(edi, l, line);
    } else {
        dia_show_message(edi, "Line not found!");
    }
}

/**
 * @brief go to specific line.
 *
 * @param edi the edi system to work on.
 * @param l the line.
 * @param line_num the line number.
 */
static void edi_goto_line(edi_t* edi, line_t* l, int line_num) {
    if (l) {
        edi->current = edi->top = l;
        if (edi->x > edi->current->length) {
            edi->x = edi->current->length;
        }
        edi->y = 0;
        edi->num = line_num;
    }
}

/**
 * @brief compare find to txt.
 *
 * @param find null terminated string to find.
 * @param txt text buffer (not null terminated).
 * @param remainder number of chars left in buffer.
 *
 * @return true if the string was found
 * @return false if the string was not found
 */
static bool edi_cmp(char* find, char* txt, int remainder) {
    if (remainder < strlen(find)) {
        return false;
    }

    while (*find) {
        if (tolower(*find) != tolower(*txt)) {
            return false;
        }
        txt++;
        find++;
    }

    return true;
}

/**
 * @brief try to find string in edi startin with current line/x-pos
 *
 * @param edi the edi system to work on.
 * @param find_buffer the search string
 */
static void edi_do_find(edi_t* edi, char find_buffer[80]) {
    if (strlen(find_buffer)) {
        int xPos = edi->x + 1;
        line_t* l = edi->current;
        int line_inc = 0;
        while (true) {
            if (edi_cmp(find_buffer, &l->txt[xPos], l->length - xPos)) {
                // found
                edi->current = edi->top = l;
                edi->x = xPos;
                edi->y = 0;
                edi->num += line_inc;
                break;
            }
            if (xPos < l->length) {
                xPos++;
            } else if (l->next) {
                l = l->next;
                line_inc++;
                xPos = 0;
            } else {
                // EOF
                dia_show_message(edi, "Search string not found.");
                break;
            }
        }
    }
}

/**
 * @brief jump to start of file.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_start(edi_t* edi) {
    edi->current = edi->top = edi->first;
    edi->x = 0;
    edi->y = 0;
    edi->num = 1;
}

/**
 * @brief jump to end of file.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_end(edi_t* edi) {
    line_t* l = edi->current;
    while (l->next) {
        l = l->next;
        edi->num++;
    }
    edi->current = edi->top = l;
    edi->x = l->length;
    edi->y = 0;
}

/**
 * @brief jump up one page.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_pageup(edi_t* edi) {
    int i = edi->height - 1;

    line_t* new_top = edi->top;
    line_t* new_cur = edi->current;

    while (i && (new_top->prev != NULL) && (new_cur->prev != NULL)) {
        new_top = new_top->prev;
        new_cur = new_cur->prev;
        i--;
        edi->num--;
    }
    assert(new_top);
    assert(new_cur);
    edi->top = new_top;
    edi->current = new_cur;
    if (edi->x > edi->current->length) {
        edi->x = edi->current->length;
    }
}

/**
 * @brief jump down one page.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_pagedown(edi_t* edi) {
    int i = edi->height - 1;

    line_t* new_top = edi->top;
    line_t* new_cur = edi->current;

    while (i && (new_top->next != NULL) && (new_cur->next != NULL)) {
        new_top = new_top->next;
        new_cur = new_cur->next;
        i--;
        edi->num++;
    }
    assert(new_top);
    assert(new_cur);
    edi->top = new_top;
    edi->current = new_cur;
    if (edi->x > edi->current->length) {
        edi->x = edi->current->length;
    }
}

/**
 * @brief insert a character at cursor location.
 *
 * @param edi the edi system to work on.
 * @param ch the character to insert (no newlines, these must be inserted using edi_do_enter()).
 */
static void edi_do_insert_char(edi_t* edi, char ch) {
    lin_insertch(edi, edi->current, edi->x, ch);
    edi_do_right(edi);
}

/**
 * @brief clear current cnp buffer.
 *
 * @param edi the edi system to work on.
 */
static void edi_clear_cnp(edi_t* edi) {
    if (!edi->cnp) {
        edi->cnp = malloc(EDI_CNP_SIZE);  // TODO: handle alloc failure differently?
        assert(edi->cnp);
    }
    edi->cnp[0] = 0;
    edi->cnp_pos = 0;
    edi->cnp_size = EDI_CNP_SIZE;
}

/**
 * @brief add the given line to the current cnp-buffer.
 *
 * @param edi the edi system to work on.
 * @param l the line.
 * @param start first character of line to add.
 * @param end last character of line to add.
 * @param newline true to terminate the copy with a newline, false to just copy the line and not add a newline.
 */
static void edi_copy_line(edi_t* edi, line_t* l, int start, int end, bool newline) {
    if (end - start + 2 > edi->cnp_size - edi->cnp_pos) {
        int newSize = edi->cnp_size * 2;
        edi->cnp = realloc(edi->cnp, newSize);  // TODO: handle alloc failure differently?
        assert(edi->cnp);
        edi->cnp_size = newSize;
    }

    volatile char* dst = edi->cnp;
    volatile char* src = l->txt;
    while (start < end) {
        dst[edi->cnp_pos] = src[start];
        start++;
        edi->cnp_pos++;
    }

    // append newline and null byte
    if (newline) {
        edi->cnp[edi->cnp_pos] = '\n';
        edi->cnp_pos++;
    }
    edi->cnp[edi->cnp_pos] = 0;
}

/**
 * @brief handle CTRL-C copy command.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_copy(edi_t* edi) {
    if (EDI_IS_SEL(edi)) {
        edi_clear_cnp(edi);
        cnp_t cnp;
        edi_get_cnp(edi, &cnp);

        line_t* sLine = lin_find(edi, cnp.startY - 1);
        line_t* eLine = lin_find(edi, cnp.endY - 1);

        if (sLine == eLine) {
            // do copy when start==end
            edi_copy_line(edi, sLine, cnp.startX, cnp.endX, false);
        } else {
            // do copy for multiple lines
            edi_copy_line(edi, sLine, cnp.startX, sLine->length, true);  // copy from first line
            sLine = sLine->next;

            // copy in-between lines
            while (sLine != eLine) {
                edi_copy_line(edi, sLine, 0, sLine->length, true);
                EDIF("%s\n", sLine->txt);

                sLine = sLine->next;
            }

            // copy last line
            edi_copy_line(edi, sLine, 0, cnp.endX, false);
        }
    }
    if (edi->cnp && edi->cnp_pos > 0) {
        EDIF("cnp='%s'\n", edi->cnp);
    } else {
        EDIF("cnp='%s'\n", "EMPTY");
    }
}

/**
 * @brief handle CTRL-X cut command.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_cut(edi_t* edi) {
    edi_do_copy(edi);
    edi_do_del_sel(edi);
}

/**
 * @brief handle CTRL-V paste command.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_paste(edi_t* edi) {
    if (edi->cnp_pos) {
        if (EDI_IS_SEL(edi)) {
            edi_do_del_sel(edi);
        }

        // now paste text
        for (int i = 0; i < edi->cnp_pos; i++) {
            if (edi->cnp[i] == '\n') {
                edi_do_enter(edi, false);
            } else {
                edi_do_insert_char(edi, edi->cnp[i]);
            }
        }
        edi_clear_selection(edi);
    }
}

/**
 * @brief delete selected text.
 *
 * @param edi the edi system to work on.
 */
static void edi_do_del_sel(edi_t* edi) {
    if (EDI_IS_SEL(edi)) {
        cnp_t cnp;
        edi_get_cnp(edi, &cnp);

        line_t* sLine = lin_find(edi, cnp.startY - 1);
        line_t* eLine = lin_find(edi, cnp.endY - 1);

        if (sLine == eLine) {
            edi->x = cnp.startX;
            // delete when start==end
            for (int i = 0; i < cnp.endX - cnp.startX; i++) {
                lin_delch_right(edi, sLine, cnp.startX);
            }
        } else {
            if (cnp.cursor_at_end) {
                // move up until cursor is at line after selection start
                while (edi->current != sLine->next) {
                    edi_do_up(edi);
                    EDIF("UP cur=%p, sLine=%p, eLine=%p\n", edi->current, sLine, eLine);
                }
            } else {
                // move down once
                edi_do_down(edi);
                EDIF("DOWN cur=%p, sLine=%p, eLine=%p\n", edi->current, sLine, eLine);
            }

            // now we are at the line after selection start, delete all lines until sLine is next to eLine
            while (sLine->next != eLine) {
                edi_do_del_line(edi);
                EDIF("DEL cur=%p, sLine=%p, eLine=%p\n", edi->current, sLine, eLine);
            }
            edi_do_up(edi);
            edi->x = cnp.startX;

            // now lines are next to each other and cursor at start of selection
            int del_length = (sLine->length - cnp.startX) + cnp.endX + 1;
            for (int i = 0; i < del_length; i++) {
                EDIF("Now at %d, '%s'\n", edi->x, sLine->txt);
                edi_do_del(edi);
            }
        }
        edi_clear_selection(edi);
    }
}

/**
 * @brief editor input loop.
 *
 * @param edi the edi system to work on.
 *
 * @return description how the user quit the editor.
 */
static edi_exit_t edi_loop(edi_t* edi) {
    int last_help_pos = 0;
    char find_buffer[DIA_ASK_SIZE] = {0};
    edi_exit_t exit = EDI_KEEPRUNNING;
    while (exit == EDI_KEEPRUNNING) {
        int ch = getxkey();

        int shift = bioskey(_NKEYBRD_SHIFTSTATUS);

        // EDIF("ch = %02X, shift = %02X\n", ch, shift);

        switch (ch) {
            case K_Left:
            case K_ELeft:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi_do_left(edi);
                break;

            case K_Right:
            case K_ERight:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi_do_right(edi);
                break;

            case K_Down:
            case K_EDown:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi_do_down(edi);
                break;

            case K_Up:
            case K_EUp:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi_do_up(edi);
                break;

            case K_BackSpace:
                if (EDI_IS_SEL(edi)) {
                    edi_do_del_sel(edi);
                } else {
                    edi_do_bs(edi);
                }
                break;

            case K_Delete:
            case K_EDelete:
                if (EDI_IS_SEL(edi)) {
                    edi_do_del_sel(edi);
                } else {
                    edi_do_del(edi);
                }
                break;

            case K_Control_D:  // delete line
                edi_do_del_line(edi);
                break;

            case K_Control_L:  // jump to line
                edi_do_goto_line(edi);
                break;

            case K_Control_C:  // Copy
                edi_do_copy(edi);
                break;

            case K_Control_X:  // Cut
                edi_do_cut(edi);
                break;

            case K_Control_V:  // Paste
                edi_do_paste(edi);
                break;

            case K_Return:
                edi_do_enter(edi, true);
                break;

            case K_Home:
            case K_EHome:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi->x = 0;
                break;

            case K_End:
            case K_EEnd:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi->x = edi->current->length;
                break;

            case K_Control_Home:
            case K_Control_EHome:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi_do_start(edi);
                break;

            case K_Control_End:
            case K_Control_EEnd:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi_do_end(edi);
                break;

            case K_Control_Left:
            case K_Control_ELeft:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi_do_wordleft(edi);
                break;

            case K_Control_Right:
            case K_Control_ERight:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi_do_wordright(edi);
                break;

            case K_PageUp:
            case K_EPageUp:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi_do_pageup(edi);
                break;

            case K_PageDown:
            case K_EPageDown:
                if (EDI_SHIFT_DOWN(shift)) {
                    edi_start_selection(edi);
                } else {
                    edi_clear_selection(edi);
                }
                edi_do_pagedown(edi);
                break;

            case K_Tab:
                edi_do_tab(edi);
                break;

            case K_BackTab:
                edi_do_backtab(edi);
                break;

            case K_F1:  // show help
                dia_show_file(edi, EDI_HELPFILE, &last_help_pos, false);
                break;

            case K_Shift_F1:  // context help TODO
                break;

            case K_F3:  // save file
                edi_do_save(edi);
                break;

            case K_Shift_F4:  // truncate logfile and run
            {
                FILE* f = fopen(LOGFILE, "w");
                fflush(f);
                fclose(f);
                // FALLTHROUGH!
            }
            case K_F4:  // run script
                if (edi->changed) {
                    char* error = edi_save(edi, edi->name);
                    if (error) {
                        dia_show_message(edi, error);
                    } else {
                        exit = EDI_RUNSCRIPT;
                    }
                } else {
                    exit = EDI_RUNSCRIPT;
                }
                break;

            case K_F7:  // find
                if (dia_ask_text(edi, find_buffer, NULL, "Search")) {
                    edi_do_find(edi, find_buffer);
                }
                break;

            case K_Shift_F7:  // find again
                edi_do_find(edi, find_buffer);
                break;

            case K_F9:  // show log
                dia_show_file(edi, LOGFILE, NULL, true);
                break;

            case K_F10:  // exit
                if (!edi->changed || dia_show_confirm(edi, "File modified, really QUIT?")) {
                    exit = EDI_QUIT;
                }
                break;
#ifdef DEBUG_ENABLED
            case K_F12:
                EDIF("x=%02d, y=%02d, w=%02d, h=%02d, num=%03d, first=0x%p, top=0x%p, current=0x%p, '%s'\n", edi->x, edi->y, edi->width, edi->height, edi->num, (void*)edi->first,
                     (void*)edi->top, (void*)edi->current, edi->current->txt);
                break;
#endif  // DEBUG_ENABLED
            default:
                if (ch <= '~' && ch >= ' ') {
                    edi_clear_selection(edi);
                    edi_do_insert_char(edi, ch);
                }
                break;
        }
        edi_redraw(edi);
    }
    return exit;
}

/**
 * @brief match a syntax highlight string with current cursor position.
 *
 * @param sy word to find
 * @param txt start of text to compare with
 * @param remainder remaining length of txt.
 * @return true if this is a match
 * @return false if no match was found
 */
static bool edi_colcmp(syntax_t* sy, char* txt, int remainder) {
    char* find = sy->word;
    if (remainder < strlen(find)) {
        return false;
    }

    while (*find) {
        if (*find != *txt) {
            return false;
        }
        txt++;
        find++;
        remainder--;
    }

    if (remainder == 0 || !isalnum(*txt) || sy->length == -1) {
        return true;
    } else {
        return false;
    }
}

/**
 * @brief switch display color for syntax highlighting word.
 *
 * @param l the line we are working on.
 * @param pos the cursor position in the line.
 *
 * @return int the number of characters to draw in the just set color or 0 if no match was found.
 */
static int edi_syntax(line_t* l, int pos) {
    int w = 0;
    while (edi_wordlist[w].word) {
        if (pos == 0 || edi_wordlist[w].word[0] == '.' || !isalnum(l->txt[pos - 1])) {
            if (edi_colcmp(&edi_wordlist[w], &l->txt[pos], l->length - pos)) {
                textcolor(edi_wordlist[w].color);
                if (edi_wordlist[w].length == -1) {
                    return l->length - pos;
                } else {
                    return edi_wordlist[w].length;
                }
            }
        }
        w++;
    }
    return 0;
}

/**
 * @brief convert the current selection in the edi struct to an ordered description of the selection in a cnp struct.
 *
 * @param edi the edi system to work on.
 * @param cnp an ordered version of the start-stop of the selection.
 */
static void edi_get_cnp(edi_t* edi, cnp_t* cnp) {
    if (edi->num > edi->sel_line) {  // selection stretches downwards
        cnp->startX = edi->sel_char;
        cnp->startY = edi->sel_line;
        cnp->endX = edi->x;
        cnp->endY = edi->num;
        cnp->cursor_at_end = true;
    } else if (edi->num < edi->sel_line) {  // selection stretches upwards
        cnp->startX = edi->x;
        cnp->startY = edi->num;
        cnp->endX = edi->sel_char;
        cnp->endY = edi->sel_line;
        cnp->cursor_at_end = false;
    } else {  // all on same line
        cnp->startY = cnp->endY = edi->num;

        if (edi->x > edi->sel_char) {  // selection goes to the right
            cnp->startX = edi->sel_char;
            cnp->endX = edi->x;
            cnp->cursor_at_end = true;
        } else if (edi->x < edi->sel_char) {  // selection goes to the left
            cnp->startX = edi->x;
            cnp->endX = edi->sel_char;
            cnp->cursor_at_end = false;
        } else {  // selection is on itself
            cnp->startX = cnp->endX = edi->x;
            cnp->cursor_at_end = true;
        }
    }
}

/**
 * @brief set background color for text according to current select status.
 *
 * @param edi the edi system to work on.
 * @param x x-position in whole text.
 * @param y y-position in whole text.
 */
static void edi_sel_color(edi_t* edi, int x, int y) {
    textbackground(BLACK);
    if (EDI_IS_SEL(edi)) {  // check selection is active
        cnp_t cnp;
        edi_get_cnp(edi, &cnp);

        if ((cnp.startY == cnp.endY) && (y == cnp.startY)) {
            if ((x >= cnp.startX) && (x < cnp.endX)) {
                textbackground(BLUE);
            }
        } else {
            if ((y == cnp.startY) && (x >= cnp.startX)) {
                textbackground(BLUE);
            } else if ((y > cnp.startY) && (y < cnp.endY)) {
                textbackground(BLUE);
            } else if ((y == cnp.endY) && (x < cnp.endX)) {
                textbackground(BLUE);
            }
        }
    }
}

/**
 * @brief draw given line onto screen.
 *
 * @param edi the edi system to work on.
 * @param l the line to draw.
 * @param y y position on the currently drawn screen.
 * @param offset x-offset for the currently drawn screen (horizontal scrolling of long lines).
 * @param line_num the line number of the currently drawn line (total number).
 */
static void edi_draw_line(edi_t* edi, line_t** l, int y, int offset, int line_num) {
    textcolor(WHITE);
    gotoxy(1, y);
    int hskip = 0;
    if (*l) {
        for (int i = offset; i < (*l)->length && wherex() < edi->width; i++) {
            if (hskip <= 0) {
                textcolor(WHITE);
                hskip = edi_syntax(*l, i);
            }
            edi_sel_color(edi, i, line_num);
            putch((*l)->txt[i]);
            if (hskip > 0) {
                hskip--;
            }
        }
        *l = (*l)->next;
    }
    textbackground(BLACK);
    clreol();
}

/**
 * @brief redraw editor screen.
 *
 * @param edi the edi system to work on.
 */
static void edi_redraw(edi_t* edi) {
    int offset = 0;  // x drawing offset
    if (edi->x > edi->width - 1) {
        offset = edi->x - edi->width + 1;
    }

    textbackground(BLACK);
    textcolor(WHITE);
    edi_draw_status(edi);

    textbackground(BLACK);
    textcolor(WHITE);

    // if offsets are changes redraw the whole screen
    if ((edi->last_top != edi->top) || (edi->last_offset != offset) || EDI_IS_SEL(edi)) {
        line_t* l = edi->top;
        int line_num = edi->num - edi->y;
        for (int y = 2; y <= edi->height; y++) {
            edi_draw_line(edi, &l, y, offset, line_num);
            line_num++;
        }

        edi_draw_commands(edi);
    } else {
        // if not, only redraw current line
        line_t* l = edi->current;
        edi_draw_line(edi, &l, edi->y + 2, offset, edi->num);
    }

    // set cursor to current position
    if (offset) {
        gotoxy(edi->width, edi->y + 2);
    } else {
        gotoxy(edi->x + 1, edi->y + 2);  // set cursor to current position
    }

    if (edi->msg) {
        dia_show_message(edi, edi->msg);
        edi->msg = NULL;
        edi->last_offset = -1;
        edi_redraw(edi);
    }
    edi->last_top = edi->top;
    edi->last_offset = offset;
}

/**
 * @brief draw status bar.
 *
 * @param edi the edi system to work on.
 */
static void edi_draw_status(edi_t* edi) {
    textbackground(LIGHTGRAY);
    textcolor(BLACK);

    // draw 'modified' indicator
    gotoxy(edi->scr.winleft, edi->scr.wintop);
    cputs("DOjS " DOSJS_VERSION_STR " ");
    if (edi->changed) {
        cputs("* ");
    } else {
        cputs("  ");
    }

    // draw file/script name
    if (edi->name) {
        cputs(edi->name);
    } else {
        cputs("<unnamed>");
    }

    for (int i = wherex(); i < edi->scr.winright; i++) {
        cputs(" ");
    }

    // draw cursor position
    gotoxy(edi->scr.winright - 8, edi->scr.wintop);
    cprintf("%04d/%04d", edi->num, edi->x + 1);
}

/**
 * @brief draw command bar.
 *
 * @param edi the edi system to work on.
 */
static void edi_draw_commands(edi_t* edi) {
    // 10 f-keys @ 80 col := 6 chars+SPACE+num
    gotoxy(edi->scr.winleft, edi->scr.winbottom);
    for (int i = 0; i < EDI_NUM_COMMANDS; i++) {
        textbackground(BLACK);
        textcolor(WHITE);
        if (i == 0) {
            cprintf("%d", i + 1);
        } else {
            cprintf(" %d", i + 1);
        }

        textbackground(BLUE);
        textcolor(LIGHTGREEN);
        cprintf("%-6s", edi_f_keys[i]);
    }
}

/**
 * @brief initialize editor.
 *
 * @param fname filename to use in title bar.
 *
 * @return a usable editor with a single empty line.
 */
static edi_t* edi_init(char* fname) {
    textmode(C80);                  // switch to 80column/color mode
    clrscr();                       // clear screen
    _setcursortype(_NORMALCURSOR);  // underline cursor
    _wscroll = 0;                   // disable scrolling

    return lin_init(fname);
}

/**
 * @brief shutdown editor and free all ressources.
 *
 * @param edi the edi system to work on.
 */
static void edi_shutdown(edi_t* edi) {
    lin_shutdown(edi);
    textbackground(BLACK);
    textcolor(WHITE);
    clrscr();
    normvideo();
}

/**
 * @brief start selection of text at current cursor position.
 *
 * @param edi the edi system to work on.
 */
static void edi_start_selection(edi_t* edi) {
    if (!EDI_IS_SEL(edi)) {
        edi->sel_line = edi->num;
        edi->sel_char = edi->x;
    }
}

/***********************
** exported functions **
***********************/
/**
 * @brief clear currently selected text.
 *
 * @param edi the edi system to work on.
 */
void edi_clear_selection(edi_t* edi) {
    edi->sel_line = -1;
    edi->sel_char = -1;
    edi->last_offset = -1;
}

/**
 * @brief edi the given file.
 *
 * @param fname name of the file.
 *
 * @return a code describing why the editor was quit.
 */
edi_exit_t edi_edit(char* fname) {
    check_file_t exists = ut_check_file(fname);
    if (exists == CF_ERROR) {
        perror("Error accessing file");
        return EDI_ERROR;
    }

    edi_t* edi = edi_init(fname);
    if (exists == CF_YES) {
        char* error = edi_load(edi, fname);
        if (error) {
            fputs(error, stderr);
            return EDI_ERROR;
        }
    } else {
        // load template
        char* error = edi_load(edi, EDI_TEMPLATE);
        if (error) {
            fputs(error, stderr);
            return EDI_ERROR;
        }

        // replace template name with command line name again and mark as changed
        edi->changed = true;
        edi->name = fname;
        edi->msg = "New file: Template loaded!";
    }
    edi_redraw(edi);
    edi_exit_t exit = edi_loop(edi);
    edi_shutdown(edi);
    return exit;
}
