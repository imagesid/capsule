

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/reader.h"
#include "../../utils/include/mymath.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"
#include "../../../ioblazer/ioblazer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"
#include "debug.h"

#ifdef __cplusplus
extern "C" {
#endif

int bl=1;
static void print_progress(size_t count, size_t max) {
    const int bar_width = 50;

    float progress = (float) count / max;
    int bar_length = progress * bar_width;
    
    for (int i = 0; i < bl; ++i) {
        printf("###");
    }
    bl++;
    if(bl == 15){
      bl=1;
    }
    

    fflush(stdout);
}

static double gettimex(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec * 1.0e9 + ts.tv_nsec;
}

int
mainx(reader_t *reader, cache_t *cache, int report_interval,
              int warmup_sec, char *ofilepath)
{
  
   
   /**
    * a = access pattern random
    * r = read ratio = 1
    * g = Inter-burst time (Avg)  / iogap
    * G = Inter-burst time pattern / iogappattern
    * u = uniform
    * B = bBufferedIO cl
    * o = dwOutstandingIOs
   */
  int argc = 17; // Number of arguments
  // int argc = 13; // Number of arguments
    char *argv[] = {
        "./ioblazer", "-a", "r", "-t", "1800", "-r", "1", "-o", "8", "-g", "10000", "-G", "u", "-B", "-d", "/mnt/new_root"
        // "./ioblazer", "-a", "r", "-r", "1", "-g", "10000", "-G", "u", "-B", "-d", "/mnt/new_root"
    };

   SYSTEMTIME sNow;
   DWORD dwStartTime, dwFinishTime;
   double dTestDuration;
   char *optString = "a:A:b:Bcd:f:Fg:G:hi:I:l:o:O:p:P:r:Rt:w:";
   unsigned i;
   int c;

   /* Performance counters */
   DWORD64 qwTotalBytesTransferred = 0;
   DWORD   dwCompletedIOs = 0;
   DWORD   dwIosAboveLatThresh = 0;
   double  dAvgLatency = 0;
   DWORD   dwMaxLatency = 0;

   fprintf(stderr, "\nIOBlazer, the ultimate IO benchmark :D\n\n");

   /*
    * Parse command line arguments.
    */

  // untuk -a = r
  opt.cIoAccessPattern = tolower('r');
  if (opt.cIoAccessPattern != 's' &&
      opt.cIoAccessPattern != 'r') {
    fprintf(stderr, "\nERROR: Illegal IO access pattern '%c'\n",
            opt.cIoAccessPattern);
    usage();
    exit(-1);
  }
  // break;

  // -t = 1800
  opt.dwTestDuration = atoi("18000000");
  // break;

  // -r = 1
  opt.dRDRatio = atof("1");
        //  break;
  
  // -o = 8
  opt.dwOutstandingIOs = atoi("8");
  if (opt.dwOutstandingIOs > MAX_OUTSTANDING_IOS) {
    opt.dwOutstandingIOs = MAX_OUTSTANDING_IOS;
  }
  // break;

  // -g = 10000
  opt.dwIOGap= atoi("10000");
        //  break;
  
  // -G = u
  opt.cIOGapPattern = 'u';
  if (opt.cIOGapPattern != 'f' && opt.cIOGapPattern != 'u') {
    fprintf(stderr, "\nERROR: Illegal inter-burst time pattern '%c'\n",
            opt.cIOGapPattern);
    usage();
    exit(-1);
  }
  // break;

  // -B
  opt.bBufferedIO = FALSE;
        //  break;

  //"-d", "/mnt/new_root"
  if (strlen("/mnt/new_root") > STR_SIZE) {
    fprintf(stderr, "Error: device/file path string too long\n");
    exit(-1);
  }
  strcpy(opt.lpDevFilePath, "/mnt/new_root");
  printf("lpFileName %s\n", opt.lpDevFilePath);
  // break;

   if (opt.cIoAccessPattern == 's' && opt.cIoSizePattern != 'f') {
      fprintf(stderr, "\nERROR: sequential access reqires fixed IO size!\n");
      exit(-1);
   }

   if (opt.bTrace) {
      fprintf(stderr, "Warning: Trace Playback mode enabled. IO Spec parameters "
                      "will be ignored.\n");
      opt.dwNumThreads = 1;
      opt.dwOutstandingIOs = 1;
   }
 
   if (opt.cOutputFormat == 'f') {
      printf("Test parameters:\n");
      // printf("\tTest duration            = %ld s\n", opt.dwTestDuration);
      printf("\tWorker threads           = %ld\n", opt.dwNumThreads);
      printf("\tFile/Device path(s)      = ");
      for (i = 0; i < opt.dwNumThreads; i++) {
#ifdef __UNIX__
         char lpPathName[STR_SIZE];
         int lastCharIdx;

         strcpy(lpPathName, opt.lpDevFilePath);
         lastCharIdx = strlen(lpPathName) - 1;
         lpPathName[lastCharIdx] += i;
         printf("%s, ", lpPathName);
#endif
#ifdef _WIN32
         if (opt.bRawDev) {
            int devNum = atoi(opt.lpDevFilePath + DEVICE_PREFIX);

            printf("%s\b%d, ", opt.lpDevFilePath, devNum + i);
         } else {
            printf("%c%s, ", opt.lpDevFilePath[0] + i, &opt.lpDevFilePath[1]);
         }
#endif
      }
      printf("\n");
      
      printf("\tIO Alignment             = %lu B\n", opt.dwIOAlignment);
      printf("\tIO access pattern        = %c\n", opt.cIoAccessPattern);
      printf("\tIO size (Avg)            = %lu B\n", opt.dwIOSize);
      printf("\tIO size pattern          = %c\n", opt.cIoSizePattern);
      printf("\tBurst size (OIOs, Avg)   = %lu\n", opt.dwOutstandingIOs);
      printf("\tBurst size pattern       = %c\n", opt.cOutstandingIOsPattern);
      printf("\tInter-burst time (Avg)   = %lu us\n", opt.dwIOGap);
      printf("\tInter-burst time pattern = %c\n", opt.cIOGapPattern);
      printf("\tRead ratio               = %.1f\n", opt.dRDRatio);
      
   }

   fprintf(stderr, "\nInitializing test...\r");

   /*
    * We don't want the partial results to go to waste should the
    * user press CTRL-C, so we install a signal handler that
    * terminates gracefully in such a case.
    */
   signal(SIGINT, CtrlCHandler);

   /*
    * Allocate the I/O buffer.  In order to perform dirct IOs the buffer
    * needs to be aligned to a memory page boundary.  This is why valloc
    * or VirtualAlloc are being used here instead of malloc.
    */
#ifdef _WIN32
   threadInfo.pszBuf = (char *) VirtualAlloc(NULL,
                                             opt.dwBufferSize,
                                             MEM_RESERVE | MEM_COMMIT,
                                             PAGE_READWRITE);
#endif
#ifdef __UNIX__
   threadInfo.pszBuf = valloc(opt.dwBufferSize);
#endif
   if (threadInfo.pszBuf == NULL) {
      fprintf(stderr, "Cannot allocate memory for buffer\n");
      exit(-1);
   }

   /*
    * Fill up the buffer with random garbage.  Write the page
    * number in the first word of each page to prevent ESX
    * page sharing from deallocating memory under our feet
    */
   for (i = 0; i < MEM_PAGE_SIZE; i++) {
      threadInfo.pszBuf[i] = rand();
   }
   for (i = 1; i < MEM_BUFFER_PAGES; i++) {
      memcpy(threadInfo.pszBuf + i * MEM_PAGE_SIZE, threadInfo.pszBuf,
             MEM_PAGE_SIZE);
      threadInfo.pszBuf[i * MEM_PAGE_SIZE] = i;
   }

   /*
    * The test starts here
    */
   DEBUG_MSG("Creating worker threads\n");
   
   
   createWorkerThreads(reader, cache);

   DEBUG_MSG( "Waiting for worker threads initialize\n");
   synchWorkerThreads();
   DEBUG_MSG( "All worker threads initialized\n");

   /* The test has begun: record start time */
   GetSystemTime(&sNow);
   dwStartTime = convSysTimeToMillisec(&sNow);

   if (opt.bTrace) {
      fprintf(stderr, "Playing back trace file...\n");
      Sleep(1000);
   } else {
      // fprintf(stderr, "Running test for %lu seconds...\n", opt.dwTestDuration);
      for (i = 0; i < opt.dwTestDuration; i++) {
        print_progress(i, opt.dwTestDuration);
         fprintf(stderr, " %3lu%% complete...\r", i * 100 / opt.dwTestDuration);
         Sleep(1000);

         /* This can happen only if the user pressed CTRL-C */
         if (threadInfo.bTestFinished == TRUE) {
            fprintf(stderr, "\nTest interrupted.\n");
            break;
         }
      }

      /* Test is over, signal the worker threads to shutdown*/
      if (threadInfo.bTestFinished == FALSE) {
         fprintf(stderr, "Test complete.   \n");
         fprintf(stderr, "Boost 1.   \n");
      }
      threadInfo.bTestFinished = TRUE;
   }

   /* Wait for all the threads to complete shutdown */
   fprintf(stderr, "Boost 2.   \n");
   DEBUG_MSG("Waiting for worker threads to shutdown\n");
#ifdef __UNIX__
   threadInfo.synchedThreads = 0;
#endif
  fprintf(stderr, "Boost 3.   \n");
  //  synchWorkerThreads();
   DEBUG_MSG("All workers completed shut down\n\n");
  fprintf(stderr, "Boost 4.   \n");
   /* The test is over: record the finish time */
   GetSystemTime(&sNow);
   dwFinishTime = convSysTimeToMillisec(&sNow);

   /*
    * Consolidate and dump the stats
    */
   for (i = 0; i < opt.dwNumThreads; i++) {
      qwTotalBytesTransferred += 
         threadInfo.sThreads[i].qwTotalBytesTransferred;
      dwCompletedIOs += threadInfo.sThreads[i].dwCompletedIOs;
      dwIosAboveLatThresh += threadInfo.sThreads[i].dwIosAboveLatThresh;
      dAvgLatency += threadInfo.sThreads[i].dAvgLatency;
      if (dwMaxLatency < threadInfo.sThreads[i].dwMaxLatency){
         dwMaxLatency = threadInfo.sThreads[i].dwMaxLatency;
      }
   }
   dTestDuration = (dwFinishTime - dwStartTime)/1000.0;

   switch (opt.cOutputFormat) {
   case 'f':
      printf("\nTest duration   = %12.3f s\n", dTestDuration);
      break;

   case 'c':
      printf("Test duration [s],"
             "Worker threads,"
             "File/Device path prefix,"
             "File/Device size [MB],"
             "Buffer size [MB],"
             "IO access pattern,"
             "IO size (Avg) [B],"
             "IO size pattern,"
             "Burst size (OIOs Avg) [#],"
             "Burst size pattern,"
             "Inter-burst time (Avg) [us],"
             "Inter-burst time pattern,"
             "Read ratio,"
             "Latency threshold [ms],"
             "Buffered IO,"
             "Checksum,"
             "Raw Device Access,"
             "Fill file,"
             "Trace file,"
             "Actual test duration [s],"
             "Total IOs,"
             "Total Bytes,"
             "IOPS,"
             "Throughput [B/s],"
             "Average latency [us],"
             "Max Latency [us],"
             "Long IOs\n");

   case 'z': 
      printf("%ld,", opt.dwTestDuration);
      printf("%ld,", opt.dwNumThreads);
      printf("%s,", opt.lpDevFilePath);
      printf("%llu,", opt.qwFileSize / 1024 / 1024);
      printf("%lu,", opt.dwBufferSize / 1024 / 1024);
      printf("%c,", opt.cIoAccessPattern);
      printf("%lu,", opt.dwIOSize);
      printf("%c,", opt.cIoSizePattern);
      printf("%lu,", opt.dwOutstandingIOs);
      printf("%c,", opt.cOutstandingIOsPattern);
      printf("%lu,", opt.dwIOGap);
      printf("%c,", opt.cIOGapPattern);
      printf("%.1f,", opt.dRDRatio);
      printf("%ld,", opt.dwLatencyThreshold);
      printf("%d,", opt.bBufferedIO);
      printf("%d,", opt.bDataChecksum);
      printf("%d,", opt.bRawDev);
      printf("%d,", opt.bFillFile);
      printf("%s,", opt.lpTraceFile);
      printf("%12.3f,", dTestDuration);
      printf("%8ld,", dwCompletedIOs);
      printf("%12llu,", qwTotalBytesTransferred);
      printf("%8.0lf,", dwCompletedIOs / dTestDuration);
      printf("%12.0lf,", qwTotalBytesTransferred / dTestDuration);
      printf("%12.3f,", dAvgLatency / dwCompletedIOs);
      printf("%12lu,", dwMaxLatency);
      printf("%8lu\n", dwIosAboveLatThresh);
      break;
   }
  
   return 0;
}



void simulate(reader_t *reader, cache_t *cache, int report_interval,
              int warmup_sec, char *ofilepath) {

if(cache->io == false){
  /* random seed */
  srand(time(NULL));
  set_rand_seed(rand());

  request_t *req = new_request();
  uint64_t req_cnt = 0, miss_cnt = 0;
  uint64_t last_req_cnt = 0, last_miss_cnt = 0;
  uint64_t req_byte = 0, miss_byte = 0;

  read_one_req(reader, req);
  uint64_t start_ts = (uint64_t)req->clock_time;
  uint64_t last_report_ts = warmup_sec;

  double start_time = -1; 
  int no=1;
  
  
  while (req->valid) {
    req->clock_time -= start_ts;
    if (req->clock_time <= warmup_sec) {
      cache->get(cache, req);
      read_one_req(reader, req);
      continue;
    } else {
      if (start_time < 0) {
        start_time = gettime();
      }
    }

    req_cnt++;
    req_byte += req->obj_size;
    //  printf("%" PRIu64 "\n", req->obj_id); 
    if (cache->get(cache, req) == false) {
      miss_cnt++;
      miss_byte += req->obj_size; 
      
    }else{ 
      
    } 
    
    bool do_flog = true;
    if(do_flog){
      FILE *fp3 = fopen("/dev/shm/lll.txt", "a");  // "a" = append mode
      if (fp3 == NULL) {
          perror("Error opening file");
          // return 1; 
      }

      // fprintf(fp3, "0,%d,%d\n", req_cnt, miss_cnt);
      // fprintf(fp3, "%d,%d\n", req_cnt, miss_cnt);
      fclose(fp3);  // Always close after writing
    }
    
    
    no++; 
    if (req->clock_time - last_report_ts >= report_interval &&
        req->clock_time != 0) {
      INFO(
          "%s %s %.2lf hour: %lu requests, miss ratio %.4lf, interval miss "
          "ratio "
          "%.4lf\n",
          mybasename(reader->trace_path), cache->cache_name,
          (double)req->clock_time / 3600, (unsigned long)req_cnt,
          (double)miss_cnt / req_cnt,
          (double)(miss_cnt - last_miss_cnt) / (req_cnt - last_req_cnt));
      last_miss_cnt = miss_cnt;
      last_req_cnt = req_cnt;
      last_report_ts = (int64_t)req->clock_time;
    }

    read_one_req(reader, req);
  }

  // fclose(file);

  printf("Value: %" PRIu64 "\n", req_byte);


  double runtime = gettime() - start_time;

  char output_str[1024];
  char size_str[8];
  convert_size_to_str(cache->cache_size, size_str);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
  snprintf(output_str, 1024,
           "%s %s cache size %8s, %16lu req, miss ratio %.4lf\n",
           reader->trace_path, cache->cache_name, size_str,
           (unsigned long)req_cnt, (double)miss_cnt / (double)req_cnt);

#pragma GCC diagnostic pop
  printf("%s", output_str);

  FILE *output_file = fopen(ofilepath, "a");
  if (output_file == NULL) {
    ERROR("cannot open file %s %s\n", ofilepath, strerror(errno));
    exit(1);
  }
  fprintf(output_file, "%s\n", output_str);
  fclose(output_file);

#if defined(TRACK_EVICTION_V_AGE)
  while (cache->get_occupied_byte(cache) > 0) {
    cache->evict(cache, req);
  }

#endif
// #else
}else{
  // this is with I/O
  mainx(reader, cache, report_interval,warmup_sec,ofilepath);


//   request_t *req = new_request();
  uint64_t req_cnt = 0, miss_cnt = 0;
  uint64_t last_req_cnt = 0, last_miss_cnt = 0;
  uint64_t req_byte = 0, miss_byte = 0;

  double start_time = -1;

  double runtime = gettime() - start_time;

  char output_str[1024];
  char size_str[8];
  convert_size_to_str(cache->cache_size, size_str);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
  // snprintf(output_str, 1024,
  //          "%s %s cache size %8s, %16lu req, miss ratio %.4lf, throughput "
  //          "%.2lf MQPS\n",
  //          reader->trace_path, cache->cache_name, size_str,
  //          (unsigned long)req_cnt, (double)miss_cnt / (double)req_cnt,
  //          (double)req_cnt / 1000000.0 / runtime);

#pragma GCC diagnostic pop
  // printf("%s", output_str);

  FILE *output_file = fopen(ofilepath, "a");
  if (output_file == NULL) {
    ERROR("cannot open file %s %s\n", ofilepath, strerror(errno));
    exit(1);
  }
  fprintf(output_file, "%s\n", output_str);
  fclose(output_file);

#if defined(TRACK_EVICTION_V_AGE)
  while (cache->get_occupied_byte(cache) > 0) {
    cache->evict(cache, req);
  }

#endif

//. end of HIT_RATIO_ONLY
// #endif

}
const char *dir = "/dev/shm";

/* Create directory if it does not exist */
if (mkdir(dir, 0755) == -1) {
    if (errno != EEXIST) {
        perror("Failed to create directory");
        return;
    }
}
char filepath[256];
  snprintf(filepath, sizeof(filepath), "/dev/shm/prefetched%d.txt", cache->log);

  FILE *fp = fopen(filepath, "r");
  if (fp == NULL) {
      perror("Failed to open file for reading");
      // return;
  }

  char buffer[512];
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
      printf("Prefetched: %s\n", buffer);  // buffer already includes newline if present
  }

  fclose(fp);
}






#ifdef __cplusplus
}
#endif
