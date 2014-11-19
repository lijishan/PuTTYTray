/*
 * COM interface for PuTTY.
 */

#include "putty.h"
#include  <windows.h>
#include   <stdio.h>
#include   <stdlib.h>

HANDLE open_comm(char *port_name)	//�򿪴���
{
    HANDLE hComm;
    hComm = CreateFile(port_name, //���ں�
		       GENERIC_READ | GENERIC_WRITE, //������д
		       0, //ͨѶ�豸�����Զ�ռ��ʽ��
		       0, //�ް�ȫ����
		       OPEN_EXISTING, //ͨѶ�豸�Ѵ���
		       0, // ͬ��IO //FILE_FLAG_OVERLAPPED, //�첽I/O
		       0); //ͨѶ�豸������ģ���
    if (hComm == INVALID_HANDLE_VALUE)
    {
	CloseHandle(hComm);
	return FALSE;
    }
    else
	return hComm;
}


BOOL setup_DCB(HANDLE hd, int rate_arg)
{
    DCB  dcb;
    int rate= rate_arg;
    memset(&dcb, 0, sizeof(dcb));
    if(!GetCommState(hd, &dcb))
    {
        return FALSE;
    }
    /* -------------------------------------------------------------------- */
    // set DCB to configure the serial port
    dcb.DCBlength       = sizeof(dcb);
    /* ---------- Serial Port Config ------- */
    dcb.BaudRate        = rate;
    dcb.Parity      = NOPARITY;
    dcb.fParity     = 0;
    dcb.StopBits        = ONESTOPBIT;
    dcb.ByteSize        = 8;
    dcb.fOutxCtsFlow    = 0;
    dcb.fOutxDsrFlow    = 0;
    dcb.fDtrControl     = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = 0;
    dcb.fRtsControl     = RTS_CONTROL_DISABLE;
    dcb.fOutX           = 0;
    dcb.fInX            = 0;
    /* ----------------- misc parameters ----- */
    dcb.fErrorChar      = 0;
    dcb.fBinary         = 1;
    dcb.fNull           = 0;
    dcb.fAbortOnError   = 0;
    dcb.wReserved       = 0;
    dcb.XonLim          = 2;
    dcb.XoffLim         = 4;
    dcb.XonChar         = 0x13;
    dcb.XoffChar        = 0x19;
    dcb.EvtChar         = 0;
    /* -------------------------------------------------------------------- */
    // set DCB
    if(!SetCommState(hd, &dcb))
    return FALSE;
    else
    return TRUE;
}
  
/*
    ����readfile��writefile��д���п�ʱ����Ҫ���ǳ�ʱ����, ��д���ڵĳ�ʱ�����֣�
    �����ʱ���ܳ�ʱ, д����ֻ֧���ܳ�ʱ�������������ֳ�ʱ��֧��, �������
    д��ʱ������Ϊ0����ô�Ͳ�ʹ��д��ʱ��
*/
BOOL setup_timeout(HANDLE hd,
                    DWORD readInterval,
		    DWORD readTotalMultiplier,
		    DWORD readTotalConstant,
		    DWORD writeTotalMultiplier,
		    DWORD writeTotalConstant)
{
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = readInterval; //�������ʱ
    timeouts.ReadTotalTimeoutConstant = readTotalConstant; //����ʱ����
    timeouts.ReadTotalTimeoutMultiplier = readTotalMultiplier; //��ʱ��ϵ��
    timeouts.WriteTotalTimeoutConstant = writeTotalConstant; // д��ʱ����
    timeouts.WriteTotalTimeoutMultiplier = writeTotalMultiplier; //дʱ��ϵ��, �ܳ�ʱ�ļ��㹫ʽ�ǣ��ܳ�ʱ��ʱ��ϵ����Ҫ���/д���ַ�����ʱ�䳣��
    if(!SetCommTimeouts(hd, &timeouts))
	return FALSE;
    else
	return TRUE;
}  
  
read_char(HANDLE hd, BYTE* buff, DWORD *len)
{
    BOOL  bRead = TRUE;
    BOOL  bResult = TRUE;
    DWORD dwError = 0;
    DWORD BytesRead = 0;
    DWORD rlen = 0;
    char ch = 0;
    COMSTAT comstat;
    OVERLAPPED m_ov;
    memset(&m_ov, 0, sizeof(m_ov));
    for (;;)
    {
        bResult = ClearCommError(hd, &dwError, &comstat);
	if (comstat.cbInQue == 0)
  	    continue;
        // can't hold so much data
        if (comstat.cbInQue > len - rlen)
            break;
  	if (bRead)
  	{
  	    bResult = ReadFile(hd, buff+rlen, comstat.cbInQue, &BytesRead, &m_ov);
            if (BytesRead > 0)
            {
                rlen += BytesRead;
                ch = buff[rlen - 1];
                if (rlen >= *len)
                    break;
            }

	    if (!bResult)
  	    {
  	        switch (dwError = GetLastError())
  	        {
  	            case ERROR_IO_PENDING:
  	            {
  	                bRead = FALSE;
  	                break;
  	            }
  	            default:
  	            {
  	                break;
  	            }
  	        }
  	    }
  	    else
  	    {
  	        bRead = TRUE;
  	    }
            if (ch == '\r' || ch == '\n' || ch == '\x1c')
            {
                break;
            }
  	}  
        // close if (bRead)
  	if (!bRead)
  	{
  	    bRead = TRUE;
  	    bResult = GetOverlappedResult(hd,	// Handle to COMM port
  					    &m_ov,	// Overlapped structure
  				            &BytesRead,		// Stores number of bytes read
  					    TRUE); 			// Wait flag
  	    }
    }
    *len = rlen;
}
 
write_char(HANDLE hd, BYTE* m_szWriteBuffer, DWORD m_nToSend)
{
  BOOL bWrite = TRUE;
  BOOL bResult = TRUE;
  DWORD BytesSent = 0;
  //HANDLE	m_hWriteEvent;
  OVERLAPPED m_ov;
  memset(&m_ov, 0, sizeof(m_ov));
  //ResetEvent(m_hWriteEvent);
  if (bWrite)
  {
      m_ov.Offset = 0;
      m_ov.OffsetHigh = 0;
      // Clear buffer
      bResult = WriteFile(hd, m_szWriteBuffer, m_nToSend, &BytesSent, &m_ov);
      if (!bResult)  
    {
        DWORD dwError = GetLastError();
        switch (dwError)
        {
            case ERROR_IO_PENDING: //GetLastError��������
                        //ERROR_IO_PENDING����˵���ص�������δ���
            {
                // continue to GetOverlappedResults()
                BytesSent = 0;
                bWrite = FALSE;
                break;
            }
            default:
            {
                // all other error codes
                break;
            }
        }
    }
  } // end if(bResult)
  if (!bWrite)
  {
      bWrite = TRUE;
      bResult = GetOverlappedResult(hd,	// Handle to COMM port
  			        &m_ov,		// Overlapped structure
  			        &BytesSent,		// Stores number of bytes sent
  			        TRUE); 			// Wait flag
  
      // deal with the error code
      if (!bResult)
      {
        printf("GetOverlappedResults() in WriteFile()");
      }
  } // end if (!bWrite)
  
  // Verify that the data size send equals what we tried to send
  if (BytesSent != m_nToSend)
  {
    printf("WARNING: WriteFile() error.. Bytes Sent: %d; Message Length: %d\n", BytesSent, strlen((char*)m_szWriteBuffer));
  }
  return TRUE;
}
  

BOOL write_comm(int port, int rate, BYTE* buff, DWORD len, BYTE* rbuff, DWORD *rlen)
{
    HANDLE hComm;
    char name[32];
    sprintf(name, "COM%d", port);
    if (!(hComm = open_comm(name)))
    {
        printf("ERROR: Can't open port %s.", name);
        return FALSE;
    }
    if (!setup_DCB(hComm, rate))
    {
        printf("ERROR: Setup DCB failed.");
        return FALSE;
    }
    if (!setup_timeout(hComm, 0,0,0,0,0))
    {
        printf("ERROR: Setup timeout failed.");
        return FALSE;
    }
    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
    write_char(hComm, buff, len);
    read_char(hComm, rbuff, rlen);
    CloseHandle(hComm);
    return TRUE;
}
