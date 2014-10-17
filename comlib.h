#ifndef PUTTY_COMLIB_H
#define PUTTY_COMLIB_H

#define ERROR_COMM_TIMEOUT    -9
#define ERROR_COMM_OPEN       -7
#define ERROR_COMM_WRITE      -6
#define ERROR_COMM_READ       -5

BOOL CommOpen (int Port, int Baud, int Parity, int Flow);
int CommRead (char *Buffer, int Size);
int CommReadLine (char *Buffer, int Size);
int CommReadUntil (char *Buffer, int Size, char Term, int Tick);
int CommQueue (void);
int CommCall(int Port, int Baud, char *WriteBuffer, int wSize, 
               char *ReadBuffer, int rSize, char StopTerm, int Tick);
void CommTimeout (int Interval, int Multiplier, int Constant);
BOOL CommWrite (char *Buffer, int Size);
void CommClose (void);
void CommShowStatus (void);
void CommShowDcb (DCB *Dcb);

#endif