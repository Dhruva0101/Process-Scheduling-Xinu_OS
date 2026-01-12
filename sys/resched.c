/* resched.c  -  resched */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sched.h>
#include <stdio.h>
#include <math.h>

unsigned long currSP;	/* REAL sp of current process */
extern int ctxsw(int, int, int, int);
/*-----------------------------------------------------------------------
 * resched  --  reschedule processor to highest priority ready process
 *
 * Notes:	Upon entry, currpid gives current process id.
 *		Proctab[currpid].pstate gives correct NEXT state for
 *			current process if other than PRREADY.
 *------------------------------------------------------------------------
 */

static int all_processes_goodness_zero() {
	for (int i = 0; i < NPROC; i++) {
        if (proctab[i].pstate != PRFREE && proctab[i].goodness > 0) {
            return 0;
        }
    }
    return 1;
}

int resched()
{
	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */

	int sched = getschedclass();    

	/* no switch needed if current process priority higher than next */
	if (sched != EXPDISTSCHED && sched != LINUXSCHED ) {
		if (((optr = &proctab[currpid])->pstate == PRCURR) &&
			(lastkey(rdytail) < optr->pprio)) {
			return OK;
		}
	} else {
		optr = &proctab[currpid];
	}
		
	/* force context switch */

	if (optr->pstate == PRCURR) {
		optr->pstate = PRREADY;
		insert(currpid,rdyhead,optr->pprio);
	}

// ---------------EXPDISTSCHED---------------

	if(getschedclass()== EXPDISTSCHED){

		double exprand = expdev(0.1);    // generate a exponential random number 
		int pid = firstid(rdyhead); 	 // first process of ready queue - lowest priority

		// traverse entire queue low -> high priority and keep moving while process priority <= our random number
		while(pid != rdytail && proctab[pid].pprio <= exprand) {
			pid = q[pid].qnext;          // go to next process in queue
		}

		if (pid == rdytail) {
			// pick highest priority process if we traversed entire queue bcz exprand is greater that all priorities
			pid = getlast(rdytail);
		}

		else{	
			// handingly processes with same priority
			int p = pid;
			// traverse and point to last process of equal priorities
			while (q[p].qnext !=rdytail && proctab[q[p].qnext].pprio == proctab[p].pprio){
				p =q[p].qnext;			// move to next one with same priority
			}

			pid = p ;               	// pick that process
			dequeue(pid);  				// remove it from ready queue to run it
			
		}
		
		// set the chosen process as the current running process
		currpid =pid;
		nptr = &proctab[currpid];		// nptr points to its process table entry

	}

// ---------------LINUXSCHED---------------

	else if (getschedclass() == LINUXSCHED) {

		// save remaining quantum and update goodness for current process
		if (currpid != NULLPROC) {
			proctab[currpid].remainingquantum = preempt;   // save leftover time

			if (preempt == 0){
				proctab[currpid].goodness = 0;			   // process used its entire quantum
			}

			else{
			// goodness = priority + remaining time
				proctab[currpid].goodness = proctab[currpid].pprio + preempt;   
			}
		}

		// check if all processes exhausted i.e. goodness = 0 then start new epoch 
		if (all_processes_goodness_zero()) {

			// For every valid - non-free, non-null process
			for (int pid = 0; pid < NPROC; pid++) {

				if (proctab[pid].pstate != PRFREE && pid != NULLPROC) {

					// assigning new time quantum for next epoch
					if (proctab[pid].remainingquantum == 0)	{
					// process finished last epoch then new quantum = priority
						proctab[pid].quantum = proctab[pid].pprio;
					}

					else {
					// process still had some time but gave up then gets bonus - half leftover
						proctab[pid].quantum = proctab[pid].pprio + (proctab[pid].remainingquantum / 2);
					}

					// reset remaining time and recompute goodness
					proctab[pid].remainingquantum = proctab[pid].quantum;
					proctab[pid].goodness = proctab[pid].pprio + proctab[pid].remainingquantum;
				}
			}
		}

		// find process with highest goodness
		int next_pid = NULLPROC;
		int max_goodness = 0;

		for (int pid = 0; pid < NPROC; pid++) {

			if (proctab[pid].pstate == PRREADY) {
    			if (proctab[pid].goodness > max_goodness || (proctab[pid].goodness == max_goodness && pid > next_pid)) {
					max_goodness = proctab[pid].goodness;
					next_pid = pid;
    			}
			}
		}

		// choose process 
		if (next_pid == NULLPROC) {
		// no ready processes found then run null process
			currpid = NULLPROC;
		} 

		else {
			// remove the chosen process from the ready queue and run it
			dequeue(next_pid);
			currpid = next_pid; //priority with highest goodness
		}

		// update current process pointer
		nptr = &proctab[currpid];
	}
	else {

		/* remove highest priority process at end of ready list */
		nptr = &proctab[ (currpid = getlast(rdytail)) ];
	}

nptr->pstate = PRCURR;		/* mark it currently running	*/

#ifdef	RTCLOCK

if (sched == LINUXSCHED){
	preempt = proctab[currpid].remainingquantum;
}
else{
	preempt = QUANTUM;		/* reset preemption counter	*/
}
	
#endif
	// perform the context switch: save old process, restore new
	ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	
	/* The OLD process returns here when resumed. */
	return OK;
}
