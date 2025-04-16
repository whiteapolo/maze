#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <SDL2/SDL.h>

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define MIN_WINDOW_WIDTH 800
#define MIN_WINDOW_HEIGHT 800
#define MAX_WINDOW_WIDTH 800
#define MAX_WINDOW_HEIGHT 800

#define MAZE_BORDER_COLOR 0x707070 /*0xbab5a8*/
#define MAZE_BORDER_WIDTH 3
#define WINDOW_BACKGROUND_COLOR 0x282828 // 0x0e232e
#define TILE_WIDTH  40
#define TILE_HEIGHT TILE_WIDTH

#define MAZE_ROWS 20
#define MAZE_COLS 20

#define LeftBorderMask    0b00001
#define RightBorderMask   0b00010
#define TopBorderMask     0b00100
#define BottomBorderMask  0b01000
#define BorderDot         0b10000
#define BorderAllMask     0b11111
#define BorderNoneMask    0b00000

typedef struct {
	u8 borderStyle;
	u32 row;
	u32 col;
} Tile;

typedef struct {
	Tile **mat;
	u32 rows;
	u32 cols;
} Maze;

static Display *dpy;
static int scr;
static Window *root;
static Window win;
static XEvent ev;
static GC gc;
static Maze *maze;
static u32 window_width = 0;
static u32 window_height = 0;

void **alocate_mat(u32 rows, u32 cols, size_t element_size)
{
	void **mat = malloc(sizeof(void *) * rows);

	for (u32 i = 0; i < rows; i++) {
		mat[i] = malloc(element_size * cols);
	}

	return mat;
}

void reset_maze(Maze *maze)
{
	for (u32 i = 0; i < maze->rows; i++) {
		for (u32 j = 0; j < maze->cols; j++) {
			maze->mat[i][j].borderStyle = BorderAllMask;
			maze->mat[i][j].row = i;
			maze->mat[i][j].col = j;
		}
	}
}

// TODO: change naming to snake case
Maze *createEmptyMaze(u32 rows, u32 cols)
{
	Maze *maze = malloc(sizeof(Maze));
	maze->rows = rows;
	maze->cols = cols;
	maze->mat = (Tile **)alocate_mat(rows, cols, sizeof(Tile));

	reset_maze(maze);

	return maze;
}

void initX()
{
	dpy = XOpenDisplay(NULL);

	if (dpy == NULL) {
		printf("couldnt connect to X server.\n");
		exit(1);
	}

	win = XCreateSimpleWindow(
		dpy,
		RootWindow(dpy, scr),
		0, 0,
		WINDOW_WIDTH, WINDOW_HEIGHT,
		0, 0xffffff, WINDOW_BACKGROUND_COLOR
	);

	XSelectInput(dpy, win, ExposureMask | KeyPressMask);
	gc = DefaultGC(dpy, scr);

	XSizeHints* sizeHints = XAllocSizeHints();
	sizeHints->flags = PMinSize;
	sizeHints->min_width = MIN_WINDOW_WIDTH;
	sizeHints->min_height = MIN_WINDOW_HEIGHT;
	sizeHints->max_width = MAX_WINDOW_WIDTH;
	sizeHints->max_height = MAX_WINDOW_HEIGHT;
	XSetWMNormalHints(dpy, win, sizeHints);
	XFree(sizeHints);

	XMapWindow(dpy, win);
}

void closeX()
{
	XUnmapWindow(dpy, win);
	XDestroyWindow(dpy, win);
}

void drawTile(const Tile tile, u32 x, u32 y)
{
	XSetForeground(dpy, gc, MAZE_BORDER_COLOR);

	if (tile.borderStyle & BorderDot) {
		XFillRectangle(dpy, win, gc,
				x, y, MAZE_BORDER_WIDTH, MAZE_BORDER_WIDTH);

		XFillRectangle(dpy, win, gc,
				x + TILE_WIDTH - MAZE_BORDER_WIDTH, y, MAZE_BORDER_WIDTH, MAZE_BORDER_WIDTH);

		XFillRectangle(dpy, win, gc,
				x, y + TILE_WIDTH - MAZE_BORDER_WIDTH, MAZE_BORDER_WIDTH, MAZE_BORDER_WIDTH);

		XFillRectangle(dpy, win, gc,
				x + TILE_WIDTH - MAZE_BORDER_WIDTH, y + TILE_WIDTH - MAZE_BORDER_WIDTH, MAZE_BORDER_WIDTH, MAZE_BORDER_WIDTH);
	}

	if (tile.borderStyle & LeftBorderMask) {
		XFillRectangle(dpy, win, gc,
				x, y, MAZE_BORDER_WIDTH, TILE_HEIGHT);
	}

	if (tile.borderStyle & RightBorderMask) {
		XFillRectangle(dpy, win, gc,
				x + TILE_WIDTH - MAZE_BORDER_WIDTH, y, MAZE_BORDER_WIDTH, TILE_HEIGHT);
	}

	if (tile.borderStyle & TopBorderMask) {
		XFillRectangle(dpy, win, gc,
				x, y, TILE_WIDTH, MAZE_BORDER_WIDTH);
	}

	if (tile.borderStyle & BottomBorderMask) {
		XFillRectangle(dpy, win, gc,
				x, y + TILE_HEIGHT - MAZE_BORDER_WIDTH, TILE_WIDTH, MAZE_BORDER_WIDTH);
	}
}

void connectTwoTiles(Tile *a, Tile *b)
{
	if (a->row + 1 == b->row) {
		a->borderStyle &= ~RightBorderMask;
		b->borderStyle &= ~LeftBorderMask;
	} else if (a->row - 1 == b->row) {
		a->borderStyle &= ~LeftBorderMask;
		b->borderStyle &= ~RightBorderMask;
	} else if (a->col + 1 == b->col) {
		a->borderStyle &= ~BottomBorderMask;
		b->borderStyle &= ~TopBorderMask;
	} else if (a->col - 1 == b->col){
		a->borderStyle &= ~TopBorderMask;
		b->borderStyle &= ~BottomBorderMask;
	}
}

Tile *getUnvisitedTile(Maze *maze, bool visited[][maze->cols], u32 row, u32 col)
{
	Tile *stack[4];
	u32 stackPtr = 0;

	if (row > 0 && !visited[row - 1][col]) {
		stack[stackPtr++] = &maze->mat[row - 1][col];
	}

	if (col > 0 && !visited[row][col - 1]) {
		stack[stackPtr++] = &maze->mat[row][col - 1];
	}

	if (row + 1 < maze->rows && !visited[row + 1][col]) {
		stack[stackPtr++] = &maze->mat[row + 1][col];
	}

	if (col + 1 < maze->cols && !visited[row][col + 1]) {
		stack[stackPtr++] = &maze->mat[row][col + 1];
	}

	return stackPtr == 0 ? NULL : stack[rand() % stackPtr];
}

void randomizeMaze(Maze *maze)
{
	bool visited[maze->rows][maze->cols];
	Tile *stack[maze->rows * maze->cols];
	u32 stackPtr = 0;

	memset(visited, 0, sizeof(visited));

	// stack[stackPtr++] = &maze->mat[rand() % maze->rows][rand() % maze->cols];
	stack[stackPtr++] = &maze->mat[0][0];

	while (stackPtr > 0) {
		Tile *curr = stack[stackPtr - 1];
		visited[curr->row][curr->col] = true;

		Tile *next = getUnvisitedTile(maze, visited, curr->row, curr->col);

		if (next == NULL) {
			stackPtr--;
		} else {
			connectTwoTiles(curr, next);
			stack[stackPtr++] = next;
		}
	}
}

Maze *createRandomMaze(u32 rows, u32 cols)
{
	Maze *maze = createEmptyMaze(rows, cols);
	randomizeMaze(maze);
	return maze;
}

void freeMaze(Maze *maze)
{
	for (u32 i = 0; i < maze->rows; i++) {
		free(maze->mat[i]);
	}

	free(maze->mat);
	free(maze);
}

void drawMaze()
{
	int padX = 0;
	int padY = 0;

	for (u32 i = 0; i < maze->rows; i++) {
		for (u32 j = 0; j < maze->cols; j++) {
			drawTile(maze->mat[i][j], i * TILE_WIDTH + padX, j * TILE_HEIGHT + padY);
		}
	}
}

void handleKeyPress()
{
	KeySym keysym = XLookupKeysym(&ev.xkey, 0);

	if (keysym == XK_g) {
		reset_maze(maze);
		randomizeMaze(maze);
		XClearWindow(dpy, win);
		drawMaze();
	}

	if (keysym == XK_Escape || keysym == XK_q) {
		closeX();
		exit(0);
	}
}

int main(void)
{
	srand(time(NULL));
	initX();
	maze = createRandomMaze(MAZE_ROWS, MAZE_COLS);

	for (;;) {

		XNextEvent(dpy, &ev);

		switch (ev.type) {
			case Expose: {
				 if (ev.xexpose.width != window_width || ev.xexpose.height != window_height) {
					 window_width = ev.xexpose.width;
					 window_height = ev.xexpose.height;
					 freeMaze(maze);
					 maze = createRandomMaze(window_width / TILE_WIDTH, window_height / TILE_HEIGHT);
				 }
				 drawMaze();
			 } break;

			case KeyPress: {
				handleKeyPress();
			} break;

		}

	}

	return 0;
}
