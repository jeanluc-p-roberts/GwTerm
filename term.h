#ifndef _GWS_TERM_H_
#define _GWS_TERM_H_

#include <termios.h>
#include <unistd.h>
#include <stdint.h>

typedef struct{
	uint32_t fd;
	struct termios originalTermios;
	struct termios currentTermios;
	char* buffer;
	uint16_t bufferSize, currentPositionInBuffer, endPosition;
	char* currentPrompt;
	uint16_t currentPromptSize;
	uint16_t rows, columns, currentRow, currentColumn;
	uint16_t promptRow;
} TermInfo;

extern uint8_t gwtHandleSignals;
extern uint8_t gwtHandleSigInt, gwtHandleSigQuit, gwtHandleSigTerm, gwtHandleSigHup, gwtHandleSigAlrm, gwtHandleSigTstp;

void gwtInstallSignalHandlers();
void gwtRemoveSignalHandlers();
void gwtResetLineState();
char* gwtTermRead(const char* prompt);
void gwtWriteSignal(int sig);
void gwtCleanupAfterSignal();

#endif