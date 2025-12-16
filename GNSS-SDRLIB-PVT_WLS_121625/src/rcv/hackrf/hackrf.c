/*------------------------------------------------------------------------------
* hackrf.c : HackRF One functions
*
*-----------------------------------------------------------------------------*/
#include "../../../src/sdr.h"

static hackrf_device* device = NULL;

/* hackrf stream callback  -----------------------------------------------------
* callback for receiving RF front end data and loading it in memory buffer
*-----------------------------------------------------------------------------*/
int stream_callback_hackrf(hackrf_transfer* transfer)
{
  int i, ind;

  mlock(hbuffmtx);
  ind = (sdrstat.buffcnt%MEMBUFFLEN)*2*HACKRF_DATABUFF_SIZE;
  for (i=0;i<2*HACKRF_DATABUFF_SIZE;i++) {
    sdrstat.buff[ind+i]=(unsigned char)(transfer->buffer[i]+127.5);
  }
  //printf("buffcnt: %ld, buff[999]: %d, buffer[999]: %d\n", sdrstat.buffcnt,
  //  sdrstat.buff[ind+999], transfer->buffer[999]);
  unmlock(hbuffmtx);


  mlock(hreadmtx);
  sdrstat.buffcnt++;
  unmlock(hreadmtx);

  return 0;
}
/* hackrf initialization -------------------------------------------------------
* search front end and initialization
* args   : none
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int hackrfOne_init(void)
{
  int result = 0;
  //int dev_index = 0;;

  // Init HackRF
  result = hackrf_init();
  if( result != HACKRF_SUCCESS )
  {
    fprintf(stderr, "hackrf_init() failed: %s (%d)\n", hackrf_error_name(result), result);
    //return EXIT_FAILURE;
  }

  // Open device
  const char* serial_number = NULL;
  result = hackrf_open_by_serial(serial_number, &device);
  if( result == HACKRF_SUCCESS ) {
    printf("HackRF One device opened.\n");
  } else {
    fprintf(stderr, "hackrf_open() failed: %s (%d)\n", hackrf_error_name(result), result);
    //return EXIT_FAILURE;
  }

  /* set configuration */
  result = hackrf_initconf();
  if (result<0) {
    SDRPRINTF("error: failed to initialize rtlsdr\n");
    return -1;
  }

  return 0;
}

/* stop front-end --------------------------------------------------------------
* stop grabber of front end
* args   : none
* return : none
*-----------------------------------------------------------------------------*/
extern void hackrf_quit(void)
{
  int result=0;

  // Perform clean shutdown of hackRF. Stop streaming, disable bias-tee,
  // close device, and exit device.
  printf("\nStopping async streaming.\n");
  result = hackrf_stop_rx(device);
  if (result == HACKRF_SUCCESS) {
    printf("Streaming halted.\n");
  } else {
    fprintf(stderr, "hackrf_stop_rx() failed: %s (%d)\n",
        hackrf_error_name(result), result);
    //return EXIT_FAILURE;
  }
  // Disable bias-tee
  result = hackrf_set_antenna_enable(device, 0);
  if (result == HACKRF_SUCCESS) {
    printf("Bias-tee disabled.\n");
  } else {
    fprintf(stderr, "Disable bias tee failed: %s (%d)\n",
       hackrf_error_name(result), result);
    //return EXIT_FAILURE;
  }
  // Close device
  result = hackrf_close(device);
  if (result == HACKRF_SUCCESS) {
    printf("HackRF device closed.\n");
  } else {
    fprintf(stderr, "hackrf_close failed: %s (%d)\n",
       hackrf_error_name(result), result);
    //return EXIT_FAILURE;
  }
  // Exit device
  hackrf_exit();
  printf("HackRF library exited.\n");
}

/* rtlsdr configuration function -----------------------------------------------
* load configuration file and setting
* args   : none
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int hackrf_initconf(void)
{
  int result;

  // Set frequency
  result = hackrf_set_freq(device, HACKRF_FREQUENCY);
  if ( result == HACKRF_SUCCESS ) {
    printf("Center frequency set to %11.6f MHz.\n", HACKRF_FREQUENCY/1e6);
  } else {
    fprintf(stderr, "hackrf_set_freq() failed: %s (%d)\n",
       hackrf_error_name(result), result);
    //return EXIT_FAILURE;
  }

  // Set sample rate
  result = hackrf_set_sample_rate(device, HACKRF_SAMPLE_RATE);
  if ( result == HACKRF_SUCCESS ) {
    //printf("Sample rate set to %d SPS.\n", DEFAULT_SAMPLERATE);
    printf("Sample rate set to %11.8f MSPS.\n", HACKRF_SAMPLE_RATE/1e6);
  } else {
    fprintf(stderr, "hackrf_sample_rate_set() failed: %s (%d)\n",
       hackrf_error_name(result), result);
    //return EXIT_FAILURE;
  }

  // Enable bias-tee
  result = hackrf_set_antenna_enable(device, 1);
  if (result == HACKRF_SUCCESS) {
    printf("Bias-tee enabled.\n");
  } else {
    fprintf(stderr, "Enable bias tee failed: %s (%d)\n",
       hackrf_error_name(result), result);
    //return EXIT_FAILURE;
  }

  // Enable/Disable the RF Amplifier
  // Value should be 0 (off) or 1 (on).
  result = hackrf_set_amp_enable(device, HACKRF_RF_AMP);
  if (result == HACKRF_SUCCESS) {
    printf("RF amp enabled.\n");
  } else {
    fprintf(stderr, "Enable RF amp failed: %s (%d)\n",
       hackrf_error_name(result), result);
    //return EXIT_FAILURE;
  }

  // Configure LNA gain
  result = hackrf_set_lna_gain(device, HACKRF_LNA_GAIN);
  if ( result == HACKRF_SUCCESS ) {
    printf("LNA gain set to %d.\n", HACKRF_LNA_GAIN);
  } else {
    fprintf(stderr, "hackrf_set_lna_gain() failed: %s\n", hackrf_error_name(result));
    //return EXIT_FAILURE;
  }

  // Configure VGA gain
  result = hackrf_set_vga_gain(device, HACKRF_VGA_GAIN);
  if ( result == HACKRF_SUCCESS ) {
    printf("VGA gain set to %d.\n", HACKRF_VGA_GAIN);
  } else {
    fprintf(stderr, "hackrf_set_vga_gain() failed: %s\n", hackrf_error_name(result));
    //return EXIT_FAILURE;
  }

    return 0;
}

/* start grabber ---------------------------------------------------------------
* start grabber of front end
* args   : none
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int hackrf_start(void)
{
  int result;

  // Start async streaming
  result = hackrf_start_rx(device, stream_callback_hackrf, NULL);
  if (result != HACKRF_SUCCESS)
  {
    fprintf(stderr, "hackrf_start_rx() failed: %s (%d)\n",
       hackrf_error_name(result), result);
    //return EXIT_FAILURE;
  }
  fprintf(stderr, "Async streaming started (16 KB blocks).\n\n");

  return 0;
}
/* data expansion --------------------------------------------------------------
* get current data buffer from memory buffer
* args   : int16_t *buf     I   bladeRF raw buffer
*          int    n         I   number of grab data
*          char   *expbuf   O   extracted data buffer
* return : none
*-----------------------------------------------------------------------------*/
extern void hackrf_exp(uint8_t *buf, int n, char *expbuf)
{
    int i;

    for (i=0;i<n;i++) {
        expbuf[i]=(char)((buf[i]-127.5)); /* unsigned char to char */
    }
}

/* get current data buffer -----------------------------------------------------
* get current data buffer from memory buffer
* args   : uint64_t buffloc I   buffer location
*          int    n         I   number of grab data
*          char   *expbuf   O   extracted data buffer
* return : none
*-----------------------------------------------------------------------------*/
extern void hackrf_getbuff(uint64_t buffloc, int n, char *expbuf)
{
    uint64_t membuffloc=2*buffloc%(MEMBUFFLEN*2*HACKRF_DATABUFF_SIZE);
    int nout;
    n=2*n;
    nout=(int)((membuffloc+n)-(MEMBUFFLEN*2*HACKRF_DATABUFF_SIZE));

    mlock(hbuffmtx);
    if (nout>0) {
        hackrf_exp(&sdrstat.buff[membuffloc],n-nout,expbuf);
        hackrf_exp(&sdrstat.buff[0],nout,&expbuf[n-nout]);
    } else {
        hackrf_exp(&sdrstat.buff[membuffloc],n,expbuf);
    }
    //printf("expbuf[999]: %d\n", expbuf[999]);
    unmlock(hbuffmtx);
}

/* push data to memory buffer --------------------------------------------------
* push data to memory buffer from binary IF file
* args   : none
* return : none
*-----------------------------------------------------------------------------*/
extern void fhackrf_pushtomembuf(void)
{
    //printf("Entered fhackrf_pushtomembuf.\n");
    size_t nread;

    mlock(hbuffmtx);
    nread=fread(
        &sdrstat.buff[(sdrstat.buffcnt%MEMBUFFLEN)*2*HACKRF_DATABUFF_SIZE],
        1,2*HACKRF_DATABUFF_SIZE,sdrini.fp1);
    //printf("buffcnt: %ld, buff[999]: %d\n", sdrstat.buffcnt,
    //  sdrstat.buff[(sdrstat.buffcnt%MEMBUFFLEN)*2*HACKRF_DATABUFF_SIZE+999]);
    unmlock(hbuffmtx);

    if (nread<2*HACKRF_DATABUFF_SIZE) {
        sdrstat.stopflag=ON;
        SDRPRINTF("end of file!\n");
    }

    mlock(hreadmtx);
    sdrstat.buffcnt++;
    unmlock(hreadmtx);
}
