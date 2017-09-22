#ifndef _GWS_READLINE_COMPAT_
#define _GWS_READLINE_COMPAT_

#include "term.h"

extern int rl_gnu_readline_p;

//Variables
#define rl_catch_signals gwtHandleSignals
#define rl_line_buffer currentTerm->buffer
#define rl_point currentTerm->currentPositionInBuffer
#define rl_end currentTerm->endPosition
#define rl_prompt currentTerm->prompt

//Functions
#define readline gwtTermRead
#define rl_set_signals gwtInstallSignalHandlers
#define rl_clear_signals gwtRemoveSignalHandlers
#define rl_free_line_state gwtResetLineState
#define rl_echo_signal_char gwtWriteSignal
#define rl_cleanup_after_signal gwtCleanupAfterSignal

#endif
