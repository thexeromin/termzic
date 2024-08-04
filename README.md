# Termzic


> [!WARNING]
> Currently, Termzic only works on Linux systems.

Termzic is a TUI music player featuring a simple set of controls and intuitive interface for easy audio playback.

### Demo
https://github.com/user-attachments/assets/43cd061d-7faa-4ed6-9983-c97480ded7d8

### Features
- Navigate between songs using arrow keys.
- Play, pause, and resume songs.
- Skip forward and backward by 5 seconds within a song.

### Requirements
- GCC compiler
- `ncurses` library

### Installation
- Clone the repository
- Compile the program using the provided Makefile
```bash
make
```

### Usage
```bash
./termzic PATH_TO_YOUR_MUSIC_FOLDER
```

### Controls
- **Up arrow**: move to the previous song.
- **Down arrow**: move to the next song.
- **p**: pause the current song.
- **r**: resume the paused song.
- **Right arrow**: skip forward 5 seconds in the current song.
- **Left arrow**: revert back 5 seconds in the current song.
