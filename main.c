#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <menu.h>
#include <dirent.h>

#include "library.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define CTRLD   4

node_t* songs_list = NULL;
char *choices[] = {
    "Choice 1",
    "Choice 2",
    "Choice 3",
    "Choice 4",
    "Choice 5",
    "Choice 6",
    "Choice 7",
    "Choice 8",
    "Choice 9",
    "Choice 10",
    "Choice 11",
    "Choice 12",
    "Choice 13",
    "Choice 14",
    "Choice 15",
    "Choice 16",
    "Choice 17",
    "Choice 18",
};

void print_in_middle(
    WINDOW* win,
    int starty,
    int startx,
    int width,
    char* str,
    chtype color
);

int main(int argc, char* argv[]) {
    ITEM** my_items;
	int c;				
	MENU* my_menu;
    WINDOW* my_menu_win;
    int n_choices, i;
    int width, height, starty, startx;
    DIR *d;
    struct dirent *dir;

    if(argc < 2) {
        printf("directory argument missing\n");
        exit(1);
    }

    d = opendir(argv[1]);

    /* Initialize curses */
    initscr();
	start_color();
    cbreak();
    noecho();
	keypad(stdscr, TRUE);
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_CYAN, COLOR_BLACK);

    width = COLS - 5;
    height = LINES / 2;
    startx = (COLS - width) / 2;
    starty = 1;

    /* Initialize songs */
    if(d) {
        while((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                push(&songs_list, dir->d_name);
            }
        }
        closedir(d);
    }

	/* Create items */
    node_t* curr_node = songs_list;
    n_choices = list_length(curr_node);
    my_items = (ITEM**) calloc(n_choices, sizeof(ITEM*));

    // mvprintw((LINES - 4), 2, "%d\n", n_choices);
    for(int i = 0; curr_node != NULL; curr_node = curr_node->next, i++) {
        my_items[i] = new_item(" ", curr_node->data);
        // mvprintw((LINES - 2) + i, 2, "%s\n", curr_node->data);
    }
    /* TODO: remove */
    /* for(i = 0; i < n_choices; ++i) {
        my_items[i] = new_item(" ", choices[i]);
    } */

	/* Crate menu */
	my_menu = new_menu((ITEM**) my_items);

	/* Create the window to be associated with the menu */
    my_menu_win = newwin(height, width, starty, startx);
    keypad(my_menu_win, TRUE);
     
	/* Set main window and sub window */
    set_menu_win(my_menu, my_menu_win);
    set_menu_sub(my_menu, derwin(my_menu_win, height - 4, width - 2, 3, 1));
	set_menu_format(my_menu, height - 4, 1);
			
	/* Set menu mark to the string " *" */
    set_menu_mark(my_menu, " *");

	/* Print a border around the main window and print a title */
    box(my_menu_win, 0, 0);
	print_in_middle(my_menu_win, 1, 0, COLS - 5, "Termzic", COLOR_PAIR(1));
	mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
	mvwhline(my_menu_win, 2, 1, ACS_HLINE, width - 2);
	mvwaddch(my_menu_win, 2, width - 1, ACS_RTEE);
        
	/* Post the menu */
	post_menu(my_menu);
	wrefresh(my_menu_win);
	refresh();

	while((c = wgetch(my_menu_win)) != KEY_F(1)) {
        switch(c) {
            case KEY_DOWN:
				menu_driver(my_menu, REQ_DOWN_ITEM);
				break;
			case KEY_UP:
				menu_driver(my_menu, REQ_UP_ITEM);
				break;
			case KEY_NPAGE:
				menu_driver(my_menu, REQ_SCR_DPAGE);
				break;
			case KEY_PPAGE:
				menu_driver(my_menu, REQ_SCR_UPAGE);
				break;
		}
        wrefresh(my_menu_win);
    }

	/* Unpost and free all the memory taken up */
    unpost_menu(my_menu);
    free_menu(my_menu);
    for(i = 0; i < n_choices; ++i) {
        free_item(my_items[i]);
    }
	endwin();
}

void print_in_middle(
    WINDOW* win,
    int starty,
    int startx,
    int width,
    char* str,
    chtype color
) {
    int length, x, y;
	float temp;

	if(win == NULL)
        win = stdscr;
	getyx(win, y, x);
	if(startx != 0)
        x = startx;
	if(starty != 0)
		y = starty;
	if(width == 0)
		width = 80;

	length = strlen(str);
	temp = (width - length)/ 2;
	x = startx + (int) temp;
	wattron(win, color);
	mvwprintw(win, y, x, "%s", str);
	wattroff(win, color);
	refresh();
}
