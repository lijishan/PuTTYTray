#include  <windows.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  "comlib.h"

/*
#define ASSERT_SUCCESS(Function) \
    if (!Success) { \
        fprintf(stderr, "File %s Line %d Function %s failed (%.8x)\n", \
               __FILE__, __LINE__, #Function, GetLastError()); \
        exit(1); \
    }
*/

#ifndef DEBUG
#define DEBUG if (0)
#endif

HANDLE hCom;

BOOL CommOpen (int Port, int Baud, int Parity, int Flow)
{
    char CommPath[100];
    DCB Dcb;
    BOOL Success;

    //
    // Open COM port.
    //

    sprintf(CommPath, "\\\\.\\COM%d", Port);
    fprintf(stderr, "** Opening port %s (%d baud)\n", CommPath, Baud);

    hCom = CreateFile(CommPath,
                      GENERIC_READ | GENERIC_WRITE,
                      0,
                      NULL,
                      OPEN_EXISTING,
                      0,
                      NULL);

    if (hCom == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "CreateFile failed: %s (%.8x)\n",
                CommPath, GetLastError());
        return FALSE;
    }

    //
    // Set comm parameters.
    //

    memset(&Dcb, 0x0, sizeof(DCB));
    Dcb.DCBlength = sizeof (DCB);
    Success = GetCommState(hCom, &Dcb);
    //ASSERT_SUCCESS(GetCommState);
    if (!Success)
        return FALSE;

    // Set parity
    Dcb.Parity = Parity;
    if (Parity == 0) {
        Dcb.Parity = NOPARITY;
    } else if (Parity & 1) {
        Dcb.Parity = ODDPARITY;
    } else {
        Dcb.Parity = EVENPARITY;
    }
    // Set frame size
    Dcb.BaudRate = Baud;
    Dcb.ByteSize = (Dcb.Parity == NOPARITY) ? 8 : 7;
    Dcb.StopBits = ONESTOPBIT;

    // Set raw mode
    Dcb.fDsrSensitivity = FALSE;
    Dcb.fOutxCtsFlow = FALSE;
    Dcb.fOutX = FALSE;
    Dcb.fInX = FALSE;
    Dcb.fNull = FALSE;
    Dcb.fBinary = TRUE;

    // PC write flow control
    Dcb.fOutxCtsFlow = FALSE;
    Dcb.fOutxDsrFlow = FALSE;

    // PC read flow control
    Dcb.fDtrControl = Flow ? DTR_CONTROL_HANDSHAKE : DTR_CONTROL_ENABLE;
    Dcb.fRtsControl = Flow ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;

    Success = SetCommState(hCom, &Dcb);
    //ASSERT_SUCCESS(SetCommState);
    if (!Success)
        return FALSE;
    DEBUG CommShowDcb(&Dcb);

    CommTimeout(200, 0, 0);
    SetupComm(hCom, 2048, 2048);
    DEBUG CommShowStatus();

    return TRUE;
}

void CommTimeout (int Interval, int Multiplier, int Constant)
{
    COMMTIMEOUTS Cto;
    BOOL Success;

    //
    // Set read time out value.
    //
    // ReadIntervalTimeout specifies the maximum time, in milliseconds,
    // allowed to between the arrival of two characters on communications
    // line. During a ReadFile operation, the time period begins when the
    // first character is received. If the interval between the arrival of
    // any two characters exceeds this amount, the ReadFile operation is
    // completed and buffered data is returned.
    //
    // Below sets blocking read with N millisecond interval timeout.
    //

    if (hCom != NULL) {
        Success = GetCommTimeouts(hCom, &Cto);
        //ASSERT_SUCCESS(GetCommTimeouts);
        if (!Success)
            return;

        Cto.ReadIntervalTimeout = Interval;
        Cto.ReadTotalTimeoutMultiplier = Multiplier;
        Cto.ReadTotalTimeoutConstant = Constant;

        Success = SetCommTimeouts(hCom, &Cto);
        //ASSERT_SUCCESS(SetCommTimeouts);
        if (!Success)
            return;
    }

    return;
}

BOOL CommWrite (char *Buffer, int Size)
{
    int Count;
    BOOL Success;

    Success = WriteFile(hCom, Buffer, Size, &Count, 0);
    //ASSERT_SUCCESS(WriteFile);
    if (!Success)
        return FALSE;

    if (Count != Size) {
        fprintf(stderr, "WriteFile Count (%d) != Size (%d)\n", Count, Size);
        return FALSE;
    }

    return TRUE;
}

int CommRead (char *Buffer, int Size)
{
    int Count;
    BOOL Success;

    Success = ReadFile(hCom, Buffer, Size, &Count, 0);
    //ASSERT_SUCCESS(ReadFile);
    if (!Success)
        return ERROR_COMM_READ;

    return Count;
}

int CommReadLine (char *Buffer, int Size)
{
    int Count;
    char *p;
    int n;
    int Total;
    BOOL Success;

    n = Size;
    p = Buffer;
    Total = 0;
    do {
        Success = ReadFile(hCom, p, n, &Count, 0);
        //ASSERT_SUCCESS(ReadFile);
        if (!Success)
            return ERROR_COMM_READ;
        Total += Count;
        if ((Count == 0) || (p[Count - 1] == '\n')) {
            break;
        }
        p += Count;
        n -= Count;

    } while (n);

    return Total;
}

int CommReadUntil (char *Buffer, int Size, char Term, int Ticks)
{
    char c;
    int Count;
    int Index;
    BOOL Success;
    int first = 1;
    unsigned int Timeout;

    if (first) {
        COMMTIMEOUTS Cto;
        BOOL Success;

        first = 0;

        Success = GetCommTimeouts(hCom, &Cto);
        //ASSERT_SUCCESS(GetCommTimeouts);
        if (!Success)
            return ERROR_COMM_READ;
    
        Cto.ReadIntervalTimeout = 5;
        Cto.ReadTotalTimeoutMultiplier = 0;
        Cto.ReadTotalTimeoutConstant = 25;
    
        Success = SetCommTimeouts(hCom, &Cto);
        //ASSERT_SUCCESS(SetCommTimeouts);
        if (!Success)
            return ERROR_COMM_READ;
    }

    Timeout = GetTickCount() + Ticks;
    Index = 0;
    while (Index < Size) {
        Success = ReadFile(hCom, &c, 1, &Count, 0);
        if (!Success) {
            return ERROR_COMM_READ;
        }
        //ASSERT_SUCCESS(ReadFile);
        if (Count == 0) {
            if (Ticks != 0 && GetTickCount() >= Timeout) {
                Buffer[Index] = '\0';
                if (Term == '\0')
                    return Index;
                return ERROR_COMM_TIMEOUT;
            }

        } else {
            Buffer[Index] = c;
            Index += 1;
            Timeout = GetTickCount() + Ticks;
            if (Term != '\0' && c == Term) {
                break;
            }
        }
    }
    Buffer[Index] = '\0';

    return Index;
}

int CommQueue (void)
{
    COMSTAT ComStat;
    int Errors;
    BOOL Success;

    Success = ClearCommError(hCom, &Errors, &ComStat);
    //ASSERT_SUCCESS(ClearCommError);
    if (!Success)
        return ERROR_COMM_READ;

    return ComStat.cbInQue;
}

/*
 * Port: Port Number
 * Baud: Baud rate
 * WriteBuffer: buffer to write
 * wSize: number of bytes to write
 * ReadBuffer: buffer for reading
 * rSize: size of ReadBuffer
 * StopTerm: stopping char for reading
 * Tick: timeout ticks, 0 for reading until the StopTerm matched
 */
int CommCall(int Port, int Baud, char *WriteBuffer, int wSize, 
               char *ReadBuffer, int rSize, char StopTerm, int Tick)
{
    BOOL ret;
    int BytesRead;
    ret = CommOpen(Port, Baud, 0, 0);
    if (!ret) {
        hCom = NULL;
        // error opening port
        return ERROR_COMM_OPEN;
    }
    ret = CommWrite(WriteBuffer, wSize);
    if (!ret) {
        CommClose();
        // error write to port
        return ERROR_COMM_WRITE;
    }

    BytesRead = CommReadUntil(ReadBuffer, rSize, StopTerm, Tick);
    CommClose();

    return BytesRead;
}

#define COMM_CLOSE

void CommClose (void)
{
    DEBUG CommShowStatus();
    CloseHandle(hCom);
    hCom = NULL;
}

void CommShowStatus (void)
{
    COMMPROP CommProp;
    COMSTAT ComStat;
    int Errors;
    int Mask;
    int Modem;
    BOOL Success;

    Success = GetCommProperties(hCom, &CommProp);
    //ASSERT_SUCCESS(GetCommProperties);
    if (!Success)
        return;

#define SHOW_COMPROP(x) printf("%20s %ld\n", #x, CommProp.x);

    SHOW_COMPROP(wPacketLength);
    SHOW_COMPROP(wPacketVersion);
    SHOW_COMPROP(dwServiceMask);
    SHOW_COMPROP(dwReserved1);
    SHOW_COMPROP(dwMaxTxQueue);
    SHOW_COMPROP(dwMaxRxQueue);
    SHOW_COMPROP(dwMaxBaud);
    SHOW_COMPROP(dwProvSubType);
    SHOW_COMPROP(dwProvCapabilities);
    SHOW_COMPROP(dwSettableParams);
    SHOW_COMPROP(dwSettableBaud);
    SHOW_COMPROP(wSettableData);
    SHOW_COMPROP(wSettableStopParity);
    SHOW_COMPROP(dwCurrentTxQueue);
    SHOW_COMPROP(dwCurrentRxQueue);
    SHOW_COMPROP(dwProvSpec1);
    SHOW_COMPROP(dwProvSpec2);
    SHOW_COMPROP(wcProvChar[1]);

    Success = GetCommMask(hCom, &Mask);
    //ASSERT_SUCCESS(GetCommMask);

    printf("Mask   = 0x%.8lx\n", Mask);

    Success = ClearCommError(hCom, &Errors, &ComStat);
    //ASSERT_SUCCESS(ClearCommError);

    printf("Errors = 0x%.8lx", Errors);
    if (Errors & CE_BREAK) printf(" BREAK");
    if (Errors & CE_FRAME) printf(" FRAME");
    if (Errors & CE_IOE) printf(" IOE");
    if (Errors & CE_OVERRUN) printf(" OVERRUN");
    if (Errors & CE_RXOVER) printf(" RXOVER");
    if (Errors & CE_RXPARITY) printf(" RXPARITY");
    if (Errors & CE_TXFULL) printf(" TXFULL");
    printf("\n");

#define SHOW_COMSTAT(x) printf("%20s %ld\n", #x, ComStat.x);

    SHOW_COMSTAT(fCtsHold);
    SHOW_COMSTAT(fDsrHold);
    SHOW_COMSTAT(fRlsdHold);
    SHOW_COMSTAT(fXoffHold);
    SHOW_COMSTAT(fXoffSent);
    SHOW_COMSTAT(fEof);
    SHOW_COMSTAT(fTxim);
    SHOW_COMSTAT(fReserved);
    SHOW_COMSTAT(cbInQue);
    SHOW_COMSTAT(cbOutQue);

    Success = GetCommModemStatus(hCom, &Modem);
    //ASSERT_SUCCESS(GetCommModemStatus);

    printf("Modem  = 0x%.8lx", Modem);
    if (Modem & MS_CTS_ON) printf(" CTS_ON");   // CTS
    if (Modem & MS_DSR_ON) printf(" DSR_ON");   // DSR
    if (Modem & MS_RING_ON) printf(" RING_ON"); // RI
    if (Modem & MS_RLSD_ON) printf(" RLSD_ON"); // DCD
    printf("\n");

    return;
}

void CommShowDcb (DCB *Dcb)
{
    char *f = "%8d %s\n";

    printf(f, Dcb->DCBlength,
            "int DCBlength;       sizeof(DCB)");
    printf(f, Dcb->BaudRate,
            "int BaudRate;        Baudrate at which running");
    printf(f, Dcb->fBinary,
            "int fBinary: 1;      Binary Mode (skip EOF check)");
    printf(f, Dcb->fParity,
            "int fParity: 1;      Enable parity checking");
    printf(f, Dcb->fOutxCtsFlow,
            "int fOutxCtsFlow:1;  CTS handshaking on output");
    printf(f, Dcb->fOutxDsrFlow,
            "int fOutxDsrFlow:1;  DSR handshaking on output");
    printf(f, Dcb->fDtrControl,
            "int fDtrControl:2;   DTR Flow control");
    printf(f, Dcb->fDsrSensitivity,
            "int fDsrSensitivity:1; DSR Sensitivity");
    printf(f, Dcb->fTXContinueOnXoff,
            "int fTXContinueOnXoff:1; Continue TX when Xoff sent");
    printf(f, Dcb->fOutX,
            "int fOutX: 1;        Enable output X-ON/X-OFF");
    printf(f, Dcb->fInX,
            "int fInX: 1;         Enable input X-ON/X-OFF");
    printf(f, Dcb->fErrorChar,
            "int fErrorChar: 1;   Enable Err Replacement");
    printf(f, Dcb->fNull,
            "int fNull: 1;        Enable Null stripping");
    printf(f, Dcb->fRtsControl,
            "int fRtsControl:2;   Rts Flow control");
    printf(f, Dcb->fAbortOnError,
            "int fAbortOnError:1; Abort all reads and writes on Error");
    printf(f, Dcb->fDummy2,
            "int fDummy2:17;      Reserved");
    printf(f, Dcb->wReserved,
            "WORD wReserved;        Not currently used");
    printf(f, Dcb->XonLim,
            "WORD XonLim;           Transmit X-ON threshold");
    printf(f, Dcb->XoffLim,
            "WORD XoffLim;          Transmit X-OFF threshold");
    printf(f, Dcb->ByteSize,
            "BYTE ByteSize;         Number of bits/byte, 4-8");
    printf(f, Dcb->Parity,
            "BYTE Parity;           0-4=None,Odd,Even,Mark,Space");
    printf(f, Dcb->StopBits,
            "BYTE StopBits;         0,1,2 = 1, 1.5, 2");
    printf(f, Dcb->XonChar,
            "char XonChar;          Tx and Rx X-ON character");
    printf(f, Dcb->XoffChar,
            "char XoffChar;         Tx and Rx X-OFF character");
    printf(f, Dcb->ErrorChar,
            "char ErrorChar;        Error replacement char");
    printf(f, Dcb->EofChar,
            "char EofChar;          End of Input character");
    printf(f, Dcb->EvtChar,
            "char EvtChar;          Received Event character");
    printf(f, Dcb->wReserved,
            "WORD wReserved1;       Fill for now.");
    fflush(stdout);
}
