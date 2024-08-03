#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <menu.h>
#include <linux/limits.h>
#include <dirent.h>

#define MINIAUDIO_IMPLEMENTATION
#include "./miniaudio.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define CTRLD   4
#define MAX_DECODERS 50
#define MAX_AUDIO_FILES 50
#define SUCCESS 0
#define FAILURE 1

typedef struct  {
    char name[PATH_MAX + 1];
    char path[PATH_MAX + 1];
} AudioFile;

// global variables
static int current_song_index = -1;
static bool is_audio_pause = false;
AudioFile* files;
ma_device device;
ma_decoder* decoders;

// module functions
int fetch_audio_files(char* path);
int init_device_and_decoder(int total_files);
void data_callback(
    ma_device* pDevice,
    void* pOutput,
    const void* pInput,
    ma_uint32 frameCount
);
void print_current_audio(int starty, int startx, char* audio_name);
void print_in_middle(
    WINDOW* win,
    int starty,
    int startx,
    int width,
    char* str,
    chtype color
);

int main(int argc, char* argv[]) {
    // ncurses vars
    ITEM** my_items;
	MENU* my_menu;
    WINDOW* my_menu_win;
    int i, c;
    int width, height, starty, startx;
    decoders = (ma_decoder*) malloc(sizeof(ma_decoder) * MAX_DECODERS);
    files = (AudioFile*) malloc(sizeof(AudioFile) * MAX_AUDIO_FILES);

    if(argc < 2) {
        fprintf(stderr, "directory argument missing\n");
        exit(1);
    }

    // fetch audio files
    int total_audio_files = fetch_audio_files(argv[1]);
    if(total_audio_files < 1) {
        fprintf(stderr, "Not able to fetch audio files from %s directory\n", argv[1]);
        exit(1);
    }

    // initialize miniaudio decoder and device array
    if(init_device_and_decoder(total_audio_files) != SUCCESS) {
        fprintf(stderr, "Not able to initialize device & decoder\n");
        exit(1);
    }

    // initialize curses
    initscr();
	start_color();
    cbreak();
    noecho();
	keypad(stdscr, TRUE);
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_CYAN, COLOR_BLACK);
    curs_set(0);

    width = COLS - 5;
    height = LINES / 2;
    startx = (COLS - width) / 2;
    starty = 1;
    
	// create items
    my_items = (ITEM**) calloc(total_audio_files + 1, sizeof(ITEM*));

    for(i = 0; i < total_audio_files; i++) {
        my_items[i] = new_item(" ", files[i].name);
    }

    // terminate my_items array with null
    my_items[i] = (ITEM*) NULL;

	// create menu
	my_menu = new_menu((ITEM**) my_items);

	// create the window to be associated with the menu
    my_menu_win = newwin(height, width, starty, startx);
    nodelay(my_menu_win, TRUE);
    keypad(my_menu_win, TRUE);
     
	// set main window and sub window
    set_menu_win(my_menu, my_menu_win);
    set_menu_sub(my_menu, derwin(my_menu_win, height - 4, width - 2, 3, 1));
	set_menu_format(my_menu, height - 4, 1);
			
	// set menu mark to the string " *"
    set_menu_mark(my_menu, " *");

	// print a border around the main window and print a title
    box(my_menu_win, 0, 0);
	print_in_middle(my_menu_win, 1, 0, COLS - 5, "Termzic", COLOR_PAIR(1));
	mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
	mvwhline(my_menu_win, 2, 1, ACS_HLINE, width - 2);
	mvwaddch(my_menu_win, 2, width - 1, ACS_RTEE);
        
	// post the menu
	post_menu(my_menu);
	wrefresh(my_menu_win);
	refresh();

	while((c = wgetch(my_menu_win)) != KEY_F(1)) {
        if(current_song_index > -1) {
            print_current_audio(
                LINES - 2,
                startx,
                files[current_song_index].name
            );
        }

        switch(c) {
            case KEY_DOWN:
            case 'j':
				menu_driver(my_menu, REQ_DOWN_ITEM);
				break;
			case KEY_UP:
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
                is_audio_pause = false;
				break;
			case 'p':   // pause song
                is_audio_pause = true;
				break;
            case 10:  // enter
                ITEM* curr_item = current_item(my_menu);
				pos_menu_cursor(my_menu);

                // change song
                current_song_index = item_index(curr_item);
                ma_decoder_seek_to_pcm_frame(&decoders[current_song_index], 0);
				break;
		}
        wrefresh(my_menu_win);
    }

	// unpost and free all the memory taken up
    unpost_menu(my_menu);
    free_menu(my_menu);
    ma_device_stop(&device);
    ma_device_uninit(&device);
    for(i = 0; i < total_audio_files; ++i) {
        free_item(my_items[i]);
        ma_decoder_uninit(&decoders[i]);
    }
    free(my_items);
	endwin();
}

// fetch_audio_files: fetch all files from dir and put in files array
int fetch_audio_files(char* path) {
    DIR *d;
    struct dirent *dir;
    int i = 0;
    char buf[PATH_MAX + 1];

    d = opendir(path);
    if(d) {
        while((dir = readdir(d)) != NULL) {
            if(i >= MAX_AUDIO_FILES) return i;

            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                // create full path of a audio file
                snprintf(buf, sizeof(buf), "%s%s", path, dir->d_name);

                // add in array
                strcpy(files[i].name, dir->d_name);
                strcpy(files[i].path, buf);

                i++;
            }
        }
        closedir(d);
        return i;
    }
    return -1;
}

// init miniaudio device & decoder array from files
int init_device_and_decoder(int total_audio_files) {
    ma_result result;
    ma_device_config device_config;

    ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_f32, 2, 48000);

    for(int i = 0; i < total_audio_files; i++) {
        result = ma_decoder_init_file(files[i].path, &decoder_config, &decoders[i]);

        if (result != MA_SUCCESS) {
            fprintf(stderr, "Could not load file: %s\n", files[i].path);
            return -2;
        }
    }

    device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.format   = ma_format_f32;
    device_config.playback.channels = 2;
    device_config.sampleRate        = 48000;
    device_config.dataCallback      = data_callback;
    device_config.pUserData         = NULL;

    if (ma_device_init(NULL, &device_config, &device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open playback device.\n");
        ma_device_uninit(&device);
        return -3;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to play device\n");
        ma_device_uninit(&device);
        return -3;
    }

    return SUCCESS;
}

void data_callback(
    ma_device* pDevice,
    void* pOutput,
    const void* pInput,
    ma_uint32 frameCount
) {
    (void) pDevice;
    if(current_song_index > -1 && !is_audio_pause) {
        ma_uint64 framesRead = ma_decoder_read_pcm_frames(
            &decoders[current_song_index],
            pOutput,
            frameCount,
            NULL
        );

        if(framesRead > frameCount) {
            current_song_index++;
            ma_decoder_seek_to_pcm_frame(&decoders[current_song_index], 0);
        }
    }

    (void)pInput;
}

void print_current_audio(int starty, int startx, char* audio_name) {
    move(starty, startx);
    clrtoeol();
    mvprintw(starty, startx, "Playing: %s", audio_name);
    refresh();
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