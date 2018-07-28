/*Compile with- gcc perf_test.c -o PERF_TEST
 * Run- sudo ./PERF_TEST
 */

#include "perThread_perf.h"
#include <stdbool.h>
#define PRINT true

uint64_t EVENT[8];

struct perf_stat *threads = NULL;

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd,
                            unsigned long flags)
{
    int ret;
    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    return ret;
}

void setPerfAttr(struct perf_event_attr pea, uint64_t EVENT, int group_fd, int *fd, uint64_t *id,
                 int cpu, pid_t tid)
{

    memset(&pea, 0, sizeof(struct perf_event_attr)); // allocating memory
    pea.type = PERF_TYPE_RAW;
    pea.size = sizeof(struct perf_event_attr);
    pea.config = EVENT;
    pea.disabled = 1;
    // pea[cpu].exclude_kernel=1;
    // pea[cpu].exclude_hv=1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    (*fd) = perf_event_open(&pea, tid, cpu, group_fd,
                            0); // group leader has group id -1
    if ((*fd) == -1)  //Don't start orstopt read events on this
        printf("Error! perf_event_open not set for TID:%d for event:%" PRIu64 "\n", tid, EVENT);
     
     if((*fd)!=-1)  
     ioctl((*fd), PERF_EVENT_IOC_ID, id); // retrieve identifier for first counter
}

void start_event(int fd)
{
    if(fd!=-1)	
    { ioctl(fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
     ioctl(fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
     }
}

void stop_read_counters(struct read_format *rf, int fd, char *buf, int size, uint64_t *val1,
                        uint64_t *val2, uint64_t id1, uint64_t id2)
{
   if(fd!=-1)	
   { 
     ioctl(fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
     read(fd, buf, size);

    uint64_t i;
    // read counter values
       for (i = 0; i < (rf->nr); i++) {
        if (rf->values[i].id == id1) (*val1) = rf->values[i].value;

        if (rf->values[i].id == id2) (*val2) = rf->values[i].value;
         }

   }
   else //file descriptor was -1, hence no monitoring happened
     {  //set these unmonitored event counts to 0
          (*val1)=0;
	  (*val2)=0;

       }	     
    close(fd);
}

void initialize_events(void)
{

    EVENT[0] = 0x3c;   // UNHALTED_CORE_CYCLES
    EVENT[1] = 0xc0;   // INSTRUCTIONS_RETIRED
    EVENT[2] = 0x10d3; // REMOTE_HITM
    EVENT[3] = 0x04d3; // REMOTE_DRAM
    EVENT[4] = 0x412e; // LLC_MISSES
    EVENT[5] = 0x10d1; // L2_MISSES
    EVENT[6] = 0x04d1; // L3_MISSES
    EVENT[7] = 0x20d1; // L3_HITS
}

void count_event_perfMultiplex(pid_t tid[], int index_tid)
{
    // Initialize time interval to count
    int millisec = TIME_IN_MILLI_SEC;
    struct timespec tim;
    tim.tv_sec = millisec / 1000;
    tim.tv_nsec = (millisec % 1000) * 1000000;

    threads = (struct perf_stat *)malloc(index_tid * sizeof(struct perf_stat));

    // iterate through all threads
    int i;
    for (i = 0; i < index_tid; i++) {
        // initialize read format descriptor for each thread and all events for that
        // thread
        int j;
        for (j = 0; j < (EVENTS / 2); j++)
            threads[i].rf[j] = (struct read_format *)(threads[i].buf[j]);

        // set up performance counters
        for (j = 0; j < (EVENTS / 2); j++) {
            setPerfAttr(threads[i].pea, EVENT[j * 2], -1, &(threads[i].fd[(j * 2)]),
                        &(threads[i].id[(j * 2)]), -1,
                        tid[i]); // measure tid statistics on any cpu
            setPerfAttr(threads[i].pea, EVENT[j * 2 + 1], threads[i].fd[(j * 2)],
                        &(threads[i].fd[(j * 2) + 1]), &(threads[i].id[(j * 2) + 1]), -1, tid[i]);
        }
    }

    int j;
    for (j = 0; j < (EVENTS / 2); j++) {

        // Start counting events for event atrribute one
        for (i = 0; i < index_tid; i++)
            start_event(threads[i].fd[j * 2]);

        // duration of count
        nanosleep(&tim, NULL);

        // stop counters and read counter values
        for (i = 0; i < index_tid; i++) {
            int size = sizeof(threads[i].buf[j]);
            stop_read_counters((threads[i].rf[j]), threads[i].fd[j * 2], threads[i].buf[j], size,
                               &(threads[i].val[(2 * j)]), &(threads[i].val[(2 * j) + 1]),
                               threads[i].id[(j * 2)], threads[i].id[(j * 2) + 1]);
        }
    } // for j close

    /*
         EVENT[0]=0x3c;           //UNHALTED_CORE_CYCLES
         EVENT[1]=0xc0;           //INSTRUCTIONS_RETIRED
         EVENT[2]=0x10d3;         //REMOTE_HITM
         EVENT[3]=0x04d3;          //REMOTE_DRAM
         EVENT[4]=0x412e;          //LLC_MISSES
         EVENT[5]=0x10d1;         //L2_MISSES
         EVENT[6]=0x04d1;         //L3_MISSES
         EVENT[7]=0x20d1;          //L3_HITS
    */
}

void displayTIDEvents(pid_t tid[], int index_tid)
{
    printf("CountEvents Index:%d\n", index_tid);

    int i;
    for (i = 0; i < index_tid; i++) {
        THREADS.tid[i] = tid[i];
        THREADS.index_tid = index_tid;

        int j;
        for (j = 0; j < (EVENTS); j++)
            THREADS.event[i][j] = (threads[i].val[j] * 4);

        if (PRINT) {
            printf("\n");
            printf("THREAD:%d UNHALTED_CORE_CYCLE: %" PRIu64 "\n", tid[i], (threads[i].val[0]) * 4);
            printf("THREAD: %d INSTRUCTION_RETIRED: %" PRIu64 "\n", tid[i],
                   (threads[i].val[1]) * 4);
            printf("\n");
            printf("\n");
            printf("THREAD: %d REMOTE_HITM: %" PRIu64 "\n", tid[i], (threads[i].val[2]) * 4);
            printf("THREAD: %d REMOTE_DRAM: %" PRIu64 "\n", tid[i], (threads[i].val[3]) * 4);
            printf("\n");
            printf("\n");
            printf("THREAD: %d LLC_MISSES: %" PRIu64 "\n", tid[i], (threads[i].val[4]) * 4);
            printf("THREAD: %d L2_MISSES: %" PRIu64 "\n", tid[i], (threads[i].val[5]) * 4);
            printf("\n");
            printf("\n");
            printf("THREAD: %d L3_MISSES: %" PRIu64 "\n", tid[i], (threads[i].val[6]) * 4);
            printf("THREAD: %d L3_HITS: %" PRIu64 "\n", tid[i], (threads[i].val[7]) * 4);
            printf("\n");

            printf("-----------------------------------------------------------------"
                   "--------\n");
        }
    } // for close

    free(threads);
    // printf("=================================================================\n");
}

int searchTID(int tid)
{
    int i = 0;
    for (i = 0; i < THREADS.index_tid; i++) {
        if ((int)THREADS.tid[i] == tid) return i;
    }

    return -1;
}

void copyValues(pid_t tid[], int index_tid)
{

    int i;
    for (i = 0; i < index_tid; i++) {
        THREADS.tid[i] = tid[i];
        THREADS.index_tid = index_tid;

        int j;
        for (j = 0; j < (EVENTS); j++)
            THREADS.event[i][j] = (threads[i].val[j]) * 4;

        /*	//FIND PerfData Instance of this particular TID and then populate
           options_t struct of that THREADS.tid[i]; //TID of the THREAD BEING
           MONITORED THREADS.event[i][0];   //UNHALTED_CORE_CYCLES
                THREADS.event[i][1];    //INSTRUCTIONS_RETIRED
                 THREADS.event[i][2];    //REMOTE_HITM
                THREADS.event[i][3];       //REMOTE_DRAM
                 THREADS.event[i][4];      //LLC_MISSES
                THREADS.event[i][5];       //L2_MISSES
                 THREADS.event[i][6];    //L3_MISSES
                THREADS.event[i][7];      //L3_HITS
            */
    }
}