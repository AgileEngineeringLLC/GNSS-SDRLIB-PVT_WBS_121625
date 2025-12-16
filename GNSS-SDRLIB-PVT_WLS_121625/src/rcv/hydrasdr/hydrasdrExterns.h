
#ifndef HYDRASDREXTERNS_H_
#define HYDRASDREXTERNS_H_

// HydraSDR FD is set to 16384
#define HYDRASDR_DATABUFF_SIZE 65536   // bladeRF is 32768
extern int hydrasdr_init(void);
extern void hydrasdr_quit(void);
extern int hydrasdr_initconf(void);
extern int hydrasdr_start(void);
extern int hydrasdr_stop(void);
extern void calibration_dcoffset(double *inbuf, int n, int dtype, char *outbuf);
extern void hydrasdr_exp(unsigned char *buf, int n, char *expbuf);
extern void hydrasdr_getbuff(uint64_t buffloc, int n, char *expbuf);
extern void fhydrasdr_pushtomembuf(void);

#endif /* HYDRASDREXTERNS_H_ */