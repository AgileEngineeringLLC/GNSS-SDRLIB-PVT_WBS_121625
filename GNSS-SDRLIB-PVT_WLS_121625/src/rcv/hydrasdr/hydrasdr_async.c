//-----------------------------------------------------------------------------
//
// hydra_async.c
//
// Purpose: HydraSDR demonstration program for setting up a HydraSDR and saving
//  raw (int8) IQ data to a binary file using async streaming and a callback
//  function.
//
// To build and run:
//  gcc hydrasdr_async.c -o hydrasdr_async -lpthread -lhydrasdr
//  ./hydrasdr_async
//
// Author: Don Kelly, 11/25/25
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
//#include <hydrasdr.h>
#include "hydrasdr.h"
#include <sys/time.h>

//-----------------------------------------------------------------------------
// Initializations
//-----------------------------------------------------------------------------
#define DEFAULT_FREQUENCY   1575420000     // 1.57542 GHz
#define DEFAULT_SAMPLERATE  10000000       // 10.0 MS/s
#define DEFAULT_GAIN        21             // Can be 0 to 21
#define DEFAULT_BIASTEE     1              // Disable 0; Enable 1
#define BLOCK_SIZE          16384          // 16 KB blocks
//#define RTLSDR_ASYNC_BUF_NUMBER   32       // 32 zero-copy buffers
#define MONITOR_INTERVAL     1.0           // seconds between rate checks
#define GAIN_MODE            0             // 0 = AGC, 1 = Manual Gain
#define OUTPUT_FILENAME     "capture.bin"  // Output binary file

static volatile int stopflag = 0;
static FILE *outfile = NULL;
static uint64_t total_bytes = 0;
static uint64_t last_bytes = 0;
static struct timeval last_time;
struct hydrasdr_device* dev = NULL;  // Device object

//-----------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------

// Asynchronous callback
int rx_callback(hydrasdr_transfer_t* transfer) {
    // Write to file
    int byte_count;
    byte_count = transfer->sample_count * 4;
    fwrite(transfer->samples, 1, byte_count, outfile);
    total_bytes += byte_count;
    return 0;
}

// Thread to read user input
void *keythread(void *arg) {
    printf("Press 'q' + Enter to halt streaming.\n\n");
    int c;
    while ((c = getchar()) != EOF) {
        if (c == 'q' || c == 'Q') {
            stopflag = 1;
            break;
        }
    }
    return NULL;
}

// Thread to read IQ data
void *datathread(void *arg) {
    // Start async streaming
    int r;
    r = hydrasdr_start_rx(dev, rx_callback, NULL);
    if (r < 0) {
        fprintf(stderr, "Async read failed: %d\n", r);
        hydrasdr_close(dev);
        //return EXIT_FAILURE;
    }
    return NULL;
}

// Function to calculate time difference in seconds
double time_diff_sec(struct timeval *t1, struct timeval *t2) {
  return (double)(t1->tv_sec - t2->tv_sec) +
         (double)(t1->tv_usec - t2->tv_usec) / 1e6;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
int main(void) {
    int r;
    int device_index = 0;
    pthread_t key_thread;
    pthread_t data_thread;

    // Handle Ctrl+C as well
    signal(SIGINT, SIG_DFL);

    // Find and open device
    if (hydrasdr_open(&dev) < 0) {
        fprintf(stderr, "Failed to open HydraSDR device.\n");
        return -1;
    }
    printf("\nHydraSDR device open successful.\n");

    // Configure device samplerate
    r = hydrasdr_set_samplerate(dev, DEFAULT_SAMPLERATE);
    if (r == 0) {
        printf("Sample rate set to %d SPS.\n", DEFAULT_SAMPLERATE);
    } else {
        fprintf(stderr, "Sample rate failed to set.\n");
    }

    // Configure device center frequency
    r = hydrasdr_set_freq(dev, DEFAULT_FREQUENCY);
    if (r == 0) {
        printf("Center frequency set to %d Hz.\n", DEFAULT_FREQUENCY);
    } else {
        fprintf(stderr, "Center frequency failed to set.\n");
    }

    // Configure gain
    r = hydrasdr_set_linearity_gain(dev, DEFAULT_GAIN);
    if (r == 0) {
        printf("Linearity gain set to %d.\n", DEFAULT_GAIN);
    } else {
        fprintf(stderr, "Linearity gain failed to set.\n");
    }

    // Configure Bias-Tee
    r = hydrasdr_set_rf_bias(dev, DEFAULT_BIASTEE);
    if (r == 0) {
        printf("Bias Tee set to %d (Disable 0; Enable: 1).\n",
                DEFAULT_BIASTEE);
    } else {
        fprintf(stderr, "Linearity gain failed to set.\n");
    }

    // Configure data type to save (int16, sample type 2)
    r = hydrasdr_set_sample_type(dev, HYDRASDR_SAMPLE_INT16_IQ);
    if (r == 0) {
        printf("Sample type set to %d (Type 2 is int16).\n",
                HYDRASDR_SAMPLE_INT16_IQ);
    } else {
        fprintf(stderr, "Sample type failed to set.\n");
    }

    // Reset front end buffer
    //rtlsdr_reset_buffer(dev);

    // Open output file
    outfile = fopen(OUTPUT_FILENAME, "wb");
    if (!outfile) {
        fprintf(stderr, "Failed to open output file '%s'.\n",
                OUTPUT_FILENAME);
        //rtlsdr_close(dev);
        return -1;
    }

    // Initialize timer
    gettimeofday(&last_time, NULL);
    last_bytes = total_bytes;

    // Initialize nanosleep function parms
    struct timespec ts;
    ts.tv_sec = 0; // seconds
    ts.tv_nsec = 990 * 1000000; // 990 ms;

    // Create keyboard and data streaming threads
    fprintf(stderr, "\nStarting async streaming (16 KB blocks).\n");
    pthread_create(&key_thread, NULL, keythread, NULL);
    pthread_create(&data_thread, NULL, datathread, NULL);

    // Main thread while loop. Verify the data rate every MONITOR_INTERVAL
    // seconds. Halt main loop if stopflag received by keyboard thread.
    while (!stopflag) {
      // Calculate elapsed time
      struct timeval now;
      gettimeofday(&now, NULL);
      double elapsed = time_diff_sec(&now, &last_time);

      // Display rate and bytes received if it is time to do so
      if (elapsed >= MONITOR_INTERVAL) {
        uint64_t delta_bytes = total_bytes - last_bytes;
          double rate_mbps = (delta_bytes * 8.0) / (elapsed * 1e6);
          double rate_msps = (delta_bytes / 4.0) / (elapsed * 1e6);
          fprintf(stderr,
                  "Rate: %.8f Mbit/s (%011.8f Msps), Bytes Received: %.8ld\n",
                  rate_mbps, rate_msps, total_bytes);
          last_time = now;
          last_bytes = total_bytes;
          nanosleep(&ts, NULL); // give thread a rest before next check
      }  // end if
    }  // end main while loop

    // Cleanup threads and close file
    printf("\nAsync streaming halted\n");
    pthread_join(key_thread, NULL);
    //pthread_join(data_thread, NULL);
    fclose(outfile);

    // If bias-tee was manually engaged, turn it off before closing device
    //rtlsdr_set_bias_tee(dev, 0);
    hydrasdr_close(dev);

    // Program done
    fprintf(stderr, "Capture stopped. Total bytes written: %.2f MB\n\n",
            total_bytes / (1024.0 * 1024.0));
    return 0;
} // end main thread
