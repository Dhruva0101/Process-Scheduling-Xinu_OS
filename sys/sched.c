#include <kernel.h>
#include <sched.h>

static int sched_class = EXPDISTSCHED;   // default

void setschedclass(int sched_class_id) {
    sched_class = sched_class_id;
}

int getschedclass(void) {
    return sched_class;
}
