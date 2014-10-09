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
		       GENERIC_READ | GENERIC_WRITE, //�����д
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
		    DWORD readTotalconstant,
		    DWORD writeTotalMultiplier,
		    DWORD writeTotalConstant)               
{
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = readInterval; //�������ʱ
    timeouts.ReadTotalTimeoutConstant = readTotalconstant; //����ʱ����
    timeouts.ReadTotalTimeoutMultiplier = readTotalMultiplier; //��ʱ��ϵ��
    timeouts.WriteTotalTimeoutConstant = writeTotalConstant; // д��ʱ����
    timeouts.WriteTotalTimeoutMultiplier = writeTotalMultiplier; //дʱ��ϵ��, �ܳ�ʱ�ļ��㹫ʽ�ǣ��ܳ�ʱ��ʱ��ϵ����Ҫ���/д���ַ�����ʱ�䳣��
    if(!SetCommTimeouts(hd, &timeouts))
	return FALSE;
    else
	return TRUE;
}  
  
read_char(HANDLE hd)
{
    BOOL  bRead = TRUE;
  BOOL  bResult = TRUE;
  DWORD dwError = 0;
  DWORD BytesRead = 0;
  char RXBuff;
  for (;;)
  {
	// ��ʹ��ReadFile �������ж�����ǰ��Ӧ��ʹ��ClearCommError�����������
        bResult = ClearCommError(hd, &dwError, &comstat);
	if (comstat.cbInQue == 0)// COMSTAT�ṹ���ش���״̬��Ϣ
    	    //����ֻ�õ���cbInQue��Ա�������ó�Ա������ֵ�������뻺�������ֽ���
  	    continue;
  	if (bRead)
  	{
  	  bResult = ReadFile(hd, // Handle to COMM port���ڵľ��
              	  &RXBuff,	// RX Buffer Pointer
                   		// ��������ݴ洢�ĵ�ַ������������ݽ���
  						// �����Ը�ָ���ֵΪ�׵�ַ��һƬ�ڴ���
  				  1,		// ��ȡһ���ֽ�
  				  &BytesRead,	// Stores number of bytes read, ָ��һ��DWORD
       					//��ֵ������ֵ���ض�����ʵ�ʶ�����ֽ���
  				  &m_ov);	// �ص�����ʱ���ò���ָ��һ��OVERLAPPED�ṹ��ͬ������ʱ���ò���ΪNULL
            printf("%c",RXBuff);
	    if (!bResult)// ��ReadFile��WriteFile����FALSEʱ����һ�����ǲ���ʧ�ܣ��߳�Ӧ�õ���GetLastError�����������صĽ��
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
}
 
write_char(HANDLE hd, BYTE* m_szWriteBuffer,DWORD m_nToSend)
{
  BOOL bWrite = TRUE;
  BOOL bResult = TRUE;
  DWORD BytesSent = 0;
  HANDLE	m_hWriteEvent;
  ResetEvent(m_hWriteEvent);
  if (bWrite)
  {
      m_ov.Offset = 0;
      m_ov.OffsetHigh = 0;
      // Clear buffer
      bResult = WriteFile(hd,	// Handle to COMM Port, ���ڵľ��
  
  		       m_szWriteBuffer,	// Pointer to message buffer in calling finction
                    // ���Ը�ָ���ֵΪ�׵�ַ��nNumberOfBytesToWrite
                                 // ���ֽڵ����ݽ�Ҫд�봮�ڵķ������ݻ�����
  
  		       m_nToSend,	// Length of message to send, Ҫд������ݵ��ֽ���
  
  		       &BytesSent,	 // Where to store the number of bytes sent
                  // ָ��ָ��һ��DWORD��ֵ������ֵ����ʵ��д����ֽ���
              
  		       &m_ov );	    // Overlapped structure
                  // �ص�����ʱ���ò���ָ��һ��OVERLAPPED�ṹ��
                            // ͬ������ʱ���ò���ΪNULL
      // ��ReadFile��WriteFile����FALSEʱ����һ�����ǲ���ʧ
   //�ܣ��߳�Ӧ�õ���GetLastError�����������صĽ��
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
  

BOOL write_com(int port, BYTE* buff, DWORD len)
{
    BOOL ok;
    HANDLE hComm;
    sprintf(port_name, "COM%d", term->esc_args[0]);
    if (!(hComm = open_port(port_name)))
    {
        printf("ERROR: Can't open port %s.", port_name);
        return FALSE;
    }
    if (!setup_DCB(4800))
    {
        printf("ERROR: Setup DCB failed.");
        return FALSE;
    }
    if (!setup_timeout(0,0,0,0,0))
    {
        printf("ERROR: Setup timeout failed.");
        return FALSE;
    }
    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
    write_char("please send data now",20);
    printf("received data:\n");
  read_char();
    return TRUE;
}

/*
void   main()
{
    int open;
    open = open_port("COM2");
    if(open)
	printf("open comport success");
    system("pause") ;
}
*/