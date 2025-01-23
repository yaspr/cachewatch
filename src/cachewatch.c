/*
  Author: yaspr - 2020
  
  CacheWatch - A watchdog for monitoring Last Level Cache traffic

  --start initiates a watchdog process that collects two hardware events using the
  perf_event_open system call.

  Collected hardware events:
  --------------------------
  
      PERF_COUNT_HW_CACHE_REFERENCES
      PERF_COUNT_HW_CACHE_MISSES

  --stop sends a signal to the watchdog process to dump the recorded values and stop the collection.
*/

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

#define MAX_BUFFER 4096

//PID file (hidden in local directory)
#define PID_FILE_PATH "./.cachewatch.pid"

//Counter output file
#define OUT_FILE_PATH     "cachewatch.out"
#define OLD_OUT_FILE_PATH "cachewatch.out.old"

struct read_format {

  unsigned long long nr;

  struct {
    
    unsigned long long value;
    unsigned long long id;
    
  } values[2];
};

int perf_refs_fd;
int perf_miss_fd;
long long perf_refs_count;
long long perf_miss_count;
unsigned long long perf_refs_id;
unsigned long long perf_miss_id;
struct perf_event_attr perf_event;

int copy_file(char *src, char *dst)
{
  //PID file doesn't exist ==> return 0
  if (access(OUT_FILE_PATH, F_OK) < 0)
    return 0;

  char rb[MAX_BUFFER];
  
  int fsrc = open(src, O_RDONLY);

  int fdst_flags = O_CREAT | O_WRONLY | O_TRUNC;
  int fdst_perms = S_IRUSR | S_IWUSR | S_IRGRP |
                   S_IWGRP | S_IROTH | S_IWOTH;
  
  int nbbytes;
  int fdst = open(dst, fdst_flags, fdst_perms);
  
  if (fdst != -1)
    {
      while((nbbytes = read(fsrc, rb, MAX_BUFFER)) > 0)
	write(fdst, rb, nbbytes);

      nbbytes = 1;
    }
  else
    nbbytes = 0;
  
  close(fsrc);
  close(fdst);

  return nbbytes;
}

//Wrapper for the perf_event_open system call
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
			    int cpu, int group_fd, unsigned long flags)
{
  return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);  
}

//Checks the PID file
pid_t cachewatch_check_pid_file()
{
  //PID file doesn't exist ==> return 0
  if (access(PID_FILE_PATH, F_OK) < 0)
    return 0;
  
  //PID file exists ==> open
  FILE *fp = fopen(PID_FILE_PATH, "rb");
  
  //Read content
  if (fp)
    {
      pid_t pid;
      
      //Read PID
      fscanf(fp, "%d", &pid);
      
      fclose(fp);

      return pid;
    }
  else
    return 0;
}

void start_perf_collection()
{  
  memset(&perf_event, 0, sizeof(struct perf_event_attr));
  
  //==> Set up the cache references counter
  perf_event.size           = sizeof(struct perf_event_attr);

  //The references counter!
  perf_event.type           = PERF_TYPE_HARDWARE;
  perf_event.config         = PERF_COUNT_HW_CACHE_REFERENCES;
  
  perf_event.disabled       = 1;
  perf_event.exclude_hv     = 1;
  perf_event.exclude_kernel = 1;
  
  perf_event.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  
  perf_refs_fd = perf_event_open(&perf_event, 0, -1, -1, 0);
  
  if (perf_refs_fd < 0)
    {
      printf("[START] (error) failed perf_event_open\n");
      exit(5);
    }
  
  //Get event ID
  ioctl(perf_refs_fd, PERF_EVENT_IOC_ID, &perf_refs_id);
  
  //==> Set up the cache misses counter
  perf_event.size = sizeof(struct perf_event_attr);
  
  //The misses counter!
  perf_event.type   = PERF_TYPE_HARDWARE;
  perf_event.config = PERF_COUNT_HW_CACHE_MISSES;
  
  perf_event.disabled       = 1;
  perf_event.exclude_hv     = 1;
  perf_event.exclude_kernel = 1;
  
  perf_event.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  
  //Cache references counter is group leader
  perf_miss_fd = perf_event_open(&perf_event, 0, -1, perf_refs_fd, 0);  
  
  if (perf_miss_fd < 0)
    {
      printf("[START] (error) failed perf_event_open\n");
      exit(7);
    }
  
  ioctl(perf_miss_fd, PERF_EVENT_IOC_ID, &perf_miss_id);
  
  //Reset and enable counters
  ioctl(perf_refs_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  ioctl(perf_refs_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  
  //Keep the watchdog hanging
  while(1)
    ;
}

//
void cachewatch_stop_handler()
{
  //Read buffer
  char rb[MAX_BUFFER];
  
  //Read format (overlap pointers)
  struct read_format *rf = (struct read_format*)rb;
  
  //Stop collection
  ioctl(perf_refs_fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  
  //Read collected counters
  read(perf_refs_fd, rb, sizeof(rb));
  
  close(perf_refs_fd);
  close(perf_miss_fd);

  //If output file exists move to cachewatch.out.old
  copy_file(OUT_FILE_PATH, OLD_OUT_FILE_PATH);
  
  //Create output file
  FILE *fp = fopen(OUT_FILE_PATH, "wb");
  
  if (fp)
    {
      //Write counter values to file
      for (unsigned long long i = 0; i < rf->nr; i++)
	{
	  if (rf->values[i].id == perf_refs_id)
	    {
	      perf_refs_count = rf->values[i].value;
	      
	      fprintf(fp, "refs: %llu\n", perf_refs_count);
	    }
	  else
	    if (rf->values[i].id == perf_miss_id)
	      {
		perf_miss_count = rf->values[i].value;
		
		fprintf(fp, "miss: %llu\n", perf_miss_count);
	      }
	}
      
      fprintf(fp, "hits: %llu\n", perf_refs_count - perf_miss_count);
      
      double mratio = (double)perf_miss_count * 100.0 / (double)perf_refs_count;
      
      fprintf(fp, "m ratio: %lf %%\n", mratio);
      fprintf(fp, "h ratio: %lf %%\n", 100 - mratio);
      
      fclose(fp);
      
      //Remove the watchdog PID file before exit
      if (unlink(PID_FILE_PATH) < 0)
	{
	  printf("[STOP] (error) cannot delete watchdog PID file '%s'\n", PID_FILE_PATH);
	  exit(4);
	}
      
      printf("[STOP] PID [%d] stopped\n", getpid());
      exit(0);
    }
  else
    {
      printf("[STOP] (error) cannot create output file '%s'\n", OUT_FILE_PATH);
      exit(1);
    }
}

void cachewatch_start(unsigned core)
{
  //If file exists, then error!
  if (cachewatch_check_pid_file())
    {
      printf("[START] (error) watchdog already running PID file found '%s'\n", PID_FILE_PATH);
      exit(2);
    }
  
  //Create a process for the watchdog
  pid_t pid_wd = fork();
  
  //If child
  if (pid_wd == 0)
    {
      cpu_set_t mask;
      
      CPU_ZERO(&mask);
      CPU_SET(core, &mask);
      sched_setaffinity(0, sizeof(mask), &mask);
      
      //Create a new session -- daemonize the child
      setsid();
      
      //Register the signal to wait for
      signal(SIGUSR1, cachewatch_stop_handler);
      
      //Run watchdog
      start_perf_collection();
    }
  else //If self (parent)
    {
      //Create PID file
      FILE *fp = fopen(PID_FILE_PATH, "wb");
      
      //Write PID
      fprintf(fp, "%d\n", pid_wd);
      
      //Close the file
      fclose(fp);
      
      printf("[START] watchdog [%d] started\n", pid_wd);
      exit(0);
    }
}

void cachewatch_stop()
{
  pid_t pid;
  
  //If file exists get the PID
  if ((pid = cachewatch_check_pid_file()))
    {
      //Send signal to the watchdog process
      if (kill(pid, SIGUSR1) < 0)
	{
	  printf("[STOP] (error) cannot send signal to watchdog [%d]\n", pid);
	  exit(5);
	}
    }
  else
    {
      printf("[STOP] (error) cannot find watchdog PID file '%s'\n", PID_FILE_PATH);
      exit(3);
    }
}

int main(int argc, char **argv)
{
  if (argc < 2)
    return printf("Usage: %s --start [core] or --stop\n", argv[0]), 2;
  
  if (!strncmp(argv[1], "--start", 7) || !strncmp(argv[1], "--summon", 8))
    {
      //Default watchdog core number
      unsigned core = 0;
      
      //If core number is set 
      if (argc == 3)
	core = atol(argv[2]);
      
      cachewatch_start(core);
    }
  else
    if (!strncmp(argv[1], "--stop", 6))
      cachewatch_stop();
    else
      {
	printf("Usage: unknown parameter\n");
	exit(6);
      }
  
  return 0;
}
