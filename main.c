#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <menu.h>
#include <linux/limits.h>
#include <limits.h>
#include <dirent.h>

#include "library.h"
#define MINIAUDIO_IMPLEMENTATION
#include "./miniaudio.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define CTRLD   4

node_t* songs_list = NULL;

void print_in_middle(
    WINDOW* win,
    int starty,
    int startx,
    int width,
    char* str,
    chtype color
);
void data_callback(
    ma_device* pDevice,
    void* pOutput,
    const void* pInput,
    ma_uint32 frameCount
);

int main(int argc, char* argv[]) {
    // ncurses vars
    ITEM** my_items;
	int c;				
	MENU* my_menu;
    WINDOW* my_menu_win;
    int songs_list_len, i;
    int width, height, starty, startx;
    DIR *d;
    struct dirent *dir;
    int currently_playing_song_index = -1;

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

    // Initialize songs
    if(d) {
        while((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                push(&songs_list, dir->d_name);
            }
        }
        closedir(d);
    }


	// Create items
    node_t* curr_node = songs_list;
    songs_list_len = list_length(curr_node);
    my_items = (ITEM**) calloc(songs_list_len + 1, sizeof(ITEM*));

    // miniaudio vars
    ma_result result;
    ma_decoder decoder[songs_list_len];
    ma_device_config deviceConfig;
    ma_device device[songs_list_len];
    char buf[PATH_MAX + 1];

    for(i = 0; curr_node != NULL; curr_node = curr_node->next, i++) {
        my_items[i] = new_item(" ", curr_node->data);
        
        // initialize decoder
        snprintf(buf, sizeof(buf), "%s%s", argv[1], curr_node->data);
        result = ma_decoder_init_file(buf, NULL, &decoder[i]);
        if (result != MA_SUCCESS) {
            printf("Could not load file: %s\n", buf);
            return -2;
        }

        deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format   = decoder[i].outputFormat;
        deviceConfig.playback.channels = decoder[i].outputChannels;
        deviceConfig.sampleRate        = decoder[i].outputSampleRate;
        deviceConfig.dataCallback      = data_callback;
        deviceConfig.pUserData         = &decoder[i];

        if (ma_device_init(NULL, &deviceConfig, &device[i]) != MA_SUCCESS) {
            printf("Failed to open playback device.\n");
            ma_decoder_uninit(&decoder[i]);
            return -3;
        }
    }
    // terminate my_items array with null
    my_items[i] = (ITEM*) NULL;

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
            case 'j':
				menu_driver(my_menu, REQ_DOWN_ITEM);
				break;
			case 'k':
				menu_driver(my_menu, REQ_UP_ITEM);
				break;
			case KEY_NPAGE:
				menu_driver(my_menu, REQ_SCR_DPAGE);
				break;
			case KEY_PPAGE:
				menu_driver(my_menu, REQ_SCR_UPAGE);
				break;
			case 'r':   // resume song
                if(currently_playing_song_index > -1) {
                    if(ma_device_start(&device[currently_playing_song_index]) != MA_SUCCESS) {
                        printf("Failed to start playback device.\n");
                        ma_device_uninit(&device[currently_playing_song_index]);
                        ma_decoder_uninit(&decoder[currently_playing_song_index]);
                        return -4;
                    }
                }
				break;
			case 'p':   // pause song
                if(currently_playing_song_index > -1) {
                    if(ma_device_stop(&device[currently_playing_song_index]) != MA_SUCCESS) {
                        printf("Failed to stop playback device.\n");
                        ma_device_uninit(&device[currently_playing_song_index]);
                        ma_decoder_uninit(&decoder[currently_playing_song_index]);
                        return -4;
                    }
                }
				break;
            case 10:  // Enter
                move(LINES - 2, startx);
				clrtoeol();
                ITEM* curr_item = current_item(my_menu);
                mvprintw(LINES - 2, startx, "Playing: %s",
                        item_description(curr_item));
                refresh();
				pos_menu_cursor(my_menu);

                // stop playing song
                if(currently_playing_song_index > -1) {
                    ma_device_stop(&device[currently_playing_song_index]);
                }

                // play audio
                currently_playing_song_index = get_index_of_node(
                    songs_list, item_description(curr_item)
                );
                if(ma_device_start(&device[currently_playing_song_index]) != MA_SUCCESS) {
                    printf("Failed to start playback device.\n");
                    ma_device_uninit(&device[0]);
                    ma_decoder_uninit(&decoder[0]);
                    return -4;
                }
				break;
		}
        wrefresh(my_menu_win);
    }

	/* Unpost and free all the memory taken up */
    unpost_menu(my_menu);
    free_menu(my_menu);
    for(i = 0; i < songs_list_len; ++i) {
        free_item(my_items[i]);
        ma_device_uninit(&device[i]);
        ma_decoder_uninit(&decoder[i]);
    }
    free(my_items);
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

void data_callback(
    ma_device* pDevice,
    void* pOutput,
    const void* pInput,
    ma_uint32 frameCount
) {
    ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
    if(pDecoder == NULL) {
        return;
    }

    ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, NULL);

    (void)pInput;
}
