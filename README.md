# GwTerm
Terminal Library for Linux

## Purpose
This is designed as a library to handle input/output for a TTY device on Linux.

## Compiling
Use `make` to compile this library, which will create a static library that is easy to include. GwTerm does not currently support being compiled as a shared library, but that may change in the future.

## Usage
First step is to include the `term.h` header. Then, all that is needed is to call the function `char* gwtTermRead(const char* prompt)`. This will return the line read after the Enter key is pressed in the terminal, or NULL if there is an error or a stopping signal. Before getting input, it will display the value passed in the `const char* prompt` variable.

To compile against it, just add the libgwterm.a static library.

## Signals
This library can also handle the following signals:

* SIGINT
* SIGQUIT
* SIGTERM
* SIGHUP
* SIGALRM
* SIGTSTP

For SIGINT, it will display the "^C" character, then move down a row from the last used row and redisplay the prompt used for the current `gwtTermRead` call.

For the other signals, it will reset the terminal settings to before the call to `gwtTermRead` and return NULL.

Signals can be disabled by setting `uint8_t gwtHandleSignals` equal to 0. This will disable all signals. To disable a single signal, keep `uint8_t gwtHandleSignals` equal to one, and set the corresponding variable equal to 0: `uint8_t gwtHandleSigInt, gwtHandleSigQuit, gwtHandleSigTerm, gwtHandleSigHup, gwtHandleSigAlrm, gwtHandleSigTstp`.

If you are manually handling signals, there are two functions that will help with this: `void gwtResetLineState()`, which will reset the line state and redisplay the prompt (used with SIGINT), and `void gwtCleanupAfterSignal()`, which will reset to before `gwtTermRead` was called.

## GNU Readline compatibility
GwTerm is designed to be mostly compatible with GNU Readline. It was developed by looking at the documentation (not source code) for GNU Readline, in order to be a mostly drop in replacement. A lot of functionality is not replicated (such as hooks, redisplay functionality, a lot of variables and utility functions), but for the basic usage it should work. Some of the functionality not implemented may be added, but may not be.

To use the GNU Readline compatability, include the `readline_compat.h` header. It uses `#define` macros to map from GNU Readline references to GwTerm.

## Todo
There is a lot to do for this. Please view each source file for what is currently to do.
