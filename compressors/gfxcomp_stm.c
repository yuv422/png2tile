#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define RLE_TYPE_NORMAL         0x01
#define RLE_TYPE_INCREMENTAL    0x03

#define HI(x)  ((x)>>8)
#define LO(x)  ((x)&0xff)

#define MIN_RLE_LEN     2
#define MAX_RLE_LEN     MIN_RLE_LEN+63
#define MAX_RAW_LEN     63

unsigned int in_size;
bool shoudchangeHH;
size_t current;
unsigned short int cur_HH;
unsigned int writepos;
unsigned short int* buf;
unsigned char* outbuf;
unsigned int outsize;

typedef unsigned char uint8_t;
typedef unsigned int  uint32_t;


bool writeRLE (unsigned char val, int cnt, unsigned char type) {
  unsigned char tmp;
  tmp=(LO(cnt-MIN_RLE_LEN)<<2)|type;
  
  if (writepos<outsize)
    outbuf[writepos++]=tmp;         // write len
  else
    return (false);                 // please give me more space for output
    
  if (writepos<outsize)
    outbuf[writepos++]=val;         // write value
  else
    return (false);                 // please give me more space for output
    
  return (true);
}

bool writeHI (unsigned char val, bool temp) {
  val<<=3;
  if (temp) val|=0x04;
  val|=0x02;
  
  if (writepos<outsize)
    outbuf[writepos++]=val;         // write value
  else
    return (false);                 // please give me more space for output
    
  return (true);
}

bool checkHI (int i) {
  // if necessary, check next HH to see if it's worth making it temporary
  if (shoudchangeHH) {
    bool temp;
    if (current+i==in_size) {
      temp=false;                            // make it permanent, no other check needed (because data is over)
    } else if (HI(cur_HH)==HI(buf[current+i])) {
        temp=true;                           // before this run==first of next run
        if (HI(buf[current+i-1])==HI(buf[current+i])) {
          temp=false;                        // last of this run==first of next run
        }
    } else {
      temp=false;                            // before this run != first of next run
    }
    
    if (!writeHI(HI(buf[current]),temp))
      return (false);                       // please give me more space for output
    if (!temp) cur_HH=buf[current]&0xff00;
    shoudchangeHH=false;
  }
  
  return (true);
}

const char* STM_getName() {
	return "ShrunkTileMap (compressed)";
}

const char* STM_getExt() {
	return "stmcompr";
}

int STM_compressTilemap(uint8_t* source, uint32_t width, uint32_t height, uint8_t* dest, uint32_t destLen) {

  int i,j;
  unsigned char tmp;

  unsigned int in_size = width*height;
  buf = (unsigned short int*)source;
  outbuf = dest;
  outsize = destLen;
    
  shoudchangeHH=false;
  current=0;
  cur_HH=0;
  writepos=0;

  // write header (it's just 1 byte and it stores the width -in tiles- of the map
  if (writepos<outsize)
    outbuf[writepos++]=width;     // write map width in tiles
  else
    return (0);                   // please give me more space for output

  while (current<in_size) {
    
    // check if the HH part of the next tile is the same as what we have
    if (HI(cur_HH)!=HI(buf[current]))
      shoudchangeHH=true;
    
    if ((current+1<in_size) && (buf[current]==buf[current+1])) {
      // there are at least 2 equal word values: RLE them
      
      for (i=2;i<MAX_RLE_LEN;i++) {
        if (current+i>=in_size) break;              // leave if data ends
        if (buf[current]!=buf[current+i]) break;    // leave if no same
      }
      
      if (!checkHI(i))
        return (0);                                         // please give me more space for output
      
      if (!writeRLE(LO(buf[current]),i,RLE_TYPE_NORMAL))
        return (0);                                         // please give me more space for output
        
     
    } else if ((current+1<in_size) && ((buf[current]+1)==buf[current+1])) {
      // there are at least 2 successive word values: RLE them
      
      for (i=2;i<MAX_RLE_LEN;i++) {
        if (current+i==in_size) break;                      // leave if data ends
        if ((buf[current+i-1]+1)!=buf[current+i]) break;   // leave if no successive
      }
      
      cur_HH=buf[current+i-1]&0xff00;                       // make sure we keep the last HH  
      
      if (!checkHI(i))
        return (0);                                         // please give me more space for output
      
      if (!writeRLE(LO(buf[current]),i,RLE_TYPE_INCREMENTAL))
        return (0);                                         // please give me more space for output
      
    } else {
      // there is data we can't RLE. Oh, well...
      for (i=1;i<MAX_RAW_LEN;i++) {
        if (current+i==in_size) break;                            // leave if data ends
        if (buf[current+i-1]==buf[current+i]) {i--; break;}      // leave if found two same
        if ((buf[current+i-1]+1)==buf[current+i]) {i--; break;}  // leave if found two successive
        if (HI(buf[current+i-1])!=HI(buf[current+i])) break;     // leave if found different HI part
      }
      
      if (!checkHI(i))
        return (0);                                         // please give me more space for output
      
      tmp=LO(i)<<2;

      if (writepos<outsize)
        outbuf[writepos++]=tmp;         // write len
      else
        return (0);                     // please give me more space for output
        
      for (j=0;j<i;j++) {
        tmp=LO(buf[current+j]);
        if (writepos<outsize)
          outbuf[writepos++]=tmp;      // write raw data
        else
          return (0);                   // please give me more space for output
      }
      
    }
    
    current+=i;
  
  }  // end while
  
  tmp=0;
  if (writepos<outsize)
    outbuf[writepos++]=tmp;         // write end of data
  else
    return (0);                     // please give me more space for output

  return (writepos);                // report size to caller
}
