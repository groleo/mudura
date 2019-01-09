#include "mdr.h"
#include <unistd.h>

#define EGL_SWAP_BEGIN 1
#define EGL_SWAP_END 2

int main()
{

    MdrJournal mdrj;

    mdrj.call_begin((void*)main);
    mdrj.event_add(EGL_SWAP_BEGIN);
    fprintf(stderr, "zzzz\n");
    mdrj.event_add(EGL_SWAP_END);


    mdrj.event_add(EGL_SWAP_BEGIN);
    mdrj.event_add(EGL_SWAP_END);
    mdrj.event_add(EGL_SWAP_BEGIN);
    mdrj.event_add(EGL_SWAP_END);
    mdrj.event_add(EGL_SWAP_BEGIN);
    mdrj.event_add(EGL_SWAP_END);
    mdrj.call_end((void*)main);
    mdrj.flush(STDOUT_FILENO);
}
