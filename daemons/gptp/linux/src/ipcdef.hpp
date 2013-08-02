#ifndef IPCDEF_HPP
#define IPCDEF_HPP

typedef struct { 
  int64_t ml_phoffset;
  int64_t ls_phoffset;
  FrequencyRatio ml_freqoffset;
  FrequencyRatio ls_freqoffset;
  int64_t local_time;
} gPtpTimeData;


/*

  Integer64  <master-local phase offset>
  Integer64  <local-system phase offset>
  LongDouble <master-local frequency offset>
  LongDouble <local-system frequency offset>
  UInteger64 <local time of last update>

  * Meaning of IPC provided values:

  master  ~= local   - <master-local phase offset>
  local   ~= system  - <local-system phase offset>
  Dmaster ~= Dlocal  * <master-local frequency offset>  
  Dlocal  ~= Dsystem * <local-system freq offset>        (where D denotes a delta)

*/

#define SHM_SIZE 4*8 + sizeof(pthread_mutex_t) /* 3 - 64 bit and 2 - 32 bits */
#define SHM_NAME  "/ptp"

#endif/*IPCDEF_HPP*/
