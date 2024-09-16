//============================================================================
//
//% Student Name 1: student1
//% Student 1 #: 123456781
//% Student 1 userid (email): stu1 (stu1@sfu.ca)
//
//% Student Name 2: student2
//% Student 2 #: 123456782
//% Student 2 userid (email): stu2 (stu2@sfu.ca)
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: _everybody helped us/me with the assignment (list names or put 'None')__
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  ___________
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P1_<userid1>_<userid2>" (eg. P1_stu1_stu2)
//% * Form groups as described at:  https://coursys.sfu.ca/docs/students
//% * Submit files to coursys.sfu.ca
//
// File Name   : SenderY.cpp
// Version     : September 5, 2024
// Description : Starting point for ENSC 351 Project
// Original portions Copyright (c) 2024 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include "SenderY.h"

#include <iostream>
#include <filesystem>
#include <stdio.h> // for snprintf()
#include <stdint.h> // for uint8_t
#include <string.h> // for memset(), and memcpy() or strncpy()
#include <errno.h>
#include <fcntl.h>	// for O_RDWR or O_RDONLY
#include <sys/stat.h>

#include "myIO.h"

using namespace std;
using namespace std::filesystem; // C++17 and beyond

SenderY::
SenderY(vector<const char*> iFileNames, int d)
:PeerY(d),
 bytesRd(-1), 
 fileNames(iFileNames),
 fileNameIndex(0),
 blkNum(0)
{
}

//-----------------------------------------------------------------------------

/* generate a block (numbered 0) with filename and filesize */
//void SenderY::genStatBlk(blkT blkBuf, const char* fileName)
void SenderY::genStatBlk(uint8_t blkBuf[BLK_SZ_CRC], const char* fileName)
{
   for (int i = 0; i < BLK_SZ_CRC; ++i) {
      blkBuf[i] = 0;
   }

   int i = 0;
   while (fileName[i] != '\0' && i < CHUNK_SZ - 3) {
      blkBuf[3 + i] = fileName[i];
      ++i;
   }

   blkBuf[3 + i] = '\0';

   struct stat st;

   if (stat(fileName, &st) == 0) {
      unsigned fileSize = st.st_size;

      char fileSizeStr[20];
      int j = 0;
      do {
         fileSizeStr[j++] = (fileSize % 10) + '0';
         fileSize /= 10;
      } while (fileSize > 0);

      for (int k = 0; k < j / 2; ++k) {
         char temp = fileSizeStr[k];
         fileSizeStr[k] = fileSizeStr[j - k - 1];
         fileSizeStr[j - k - 1] = temp;
      }
      fileSizeStr[j] = '\0';

      int offset = 3 + i + 1;
      int l = 0;
      while (fileSizeStr[l] != '\0' && offset + l < CHUNK_SZ) {
         blkBuf[offset + l] = fileSizeStr[l];
         ++l;
      }
   }
   else {
      cout << "Error: " << fileName << endl;
      return;
   }

   uint16_t myCrc16ns;
   crc16ns(&myCrc16ns, &blkBuf[0]);

   blkBuf[BLK_SZ_CRC - 2] = (myCrc16ns >> 8) & 0xFF;
   blkBuf[BLK_SZ_CRC - 1] = myCrc16ns & 0xFF;
}

/* tries to generate a block.  Updates the
variable bytesRd with the number of bytes that were read
from the input file in order to create the block. Sets
bytesRd to 0 and does not actually generate a block if the end
of the input file had been reached when the previously generated block
was prepared or if the input file is empty (i.e. has 0 length).
 */
//void SenderY::genBlk(blkT blkBuf)
void SenderY::genBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
   bytesRd = 0;

   bytesRd = myRead(transferringFileD, &blkBuf[0], CHUNK_SZ);

   if (bytesRd == -1) {
      ErrorPrinter("myRead(transferringFileD, &blkBuf[0], CHUNK_SZ )", __FILE__, __LINE__, errno);
      return;
   }

   if (bytesRd < CHUNK_SZ) {
      for (int i = bytesRd; i < CHUNK_SZ; ++i) {
         blkBuf[i] = 0;
      }
   }

   uint16_t myCrc16ns;
   crc16ns(&myCrc16ns, &blkBuf[0]);

   blkBuf[BLK_SZ_CRC - 2] = (myCrc16ns >> 8) & 0xFF;
   blkBuf[BLK_SZ_CRC - 1] = myCrc16ns & 0xFF;
}

void SenderY::cans()
{
   for (int i = 0; i < CAN_LEN; ++i) {
      uint8_t canChar = 0x18;  // CAN character (cancel character)
      if (write(mediumD, &canChar, 1) == -1) {
         ErrorPrinter("write(mediumD, &canChar, 1)", __FILE__, __LINE__, errno);
         return;
      }
   }
}

//uint8_t SenderY::sendBlk(blkT blkBuf)
void SenderY::sendBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
   int bytesWritten = write(mediumD, blkBuf, BLK_SZ_CRC);

   if (bytesWritten == -1) {
      ErrorPrinter("write(mediumD, blkBuf, BLK_SZ_CRC)", __FILE__, __LINE__, errno);
   }
   else if (bytesWritten != BLK_SZ_CRC) {
      cout << "Warning: Only " << bytesWritten << " bytes written, expected " << BLK_SZ_CRC << endl;
   }
}

void SenderY::statBlk(const char* fileName)
{
    blkNum = 0;
    // assume 'C' received from receiver to enable sending with CRC
    genStatBlk(blkBuf, fileName); // prepare 0eth block
    sendBlk(blkBuf); // send 0eth block
    // assume sent block will be ACK'd
}

void SenderY::sendFiles()
{
    for (const auto fileName : fileNames) {
        transferringFileD = myOpen(fileName, O_RDONLY, 0);
        if(-1 == transferringFileD) {
            cans();
            cout /* cerr */ << "Error opening input file named: " << fileName << endl;
            result += "OpenError";
            return;
        }
        else {
            cout << "Sender will send " << fileName << endl;

            // do the protocol, and simulate a receiver that positively acknowledges every
            //	block that it receives.

            statBlk(fileName);

            // assume 'C' received from receiver to enable sending with CRC
            genBlk(blkBuf); // prepare 1st block
            while (bytesRd)
            {
                ++ blkNum; // 1st block about to be sent or previous block was ACK'd

                sendBlk(blkBuf); // send block

                // assume sent block will be ACK'd
                genBlk(blkBuf); // prepare next block
                // assume sent block was ACK'd
            };
            // finish up the file transfer, assuming the receiver behaves normally and there are no transmission errors
            // TODO: ********* fill in some code here ***********

            //(myClose(transferringFileD));
            if (-1 == myClose(transferringFileD))
                ErrorPrinter("myClose(transferringFileD)", __FILE__, __LINE__, errno);
            result += "Done, ";
        }
    }
    // indicate end of the batch.
    statBlk("");

    // remove ", " from the end of the result string.
    if (result.size())
        result.erase(result.size() - 2);
}

