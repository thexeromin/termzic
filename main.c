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
#define SUCCESS 0
#define FAILURE 1

// global variables
node_t* songs_list = NULL;
static int current_song_index = -1;
// static bool is_song_playing = false;

// module functions
int fetch_audio_files(node_t** node, char* path);
int init_device_and_decoder(
    node_t* node,
    ma_decoder* decoders,
    ma_device* devices,
    char* path
);
int play_song(ma_device* device, int audio_index);
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

    if(argc < 2) {
        printf("directory argument missing\n");
        exit(1);
    }

    // fetch audio files
    int total_audio_files = fetch_audio_files(&songs_list, argv[1]);
    if(total_audio_files < 1) {
        fprintf(stderr, "Not able to fetch audio files from %s directory\n", argv[1]);
        exit(1);
    }

    // miniaudio vars
    ma_decoder decoder[total_audio_files];
    ma_device device[total_audio_files];

    // initialize miniaudio decoder and device array
    if(init_device_and_decoder(
        songs_list,
        decoder,
        device,
        argv[1]
    ) != SUCCESS) {
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

    width = COLS - 5;
    height = LINES / 2;
    startx = (COLS - width) / 2;
    starty = 1;
    
	// create items
    node_t* curr_node = songs_list;
    my_items = (ITEM**) calloc(total_audio_files + 1, sizeof(ITEM*));

    for(i = 0; curr_node != NULL; curr_node = curr_node->next, i++) {
        my_items[i] = new_item(" ", curr_node->data);
    }

    // terminate my_items array with null
    my_items[i] = (ITEM*) NULL;

	// create menu
	my_menu = new_menu((ITEM**) my_items);

	// create the window to be associated with the menu
    my_menu_win = newwin(height, width, starty, startx);
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
                if(current_song_index > -1) {
                    if(ma_device_start(&device[current_song_index]) != MA_SUCCESS) {
                        printf("Failed to start playback device.\n");
                        ma_device_uninit(&device[current_song_index]);
                        ma_decoder_uninit(&decoder[current_song_index]);
                        return -4;
                    }
                }
				break;
			case 'p':   // pause song
                if(current_song_index > -1) {
                    if(ma_device_stop(&device[current_song_index]) != MA_SUCCESS) {
                        printf("Failed to stop playback device.\n");
                        ma_device_uninit(&device[current_song_index]);
                        ma_decoder_uninit(&decoder[current_song_index]);
                        return -4;
                    }
                }
				break;
            case 10:  // enter
                // print currently playing audio file
                ITEM* curr_item = current_item(my_menu);
                print_current_audio(
                    LINES - 2,
                    startx,
                    (char*) item_description(curr_item)
                );
				pos_menu_cursor(my_menu);

                // start playing
                int selected_audio_index = get_index_of_node(
                    songs_list, item_description(curr_item)
                );
                if(play_song(device, selected_audio_index) != SUCCESS) {
                    printf("Failed to start playback device.\n");
                    ma_device_uninit(&device[selected_audio_index]);
                    ma_decoder_uninit(&decoder[selected_audio_index]);
                }
				break;
		}
        // move(LINES - 3, startx);
        // clrtoeol();
        // mvprintw(LINES - 3, startx, "LOG %d", counter++);
        // // check for song finished or not
        // if(!is_song_playing) {
        //     ma_uint64 currentFrameIndex;
        //     ma_uint64 totalFrameCount;

        //     ma_decoder_get_cursor_in_pcm_frames(
        //         &decoder[current_song_index],
        //         &currentFrameIndex
        //     );
        //     ma_decoder_get_length_in_pcm_frames(
        //         &decoder[current_song_index],
        //         &totalFrameCount
        //     );

        //     if (currentFrameIndex >= totalFrameCount) {
        //         is_song_playing = true;
        //         // change song
        //         current_song_index++;
        //         move(LINES - 2, startx);
		// 		clrtoeol();
        //         mvprintw(LINES - 2, startx, "Playing: %s",
        //                 item_description(my_items[current_song_index]));
        //         refresh();
        //         if(ma_device_start(&device[current_song_index]) != MA_SUCCESS) {
        //             printf("Failed to start playback device.\n");
        //             ma_device_uninit(&device[0]);
        //             ma_decoder_uninit(&decoder[0]);
        //             return -4;
        //         }
        //         is_song_playing = false;
        //     }
        // }
        // ma_sleep(50);
        wrefresh(my_menu_win);
    }

	// unpost and free all the memory taken up
    unpost_menu(my_menu);
    free_menu(my_menu);
    for(i = 0; i < total_audio_files; ++i) {
        free_item(my_items[i]);
        ma_device_uninit(&device[i]);
        ma_decoder_uninit(&decoder[i]);
    }
    free(my_items);
	endwin();
}

// fetch_audio_files: add all audio files to node from dir path
int fetch_audio_files(node_t** node, char* path) {
    DIR *d;
    struct dirent *dir;
    int i = 0;

    d = opendir(path);
    if(d) {
        while((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                push(node, dir->d_name);
                i++;
            }
        }
        closedir(d);
        return i;
    }
    return -1;
}

// init miniaudio device & decoder array from node
int init_device_and_decoder(
    node_t* node,
    ma_decoder* decoders,
    ma_device* devices,
    char* path
) {
    ma_result result;
    ma_device_config deviceConfig;
    char buf[PATH_MAX + 1];

    for(int i = 0; node != NULL; node = node->next, i++) {
        // create full path of a audio file
        snprintf(buf, sizeof(buf), "%s%s", path, node->data);

        result = ma_decoder_init_file(buf, NULL, &decoders[i]);
        if (result != MA_SUCCESS) {
            fprintf(stderr, "Could not load file: %s\n", buf);
            return -2;
        }

        deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format   = decoders[i].outputFormat;
        deviceConfig.playback.channels = decoders[i].outputChannels;
        deviceConfig.sampleRate        = decoders[i].outputSampleRate;
        deviceConfig.dataCallback      = data_callback;
        deviceConfig.pUserData         = &decoders[i];

        if (ma_device_init(NULL, &deviceConfig, &devices[i]) != MA_SUCCESS) {
            fprintf(stderr, "Failed to open playback device.\n");
            ma_decoder_uninit(&decoders[i]);
            return -3;
        }
    }

    return SUCCESS;
}

int play_song(ma_device* device, int audio_index) {
    // if audio playing then stop
    if(current_song_index > -1) {
        ma_device_stop(&device[current_song_index]);
    }

    // play audio
    // current_song_index = get_index_of_node(
    //     songs_list, item_description(curr_item)
    // );
    current_song_index = audio_index;
    if(ma_device_start(&device[current_song_index]) != MA_SUCCESS) {
        // printf("Failed to start playback device.\n");
        // ma_device_uninit(&device[0]);
        // ma_decoder_uninit(&decoder[0]);
        return FAILURE;
    }

    return SUCCESS;
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