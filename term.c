#include "term.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>

int rl_gnu_readline_p = 0;

TermInfo* currentTerm = NULL;

enum KEYS{
	KEY_CTRL_A = 1,
	KEY_CTRL_C = 3,
	KEY_CTRL_E = 5,
	KEY_TAB = '\t',
	KEY_ENTER = '\r',
	KEY_ESCAPE = '\x1b',
	KEY_BACKSPACE = 127
};

void gwtClearBuffer(){
	memset(currentTerm->buffer, 0, currentTerm->bufferSize);
	currentTerm->endPosition = 0;
	currentTerm->currentPositionInBuffer = 0;
}

//This fuction will allow removing an arbitrary number of elements from the buffer.
//TODO: Bounds checking
void gwtRemoveFromBuffer(uint16_t loc, uint16_t size){
	uint16_t afterRemove = loc+size;
	uint16_t moveSize = currentTerm->endPosition - afterRemove;
	memmove(&currentTerm->buffer[loc], &currentTerm->buffer[afterRemove], moveSize);
	uint16_t clearStart = currentTerm->endPosition - size;
	memset(&currentTerm->buffer[clearStart], 0, size);
	currentTerm->endPosition = clearStart;
}

//This function will shift the items in the buffer a given distance
//TODO: Bounds checking
void gwtAddToBuffer(uint16_t loc, uint16_t size){
	uint16_t sizeToMove = (currentTerm->endPosition - loc);
	memmove(&currentTerm->buffer[loc+size], &currentTerm->buffer[loc], sizeToMove);
	currentTerm->endPosition += size;
}

int gwtSetNewTermios(){
	return tcsetattr(currentTerm->fd,TCSAFLUSH,&(currentTerm->currentTermios));
}

void gwtSetOldTermios(){
	tcsetattr(currentTerm->fd,TCSAFLUSH,&(currentTerm->originalTermios));
}

int gwtGetTerminalSize(){
	//Get number of rows and columns from the system. ioctl is a great way to do this
	struct winsize terminalSize;
	if(ioctl(currentTerm->fd, TIOCGWINSZ, &terminalSize)){
		//unable to get from ioctl
		return -1;
	} else{
		currentTerm->rows = terminalSize.ws_row;
		currentTerm->columns = terminalSize.ws_col;
		return 0;
	}
}

//Set cleanup and returning the system to a previous state
void gwtAtExit(){
	if(currentTerm == NULL) return;
	gwtSetOldTermios();
	if(currentTerm->buffer) free(currentTerm->buffer);
	if(currentTerm->currentPrompt) free(currentTerm->currentPrompt);
	free(currentTerm);
	currentTerm = NULL;
	gwtRemoveSignalHandlers();
}

//Initialize the system
int gwtInitialize(int fd){
	//If we are not using a tty device (whole point of this) error out
	if(!isatty(fd)) return -1;
	
	//Allocate the information to store the current terminal state
	currentTerm = malloc(sizeof(TermInfo));
	memset(currentTerm, 0, sizeof(TermInfo));
	currentTerm->fd = fd;
	
	if(gwtGetTerminalSize()) goto fatal;
	
	//Get current terminal settings
	if(tcgetattr(fd,&(currentTerm->originalTermios)) == -1) goto fatal;
	
	currentTerm->currentTermios = currentTerm->originalTermios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    currentTerm->currentTermios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    currentTerm->currentTermios.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    currentTerm->currentTermios.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions */
    //TODO: Remove signal generation (such as SIGINT), so we can handle it more
    //gracefully later
    currentTerm->currentTermios.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    currentTerm->currentTermios.c_cc[VMIN] = 1; currentTerm->currentTermios.c_cc[VTIME] = 0; /* 1 byte, no timer */
	
	//Allocate buffer and store variables
	currentTerm->bufferSize = sizeof(char) * 4097;
	currentTerm->buffer = malloc(currentTerm->bufferSize);
	memset(currentTerm->buffer, 0, currentTerm->bufferSize);
	currentTerm->currentPositionInBuffer = 0;
	
	gwtInstallSignalHandlers();
	
	//Set what happens at exit
	atexit(gwtAtExit);
	
	return 0;
	
fatal:
	if(currentTerm) free(currentTerm);
	return -1;
}

//Wraps to the end of the previous row by going to the begging of the previous
//row then going to the end
void gwtEndOfPreviousRow(){
	write(currentTerm->fd, "\x1b[F", 3);
	char temp[8];
	sprintf(temp, "\x1b[%dG", currentTerm->columns);
	write(currentTerm->fd, temp, strlen(temp));
	currentTerm->currentRow--;
	currentTerm->currentColumn = currentTerm->columns;
}

//Goes to the beginning of the previous row
//Don't need to add a row, as the only way go to a row that doesn't exist is to
//write to it
void gwtStartOfNextRow(){
	write(currentTerm->fd, "\x1b[E", 3);
	currentTerm->currentRow++;
	currentTerm->currentColumn = 1;
}

//Go back a single cell, wrapping around to the previous row if needed
//Don't go back into the prompt
void gwtBackOneCell(){
	//if(currentTerm->currentColumn == 0) gwtEndOfPreviousRow();
	if(currentTerm->currentColumn == 1) gwtEndOfPreviousRow();
	else if(currentTerm->currentPositionInBuffer == 0) return;
	else{
		write(currentTerm->fd, "\x1b[D", 3);
		currentTerm->currentColumn--;
	}
	currentTerm->currentPositionInBuffer--;
}

//Go forward a single cell, wrapping to the next row if needed
//Don't go past the end of the buffer
void gwtForwardOneCell(){
	if(currentTerm->currentPositionInBuffer == currentTerm->endPosition) return;
	else if(currentTerm->currentColumn == currentTerm->columns) gwtStartOfNextRow();
	else{
		write(currentTerm->fd, "\x1b[C", 3);
		currentTerm->currentColumn++;
	}
	currentTerm->currentPositionInBuffer++;
}

void gwtHideCursos(){
	write(currentTerm->fd, "\x1b[?25l", 6);
}

void gwtShowCursos(){
	write(currentTerm->fd, "\x1b[?25h", 6);
}

void gwtSaveCursor(){
	write(currentTerm->fd, "\x1b[s", 3);
}

void gwtRestoreCursor(){
	write(currentTerm->fd, "\x1b[u", 3);
}

//Write the value of the buffer from the current position. Simpler when deleting a character
//Could also be used when doing history stuff
void gwtPrintFromCurrentLocation(){
	write(currentTerm->fd, &currentTerm->buffer[currentTerm->currentPositionInBuffer], currentTerm->endPosition-currentTerm->currentPositionInBuffer);
}

//Gets the position the cursor is currently at
void gwtGetCursorPosition(){
	char buf[32];
	write(currentTerm->fd, "\x1b[6n", 4);
	
	uint8_t i = 0;
	while(1){
		read(currentTerm->fd, &buf[i], 1);
		if(buf[i] == 'R')break;
		i++;
	}
	buf[i] = 0;
	sscanf(buf, "\x1b[%hu;%hu", &currentTerm->currentRow, &currentTerm->currentColumn);
}

//Reset information in the terminal information structure
void gwtReset(const char* prompt){
	gwtSetNewTermios();
	if(currentTerm->currentPrompt) free(currentTerm->currentPrompt);
	currentTerm->currentPromptSize = strlen(prompt);
	currentTerm->currentPrompt = malloc(sizeof(char) * (currentTerm->currentPromptSize + 1));
	strcpy(currentTerm->currentPrompt, prompt);
	write(currentTerm->fd, currentTerm->currentPrompt, currentTerm->currentPromptSize);
	
	gwtClearBuffer();
	gwtGetCursorPosition();
	currentTerm->promptRow = currentTerm->currentRow;
}

//Create a new line in the terminal. Useful for when Enter is pressed or text
//wraps to a new line
void gwtNewline(){
	if(currentTerm->currentRow == currentTerm->rows){
		//If we are at the bottom of the terminal, just add a new row
		write(currentTerm->fd, "\r\n", 2);
		currentTerm->promptRow--;
	} else{
		//Otherwise, move the cursor down a row
		write(currentTerm->fd, "\x1b[E", 3);
		currentTerm->currentRow++;
	}
	currentTerm->currentColumn = 1;
}

//Add a character to the buffer
void gwtAddCharacter(char c){
	if(currentTerm->endPosition == currentTerm->currentPositionInBuffer){
		currentTerm->buffer[currentTerm->currentPositionInBuffer] = c;
		currentTerm->currentPositionInBuffer++;
		write(currentTerm->fd, &c, 1);
		if(currentTerm->currentColumn == currentTerm->columns) gwtNewline();
		else currentTerm->currentColumn++;
		currentTerm->endPosition++;
	} else{
		gwtAddToBuffer(currentTerm->currentPositionInBuffer, 1);
		currentTerm->buffer[currentTerm->currentPositionInBuffer] = c;
		gwtSaveCursor();
		gwtPrintFromCurrentLocation();
		gwtRestoreCursor();
		gwtForwardOneCell();
		
		//This is to handle a major hack where inserting into the middle of the buffer
		//causes the system to create a new row at the bottom
		uint16_t math = (currentTerm->endPosition+currentTerm->currentPromptSize)%currentTerm->columns;
		if(math==1 && currentTerm->currentRow==currentTerm->rows){
			write(currentTerm->fd, "\x1b[A", 3);
			gwtGetCursorPosition();
			currentTerm->promptRow--;
		}
	}
}

//Deletes the current cell. Used at the end of the buffer, as if it is in
//the middle of the buffer, just rewrite the rest of the buffer
void gwtClearCurrentCell(){
	gwtSaveCursor();
	write(currentTerm->fd, " ", 1);
	gwtRestoreCursor();
}

void gwtDeleteCurrentCell(){
	//Can use save/restore here, as we keep the empty last row if it
	//happens to become blank
	gwtSaveCursor();
	gwtRemoveFromBuffer(currentTerm->currentPositionInBuffer, 1);
	gwtPrintFromCurrentLocation();
	write(currentTerm->fd, " ", 1);
	gwtRestoreCursor();
}

//Deletes a character from the stream, handling correct reprinting if needed
//TODO: Clean this up, as there is a lot of copied code
void gwtDelete(char backwards){
	if(currentTerm->currentPositionInBuffer == 0 && backwards) return;
	else if(currentTerm->endPosition == currentTerm->currentPositionInBuffer && !backwards) return;
	
	if(backwards){
		//If it is at the end of the buffer, just remove the last item and
		//delete the last cell. Otherwise, go back one spot, move the buffer
		//forward one, reprint the rest of the buffer, and remove the last item
		if(currentTerm->endPosition == currentTerm->currentPositionInBuffer){
			currentTerm->endPosition--;
			currentTerm->buffer[currentTerm->endPosition] = 0;
			gwtBackOneCell();
			gwtClearCurrentCell();
		} else{
			gwtBackOneCell();
			gwtDeleteCurrentCell();
		}
	} else if(!backwards){
		//quick hack if this is the last item in the buffer, we don't need to do much
		if(currentTerm->currentPositionInBuffer==(currentTerm->endPosition-1)){
			gwtClearCurrentCell();
			currentTerm->endPosition--;
			currentTerm->buffer[currentTerm->endPosition] = 0;
		} else{
			gwtDeleteCurrentCell();
		}
	}
}

void gwtHome(){
	char buf[16];
	sprintf(buf, "\x1b[%d;%dH", currentTerm->promptRow, currentTerm->currentPromptSize+1);
	write(currentTerm->fd, buf, strlen(buf));
	gwtGetCursorPosition();
	currentTerm->currentPositionInBuffer=0;
}

void gwtEnd(){
	uint16_t numChars = currentTerm->currentPromptSize + currentTerm->endPosition+1;
	//Add 1 to currentTerm->columns because when numChars == currentTerm, we still want
	//to display on the same row
	uint16_t numCols = numChars % (currentTerm->columns+1);
	uint16_t numRows = (numChars - numCols)/currentTerm->columns;
	if(numRows > 0) numCols++;
	char buf[16];
	sprintf(buf, "\x1b[%d;%dH", currentTerm->promptRow+numRows, numCols);
	write(currentTerm->fd, buf, strlen(buf));
	currentTerm->currentRow = currentTerm->promptRow+numRows;
	currentTerm->currentColumn = numCols;
	currentTerm->currentPositionInBuffer = currentTerm->endPosition;
}

void gwtHandleVT220(char c){
	switch(c){
		case '1':
			gwtHome();
			break;
		case '3':
			gwtDelete(0);
			break;
		case '4':
			gwtEnd();
			break;
	}
}

//Handle escape characters
//TODO: Add the rest of them, especially xterm/VT100 commands
void gwtHandleEscapeCharacter(){
	char buf[16];
	memset(buf, 0, 16);
	uint8_t i = 0;
	read(currentTerm->fd, &buf[i], 1);
	read(currentTerm->fd, &buf[++i], 1);
	if(buf[0] == '['){
		while(buf[i] >= '0' && buf[i] <= '9') read(currentTerm->fd, &buf[++i], 1);
		switch(buf[i]){
			case 'C':
				gwtForwardOneCell();
				break;
			case 'D':
				gwtBackOneCell();
				break;
			case 'H':
				gwtHome();
				break;
			case 'F':
				gwtEnd();
				break;
			case '~':
				gwtHandleVT220(buf[i-1]);
				break;
		}
	}
	return;
}

//Read from the buffer
//TODO: Add the rest of the cases that aren't from Escape keys
//TODO: Tab completion
char* gwtTermRead(const char* prompt){
	if(currentTerm == NULL){
		if(gwtInitialize(STDIN_FILENO)) return NULL;
	}
	gwtReset(prompt);
	
	while(1){
		char buf[8];
		if(read(currentTerm->fd, buf, 1) <= 0) return NULL;
		switch(buf[0]){
			case KEY_CTRL_A:
				gwtHome();
				break;
			case KEY_CTRL_E:
				gwtEnd();
				break;
			case KEY_TAB:
				//implement tab completion
				break;
			case KEY_ENTER:
				gwtSetOldTermios();
				gwtNewline();
				return strdup(currentTerm->buffer);
			case KEY_BACKSPACE:
				gwtDelete(1);
				break;
			case KEY_ESCAPE:
				gwtHandleEscapeCharacter();
				break;
			default:
				gwtAddCharacter(buf[0]);
				break;
		}
	}
	
	gwtSetOldTermios();
	return NULL;
}

void gwtResetLineState(){
	if(currentTerm==NULL) return;
	gwtEnd();
	gwtNewline();
	write(currentTerm->fd, currentTerm->currentPrompt, currentTerm->currentPromptSize);
	gwtClearBuffer();
	gwtGetCursorPosition();
	currentTerm->promptRow = currentTerm->currentRow;
}
