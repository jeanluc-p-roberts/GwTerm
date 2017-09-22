#include "term.h"
#include <signal.h>
#include <string.h>

//TODO: Handle SIGWINCH (console resize)

extern TermInfo* currentTerm;
void gwtAtExit();
void gwtEnd();
void gwtNewline();
uint8_t gwtHandleSignals = 1, gwtSignalsInstalled = 0, gwtSignalsInit = 0;
uint8_t gwtHandleSigInt = 1, gwtHandleSigQuit = 1, gwtHandleSigTerm = 1;
uint8_t gwtHandleSigHup = 1, gwtHandleSigAlrm = 1, gwtHandleSigTstp = 1;
struct sigaction oldSigInt, oldSigQuit, oldSigTerm, oldSigHup, oldSigAlrm, oldSigTstp;

void gwtInitSignals(){
	oldSigInt.sa_handler = NULL;
	oldSigQuit.sa_handler = NULL;
	oldSigTerm.sa_handler = NULL;
	oldSigHup.sa_handler = NULL;
	oldSigAlrm.sa_handler = NULL;
	oldSigTstp.sa_handler = NULL;
	gwtSignalsInit = 1;
}

void gwtCleanupAfterSignal(){
	gwtEnd();
	gwtNewline();
	gwtAtExit();
}

void gwtSignalHandler(int sig){
	gwtWriteSignal(sig);
	if(sig == SIGINT) gwtResetLineState();
	else gwtCleanupAfterSignal();
}

int gwtInstallSignal(struct sigaction* newHandler, struct sigaction* oldHandler, int sig){
	sigemptyset(&newHandler->sa_mask);
	sigaddset(&newHandler->sa_mask, sig);
	struct sigaction tempOldHandler;
	int rv = sigaction(sig, newHandler, &tempOldHandler);
	if(rv) return rv;
	if(tempOldHandler.sa_handler != gwtSignalHandler)
		memcpy(oldHandler, &tempOldHandler, sizeof(struct sigaction));
	return 0;
}

int gwtUninstallSignal(struct sigaction* handler, int sig){
	if(handler->sa_handler == NULL) return 0;
	return sigaction(sig, handler, NULL);
}

//TODO: Add error handling and additional signals
void gwtInstallSignalHandlers(){
	if(gwtHandleSignals == 0) return;
	if(gwtSignalsInit == 0) gwtInitSignals();
	struct sigaction newHandler;
	newHandler.sa_handler = gwtSignalHandler;
	newHandler.sa_flags = SA_RESTART;
	if(gwtHandleSigInt) gwtInstallSignal(&newHandler, &oldSigInt, SIGINT);
	newHandler.sa_flags = 0;
	if(gwtHandleSigQuit) gwtInstallSignal(&newHandler, &oldSigQuit, SIGQUIT);
	if(gwtHandleSigTerm) gwtInstallSignal(&newHandler, &oldSigTerm, SIGTERM);
	if(gwtHandleSigHup) gwtInstallSignal(&newHandler, &oldSigHup, SIGHUP);
	if(gwtHandleSigAlrm) gwtInstallSignal(&newHandler, &oldSigAlrm, SIGALRM);
	if(gwtHandleSigTstp) gwtInstallSignal(&newHandler, &oldSigTstp, SIGTSTP);
	gwtSignalsInstalled = 1;
}

void gwtRemoveSignalHandlers(){
	if(gwtSignalsInstalled == 0) return;
	
	gwtUninstallSignal(&oldSigInt, SIGINT);
	gwtUninstallSignal(&oldSigQuit, SIGQUIT);
	gwtUninstallSignal(&oldSigTerm, SIGTERM);
	gwtUninstallSignal(&oldSigHup, SIGHUP);
	gwtUninstallSignal(&oldSigAlrm, SIGALRM);
	gwtUninstallSignal(&oldSigTstp, SIGTSTP);
	
	gwtSignalsInstalled = 0;
	gwtSignalsInit = 0;
}

void gwtWriteSignal(int sig){
	switch(sig){
		case SIGINT:
			write(currentTerm->fd, "^C", 2);
			break;
		case SIGQUIT:
			write(currentTerm->fd, "^\\", 2);
			break;
	}
}