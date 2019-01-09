#include <fcntl.h>
#include <unistd.h>

#include "mdr.h"

MdrJournal mdrj;

static FILE* fh;

static void
__attribute__ ((destructor))
finish (void)
{
    fh = fopen ("finstr.txt", "w");
    if (fh != NULL)
    {
        mdrj.flush(fh);
        fclose (fh);
        fh = NULL;
    }
}


extern "C" void
__attribute__((no_instrument_function))
__cyg_profile_func_enter (void *this_fn, void *call_site)
{
  mdrj.call_begin(this_fn);
}

extern "C"  void
__attribute__((no_instrument_function))
__cyg_profile_func_exit (void *this_fn, void *call_site)
{
  mdrj.call_end(this_fn);
}
