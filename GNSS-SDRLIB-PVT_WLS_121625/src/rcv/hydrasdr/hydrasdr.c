/*------------------------------------------------------------------------------
* hydrasdr.c : HydraSDR functions
*
*-----------------------------------------------------------------------------*/
#include "sdr.h"
#include <unistd.h>

#define DEFAULT_FREQUENCY   1575420000     // 1.57542 GHz
#define DEFAULT_SAMPLERATE  10000000       // 10.0 MS/s
#define DEFAULT_GAIN        21             // Can be 0 to 21
#define DEFAULT_BIASTEE     1              // Disable 0; Enable 1

//void **buffers;
uint64_t countHydra=0;
struct hydrasdr_device* dev = NULL;  // Device object

/* HydraSDR stream callback  ----------------------------------------------------
* callback for receiving RF data
* see libbladeRF.h L854-L882
*-----------------------------------------------------------------------------*/
int rx_callback_hydrasdr(hydrasdr_transfer_t* transfer)
{
  int i,ind;
  // Create array for later loading into sdrstat.buff
  int16_t *sample=(int16_t *)transfer->samples;

  // buffer index
  ind=(sdrstat.buffcnt%MEMBUFFLEN)*2*HYDRASDR_DATABUFF_SIZE;

  mlock(hbuffmtx);
  // Copy stream data to memory (global) buffer
  for (i=0;i<2*HYDRASDR_DATABUFF_SIZE;i++) {
    sdrstat.buff[ind+i]=(unsigned char)((sample[i]>>4)+127.5);
  }
  unmlock(hbuffmtx);

  //printf("IQ Block %ld stored to memory buffer.\n",sdrstat.buffcnt%MEMBUFFLEN);

  // Increment buffcnt
  mlock(hreadmtx);
  sdrstat.buffcnt++;
  unmlock(hreadmtx);

  // Done
  return 0;
} // end callback function

/* HydraSDR initialization ------------------------------------------------------
* search front end and initialization
* args   : none
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int hydrasdr_init(void)
{
  int ret;

  // Open HydraSDR
  if (hydrasdr_open(&dev) < 0) {
    fprintf(stderr, "Failed to open HydraSDR device.\n");
    return -1;
  }
  printf("\nHydraSDR device open successful.\n");

  // set configuration
  ret=hydrasdr_initconf();
  if (ret<0) {
    SDRPRINTF("error: hydrasdr failed to init.\n");
    return -1;
  }
  return 0;
}

/* stop front-end --------------------------------------------------------------
* stop grabber of front end
* args   : none
* return : none
*-----------------------------------------------------------------------------*/
extern void hydrasdr_quit(void)
{
  hydrasdr_close(dev);
}

/* HydraSDR configuration function ----------------------------------------------
* load configuration file and setting
* args   : none
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int hydrasdr_initconf(void)
{
  int ret;

  // Configure device samplerate
  ret = hydrasdr_set_samplerate(dev, DEFAULT_SAMPLERATE);
  if (ret == 0) {
    printf("Sample rate set to %d SPS.\n", DEFAULT_SAMPLERATE);
  } else {
    fprintf(stderr, "Sample rate failed to set.\n");
  }

  // Configure device center frequency
  ret = hydrasdr_set_freq(dev, DEFAULT_FREQUENCY);
  if (ret == 0) {
    printf("Center frequency set to %d Hz.\n", DEFAULT_FREQUENCY);
  } else {
    fprintf(stderr, "Center frequency failed to set.\n");
  }

  // Configure gain
  ret = hydrasdr_set_linearity_gain(dev, DEFAULT_GAIN);
  if (ret == 0) {
    printf("Linearity gain set to %d.\n", DEFAULT_GAIN);
  } else {
    fprintf(stderr, "Linearity gain failed to set.\n");
  }

  // Configure Bias-Tee
  ret = hydrasdr_set_rf_bias(dev, DEFAULT_BIASTEE);
  if (ret == 0) {
    printf("Bias Tee set to %d (Disable 0; Enable: 1).\n",
            DEFAULT_BIASTEE);
  } else {
    fprintf(stderr, "Linearity gain failed to set.\n");
  }

  // Configure data type to save (int16, sample type 2)
  ret = hydrasdr_set_sample_type(dev, HYDRASDR_SAMPLE_INT16_IQ);
  if (ret == 0) {
    printf("Sample type set to %d (Type 2 is int16).\n",
            HYDRASDR_SAMPLE_INT16_IQ);
  } else {
    fprintf(stderr, "Sample type failed to set.\n");
  }

  // Function return
  return 0;
}

/* start grabber ---------------------------------------------------------------
* start grabber of front end
* args   : none
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int hydrasdr_start(void)
{
  int ret;
    
  // enable RF module  (not needed for hydrasdr)

  // start stream and stay there until we kill the stream
  ret = hydrasdr_start_rx(dev, rx_callback_hydrasdr, NULL);
  if (ret < 0) {
    fprintf(stderr, "Async read failed: %d\n", ret);
    hydrasdr_close(dev);
  }
  return 0;
}

/* stop grabber ----------------------------------------------------------------
* stop grabber of front end
* args   : none
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int hydrasdr_stop(void)
{
    // disable RF module (not needed for hydrasdr)

    // deinitialize stream (not needed for hydrasdr)

    return 0;
}

/* DC-offset calibration -------------------------------------------------------
* calibration DC offset and I/Q imbalance
* args   : double *buf      I   orignal RF data buffer
*          int    n         I   number of data buffer
*          int    dtype     I   data type (DTYPEI or DTYPEIQ)
*          char   *outbuf   O   output data buffer
* return : none
*-----------------------------------------------------------------------------*/
/*
extern void calibration_dcoffset(double *inbuf, int n, int dtype, char *outbuf)
{
    int i;
    double dmeanI=0,dmeanQ=0;

    for (i=0;i<n;i++) {
        if (dtype==DTYPEI) {
            dmeanI+=inbuf[i];
        } else if (dtype==DTYPEIQ) {
            dmeanI+=inbuf[2*i  ];
            dmeanQ+=inbuf[2*i+1];
        }
    }
    dmeanI/=n; dmeanQ/=n;

    for (i=0;i<n;i++) {
        if (dtype==DTYPEI) {
            outbuf[i]=(char)(inbuf[i]-dmeanI);
        } else if (dtype==DTYPEIQ) {
            outbuf[2*i  ]=(char)(inbuf[2*i  ]-dmeanI);
            outbuf[2*i+1]=(char)(inbuf[2*i+1]-dmeanQ);
        }
    }
}
*/

/* data expansion --------------------------------------------------------------
* get current data buffer from memory buffer
* args   : int16_t *buf     I   bladeRF raw buffer
*          int    n         I   number of grab data
*          char   *expbuf   O   extracted data buffer
* return : none
*-----------------------------------------------------------------------------*/
extern void hydrasdr_exp(unsigned char *buf, int n, char *expbuf)
{
  int i;
  for (i=0;i<n;i++) {
    expbuf[i]=(char)((buf[i]-127.5)); // unsigned char to char
  }

  /*
    double *dbuf;
    unsigned char *data=buf;

    dbuf=(double*)malloc(sizeof(double)*n);

    for (i=0;i<n/2;i++) {
        dbuf[2*i  ]=(double)(*(data+2*i  ));
        dbuf[2*i+1]=(double)(*(data+2*i+1));
    }
    calibration_dcoffset(dbuf,n/2,DTYPEIQ,expbuf);
    free(dbuf);
  */
}

/* get current data buffer -----------------------------------------------------
* get current data buffer from memory buffer
* args   : uint64_t buffloc I   buffer location
*          int    n         I   number of grab data
*          char   *expbuf   O   extracted data buffer
* return : none
*-----------------------------------------------------------------------------*/
extern void hydrasdr_getbuff(uint64_t buffloc, int n, char *expbuf)
{
  uint64_t membuffloc=2*buffloc%(MEMBUFFLEN*2*HYDRASDR_DATABUFF_SIZE);
  int nout;
  n=2*n;
  nout=(int)((membuffloc+n)-(MEMBUFFLEN*2*HYDRASDR_DATABUFF_SIZE));

  mlock(hbuffmtx);
  if (nout>0) {
    hydrasdr_exp(&sdrstat.buff[membuffloc],n-nout,expbuf);
    hydrasdr_exp(&sdrstat.buff[0],nout,&expbuf[n-nout]);
  } else {
    hydrasdr_exp(&sdrstat.buff[membuffloc],n,expbuf);
  }
  unmlock(hbuffmtx);
}

/* push data to memory buffer --------------------------------------------------
* push data to memory buffer from BladeRF binary IF file
* args   : none
* return : none
*-----------------------------------------------------------------------------*/
extern void fhydrasdr_pushtomembuf(void)
{
    size_t nread;
    uint16_t buff[HYDRASDR_DATABUFF_SIZE*2];
    int i,ind;

    mlock(hbuffmtx);

    nread=fread(buff,sizeof(uint16_t),2*HYDRASDR_DATABUFF_SIZE,sdrini.fp1);
    
    /* buffer index */
    ind=(sdrstat.buffcnt%MEMBUFFLEN)*2*HYDRASDR_DATABUFF_SIZE;

    for (i=0;i<nread;i++) {
        sdrstat.buff[ind+i]=(uint8_t)((buff[i]>>4)+127.5);
    }

    //printf("buffcnt: %ld, sdrstat.buff[999]: %d, buff[999]: %d\n",
    //  sdrstat.buffcnt, sdrstat.buff[ind+999], buff[999]);

    unmlock(hbuffmtx);

    if (nread<2*HYDRASDR_DATABUFF_SIZE) {
        sdrstat.stopflag=ON;
        SDRPRINTF("end of file!\n");
    }

    mlock(hreadmtx);
    sdrstat.buffcnt++;
    unmlock(hreadmtx);
}
