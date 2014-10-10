/*
 * COM interface for PuTTY.
 */

#include "putty.h"
#include  <windows.h>
#include   <stdio.h>
#include   <stdlib.h>

HANDLE open_comm(char *port_name)	//打开串口
{
    HANDLE hComm;
    hComm = CreateFile(port_name, //串口号
		       GENERIC_READ | GENERIC_WRITE, //允许读写
		       0, //通讯设备必须以独占方式打开
		       0, //无安全属性
		       OPEN_EXISTING, //通讯设备已存在
		       0, // 同步IO //FILE_FLAG_OVERLAPPED, //异步I/O
		       0); //通讯设备不能用模板打开
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
    在用readfile和writefile读写串行口时，需要考虑超时问题, 读写串口的超时有两种：
    间隔超时和总超时, 写操作只支持总超时，而读操作两种超时均支持, 如果所有
    写超时参数均为0，那么就不使用写超时。
*/
BOOL setup_timeout(HANDLE hd,
                    DWORD readInterval,
		    DWORD readTotalMultiplier,
		    DWORD readTotalConstant,
		    DWORD writeTotalMultiplier,
		    DWORD writeTotalConstant)               
{
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = readInterval; //读间隔超时
    timeouts.ReadTotalTimeoutConstant = readTotalConstant; //读超时常量
    timeouts.ReadTotalTimeoutMultiplier = readTotalMultiplier; //读时间系数
    timeouts.WriteTotalTimeoutConstant = writeTotalConstant; // 写超时常量
    timeouts.WriteTotalTimeoutMultiplier = writeTotalMultiplier; //写时间系数, 总超时的计算公式是：总超时＝时间系数×要求读/写的字符数＋时间常量
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
    char ch;
    COMSTAT comstat;
    OVERLAPPED m_ov;
    memset(&m_ov, 0, sizeof(m_ov));
    for (;;)
    {
        //memset(&comstat, 0, sizeof(comstat));
	// 在使用ReadFile 函数进行读操作前，应先使用ClearCommError函数清除错误
        bResult = ClearCommError(hd, &dwError, &comstat);
	if (comstat.cbInQue == 0)// COMSTAT结构返回串口状态信息
    	    //本文只用到了cbInQue成员变量，该成员变量的值代表输入缓冲区的字节数
  	    continue;
  	if (bRead)
  	{
  	    bResult = ReadFile(hd, &ch, 1, &BytesRead, &m_ov);
            if (ch == '\r' || ch == '\n')
            {
                break;
            }
            if (BytesRead == 1)
            {
                buff[rlen++] = ch;
                if (rlen >= *len)
                    break;
            }

	    if (!bResult)// 当ReadFile和WriteFile返回FALSE时，不一定就是操作失败，线程应该调用GetLastError函数分析返回的结果
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
    *len = rlen;
}
 
write_char(HANDLE hd, BYTE* m_szWriteBuffer,DWORD m_nToSend)
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
      bResult = WriteFile(hd,	// Handle to COMM Port, 串口的句柄
  
  		       m_szWriteBuffer,	// Pointer to message buffer in calling finction
                    // 即以该指针的值为首地址的nNumberOfBytesToWrite
                                 // 个字节的数据将要写入串口的发送数据缓冲区
  
  		       m_nToSend,	// Length of message to send, 要写入的数据的字节数
  
  		       &BytesSent,	 // Where to store the number of bytes sent
                  // 指向指向一个DWORD数值，该数值返回实际写入的字节数
              
  		       &m_ov );	    // Overlapped structure
                  // 重叠操作时，该参数指向一个OVERLAPPED结构，
                            // 同步操作时，该参数为NULL
      // 当ReadFile和WriteFile返回FALSE时，不一定就是操作失
   //败，线程应该调用GetLastError函数分析返回的结果
      if (!bResult)  
    {
        DWORD dwError = GetLastError();
        switch (dwError)
        {
            case ERROR_IO_PENDING: //GetLastError函数返回
                        //ERROR_IO_PENDING。这说明重叠操作还未完成
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
  

BOOL write_comm(int port, BYTE* buff, DWORD len, BYTE* rbuff, DWORD *rlen)
{
    HANDLE hComm;
    char name[32];
    sprintf(name, "COM%d", port);
    if (!(hComm = open_comm(name)))
    {
        printf("ERROR: Can't open port %s.", name);
        return FALSE;
    }
    if (!setup_DCB(hComm, 4800))
    {
        printf("ERROR: Setup DCB failed.");
        return FALSE;
    }
    if (!setup_timeout(hComm, 0,0,3,0,0))
    {
        printf("ERROR: Setup timeout failed.");
        return FALSE;
    }
    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
    write_char(hComm, buff, len);
    printf("received data:\n");
    read_char(hComm, rbuff, rlen);
    CloseHandle(hComm);
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