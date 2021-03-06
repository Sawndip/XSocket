/*
 * Copyright: 7thTool Open Source <i7thTool@qq.com>
 * All rights reserved.
 * 
 * Author	: Scott
 * Email	：i7thTool@qq.com
 * Blog		: http://blog.csdn.net/zhangzq86
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _H_XIOCPSOCKETEX_H_
#define _H_XIOCPSOCKETEX_H_

#include "XSocketEx.h"
#include <MSWSock.h>

namespace XSocket {

#define IOCP_OPERATION_EXIT DWORD(-1)
#define IOCP_OPERATION_NOTIFY DWORD(-2)
#define IOCP_OPERATION_NOTIFYDATA DWORD(-3)
#define IOCP_OPERATION_TRYRECEIVE DWORD(-4)
#define IOCP_OPERATION_TRYSEND DWORD(-5)
#define IOCP_OPERATION_TRYACCEPT DWORD(-6)

/*!
 *	@brief CompletionPort OVERLAPPED 定义.
 *
 *	ERROR_PORT_UNREACHABLE	1234	No service is operating at the destination network endpoint on the remote system.
 *	ERROR_OPERATION_ABORTED	995		The I/O operation has been aborted because of either a thread exit or an application request.
 */
enum 
{
	IOCP_OPERATION_NONE = 0,
	IOCP_OPERATION_ACCEPT,
	IOCP_OPERATION_CONNECT,
	IOCP_OPERATION_RECEIVE,
	IOCP_OPERATION_SEND,
};
typedef struct _PER_IO_OPERATION_DATA
{ 
	WSAOVERLAPPED	Overlapped;
	WSABUF			Buffer;
	byte			OperationType;
	union 
	{
		DWORD		Flags;
		SOCKET		Sock; //Accept
	};
	union
	{
		DWORD		NumberOfBytesCompleted;
		DWORD		NumberOfBytesReceived;
		DWORD		NumberOfBytesSended;
	};
}PER_IO_OPERATION_DATA, *LPPER_IO_OPERATION_DATA;

/*!
 *	@brief CompletionPortSocket 模板定义.
 *
 *	封装CompletionPortSocket
 */
template<class TSocketSet, class TBase = SocketEx>
class CompletionPortSocketT : public TBase
{
	typedef TBase Base;
public:
	typedef TSocketSet SocketSet;
protected:
	//优化成区分WORK、CONNECT、LISTEN、UDP
	union 
	{
		PER_IO_OPERATION_DATA send_overlapped_;
		char send_reserved_[sizeof(SOCKADDR_STORAGE) + sizeof(SOCKADDR_STORAGE)];
	};
	union 
	{
		PER_IO_OPERATION_DATA receive_overlapped_;
		char receive_reserved_[sizeof(PER_IO_OPERATION_DATA)];
	};
public:
	static SocketSet* service() { return dynamic_cast<SocketSet*>(SocketSet::service()); }

	CompletionPortSocketT():Base()
	{
		memset(&send_reserved_,0,sizeof(send_reserved_));
		memset(&receive_reserved_,0,sizeof(receive_reserved_));
	}

	virtual ~CompletionPortSocketT()
	{
		
	}

	SOCKET Open(int nSockAf = AF_INET, int nSockType = SOCK_STREAM, int nSockProtocol = 0)
	{
		SOCKET Sock = WSASocket(nSockAf, nSockType, nSockProtocol, NULL, 0, WSA_FLAG_OVERLAPPED);
		// DWORD dwBytes = 0;
		// do {
		// 	// 获取AcceptEx函数指针
		// 	dwBytes = 0;
		// 	GUID GuidAcceptEx = WSAID_ACCEPTEX;
		// 	if (0 != WSAIoctl(Sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		// 					&GuidAcceptEx, sizeof(GuidAcceptEx),
		// 					&lpfnAcceptEx, sizeof(lpfnAcceptEx), &dwBytes, NULL, NULL)) {
		// 		PRINTF("WSAIoctl AcceptEx is failed. Error=%d", GetLastError());
		// 		break;
		// 	}
		// 	// 获取GetAcceptExSockAddrs函数指针
		// 	dwBytes = 0;
		// 	GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
		// 	if (0 != WSAIoctl(Sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		// 					&GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
		// 					&lpfnGetAcceptExSockaddrs, sizeof(lpfnGetAcceptExSockaddrs), &dwBytes, NULL, NULL)) {
		// 		PRINTF("WSAIoctl GetAcceptExSockaddrs is failed. Error=%d", GetLastError());
		// 		break;
		// 	}
		// 	//获得ConnectEx 函数的指针
		// 	dwBytes = 0;
		// 	GUID GuidConnectEx = WSAID_CONNECTEX;
		// 	if (SOCKET_ERROR == WSAIoctl(Sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		// 		&GuidConnectEx, sizeof(GuidConnectEx ),
		// 		&lpfnConnectEx, sizeof (lpfnConnectEx), &dwBytes, 0, 0)) {
		// 		PRINTF("WSAIoctl ConnectEx is failed. Error=%d", GetLastError());
		// 		break;
		// 	}
		// } while(false);
		// if(!lpfnAcceptEx || !lpfnGetAcceptExSockaddrs || !lpfnConnectEx) {
		// 	XSocket::Socket::Close(Sock);
		// 	return INVALID_SOCKET;
		// }
		int nRole = SOCKET_ROLE_NONE;
		if ((nSockAf==AF_INET&&nSockType==SOCK_DGRAM)) {
			nRole = SOCKET_ROLE_WORK;
		}
		return Attach(Sock, nRole);
	}

	int Connect(const SOCKADDR* lpSockAddr, int nSockAddrLen)
	{
		ASSERT(IsSocket());
		OnRole(SOCKET_ROLE_CONNECT);
		role_ = SOCKET_ROLE_CONNECT;
		event_ |= FD_CONNECT;
		SetNonBlock();//设为非阻塞模式

		switch (lpSockAddr->sa_family)
		{
		/*case AF_INET:
		case AF_INET6:
		{
			DWORD ipv6only = 0;
			if (SetSockOpt(IPPROTO_IPV6, IPV6_V6ONLY, (char *)&ipv6only, sizeof(ipv6only)) == SOCKET_ERROR) {
				return SOCKET_ERROR;
			}

			char NodeName[64] = {0};
			SockAddr2IpStr(lpSockAddr, nSockAddrLen, NodeName, 64);
			char PortName[16] = {0};
			SockAddr2PortStr(lpSockAddr, nSockAddrLen, PortName, 16);
			SOCKADDR_STORAGE LocalAddr = {0};
			SOCKADDR_STORAGE RemoteAddr = {0};
			DWORD dwLocalAddr = sizeof(LocalAddr);
			DWORD dwRemoteAddr = sizeof(RemoteAddr);
			//struct timeval timeout = {0};
			PER_IO_OPERATION_DATA* pOverlapped = &send_overlapped_;
			memset(&pOverlapped->Overlapped, 0, sizeof(WSAOVERLAPPED));
			pOverlapped->Buffer.buf	= NULL;
			pOverlapped->Buffer.len	= 0;
			SockAddr2IpStr(lpSockAddr, nSockAddrLen, pOverlapped->NodeName, 64);
			SockAddr2PortStr(lpSockAddr, nSockAddrLen, pOverlapped->PortName, 16);
			pOverlapped->Flags = 0;
			pOverlapped->OperationType = IOCP_OPERATION_CONNECT;
			pOverlapped->NumberOfBytesSended = 0;
			if(WSAConnectByName((SOCKET)*this, pOverlapped->NodeName, pOverlapped->PortName
			//, &dwLocalAddr, (SOCKADDR *)&LocalAddr, &dwRemoteAddr, (SOCKADDR *)&RemoteAddr
			, &pOverlapped->dwLocalAddr, (SOCKADDR *)&pOverlapped->LocalAddr, &pOverlapped->dwRemoteAddr, (SOCKADDR *)&pOverlapped->RemoteAddr
			, NULL
			//, NULL
			, &pOverlapped->Overlapped
			)) {
				return 0;
			} else {
				int nError = GetLastError();
				char szErrorMessage[1024] = {0};
				GetErrorMessage(nError, szErrorMessage, 1024);
				PRINTF("WSAConnectByName is failed. Error=%d:%s", nError, szErrorMessage);
				return SOCKET_ERROR;
			}
		}
		break;*/
		default:
		{
			static LPFN_CONNECTEX lpfnConnectEx = [&] {
			DWORD dwBytes = 0;
			do {
				//获得ConnectEx 函数的指针
				dwBytes = 0;
				GUID GuidConnectEx = WSAID_CONNECTEX;
				if (SOCKET_ERROR == WSAIoctl((SOCKET)*this, SIO_GET_EXTENSION_FUNCTION_POINTER,
					&GuidConnectEx, sizeof(GuidConnectEx ),
					&lpfnConnectEx, sizeof (lpfnConnectEx), &dwBytes, 0, 0)) {
					PRINTF("WSAIoctl ConnectEx is failed. Error=%d", GetLastError());
					break;
				}
			} while(false);
			return lpfnConnectEx;
			}();
			if(!lpfnConnectEx) {
				return SOCKET_ERROR;
			}

			//MSDN说The parameter s is an unbound or a listening socket，
			//还是诡异两个字connect操作干嘛要绑定？不知道，没人给解释，那绑定就对了，那么绑哪个？
			//最好把你的地址结构像下面这样设置
			//为什么端口这个地方用0，原因很简单，你去查查MSDN，这样表示他会在1000-4000这个范围（可能记错，想了解的话去查MSDN）找一个没被用到的port，
			//这样的话最大程度保证你bind的成功，然后再把socket句柄丢给IOCP，然后调用ConnectEx这样就会看到熟悉的WSA_IO_PENDING了！
			switch(lpSockAddr->sa_family)
			{
			case AF_INET6:
			{
				SOCKADDR_IN6 addr = {0};
				addr.sin6_family = AF_INET6;
				addr.sin6_port = 0;
				addr.sin6_addr = in6addr_any;
				Bind((const SOCKADDR*)&addr,sizeof(SOCKADDR_IN6));
			}
			break;
			default:
			{	
				SOCKADDR_IN addr = {0};
				addr.sin_family = AF_INET;
				addr.sin_port = htons(0);
				addr.sin_addr.s_addr = htonl(ADDR_ANY);//INADDR_ANY
				Bind((const SOCKADDR*)&addr,sizeof(SOCKADDR_IN));
			}
			break;
			}

			PER_IO_OPERATION_DATA* pOverlapped = &send_overlapped_;
			memset(&pOverlapped->Overlapped, 0, sizeof(WSAOVERLAPPED));
			pOverlapped->Buffer.buf	= NULL;
			pOverlapped->Buffer.len	= 0;
			pOverlapped->Flags = 0;
			pOverlapped->OperationType = IOCP_OPERATION_CONNECT;
			pOverlapped->NumberOfBytesSended = 0;
			if(lpfnConnectEx((SOCKET)*this, 
				lpSockAddr, 
				nSockAddrLen, 
				pOverlapped->Buffer.buf,
				pOverlapped->Buffer.len,
				(LPDWORD)&pOverlapped->NumberOfBytesSended, 
				&pOverlapped->Overlapped)) {
				ASSERT(0);
				return 0;
			} else {
				int nErrorCode = GetLastError();
				PRINTF("ConnectEx is failed %d %s", nErrorCode, GetErrorMessage(nErrorCode));
				return SOCKET_ERROR;
			}
		}
		break;
		}
		return SOCKET_ERROR;
	}

	int Listen(int nConnectionBacklog = 5)
	{
		int ret = Base::Listen(nConnectionBacklog);
		RemoveSelect(FD_ACCEPT);
		return ret;
	}

	SOCKET Accept(SOCKADDR* lpSockAddr, int* lpSockAddrLen)
	{
		static LPFN_ACCEPTEX lpfnAcceptEx = [&] {
		DWORD dwBytes = 0;
		do {
			// 获取AcceptEx函数指针
			dwBytes = 0;
			GUID GuidAcceptEx = WSAID_ACCEPTEX;
			if (0 != WSAIoctl((SOCKET)*this, SIO_GET_EXTENSION_FUNCTION_POINTER,
							&GuidAcceptEx, sizeof(GuidAcceptEx),
							&lpfnAcceptEx, sizeof(lpfnAcceptEx), &dwBytes, NULL, NULL)) {
				PRINTF("WSAIoctl AcceptEx is failed. Error=%d", GetLastError());
				break;
			}
		} while(false);
		return lpfnAcceptEx;
		}();
		if(!lpfnAcceptEx) {
			return INVALID_SOCKET;
		}

		//为即将到来的Client连接事先创建好Socket，异步连接需要事先将此Socket备下，再行连接
		SOCKET Sock = XSocket::Socket::Create(AF_INET, SOCK_STREAM, 0);
		if(Sock == INVALID_SOCKET) {
			return INVALID_SOCKET;
		}

		PER_IO_OPERATION_DATA* pOverlapped = &receive_overlapped_;
		memset(&pOverlapped->Overlapped, 0, sizeof(WSAOVERLAPPED));
		pOverlapped->Buffer.buf	= send_reserved_; //这个不能为空, 应使其不小于16，因为SOCKADDR_IN大小影响
		pOverlapped->Buffer.len	= 0; //0表只连不接收、连接到来->请求完成，否则连接到来+任意长数据到来->请求完成
		pOverlapped->OperationType = IOCP_OPERATION_ACCEPT;
		pOverlapped->Sock = Sock;
		pOverlapped->NumberOfBytesReceived = 0;
		//调用AcceptEx函数，地址长度需要在原有的上面加上16个字节
		int nAccept = lpfnAcceptEx((SOCKET)*this, pOverlapped->Sock,
			pOverlapped->Buffer.buf, pOverlapped->Buffer.len, 
			sizeof(SOCKADDR_STORAGE), sizeof(SOCKADDR_STORAGE), 
			&pOverlapped->NumberOfBytesReceived, &(pOverlapped->Overlapped));
		if(nAccept == 0) {
			if(WSAGetLastError() != ERROR_IO_PENDING) {
				DWORD dwError =  GetLastError();
				PRINTF("AcceptEx is failed. Error=%d",dwError);
				XSocket::Socket::Close(Sock);
				SetLastError(dwError);
				return INVALID_SOCKET;
			}
		}
		return nAccept;
	}
	
	int Send(const char* lpBuf, int nBufLen, int nFlags = 0)
	{
		ASSERT(send_overlapped_.NumberOfBytesSended >= send_overlapped_.Buffer.len);
		PER_IO_OPERATION_DATA* pOverlapped = &send_overlapped_;
		memset(&pOverlapped->Overlapped, 0, sizeof(WSAOVERLAPPED));
		pOverlapped->Buffer.buf	= (char*)lpBuf;
		pOverlapped->Buffer.len	= nBufLen; 
		pOverlapped->Flags = nFlags;
		pOverlapped->OperationType = IOCP_OPERATION_SEND;
		pOverlapped->NumberOfBytesSended = 0;
		int nSend = WSASend((SOCKET)*this, 
			&pOverlapped->Buffer, 
			1, 
			(LPDWORD)&pOverlapped->NumberOfBytesSended, 
			(DWORD)pOverlapped->Flags, 
			&pOverlapped->Overlapped,
			NULL);
		if(nSend == 0) {
			return SOCKET_ERROR;
		}
		return nSend;
	}

	int Receive(char* lpBuf, int nBufLen, int nFlags = 0)
	{
		PER_IO_OPERATION_DATA* pOverlapped = &receive_overlapped_;
		memset(&pOverlapped->Overlapped,0,sizeof(WSAOVERLAPPED));
		pOverlapped->Buffer.buf = lpBuf;
		pOverlapped->Buffer.len = nBufLen;
		pOverlapped->Flags = nFlags;
		pOverlapped->OperationType = IOCP_OPERATION_RECEIVE;
		pOverlapped->NumberOfBytesReceived = 0;
		int nRecv = WSARecv((SOCKET)*this, 
			&pOverlapped->Buffer, 
			1, 
			(LPDWORD)&pOverlapped->NumberOfBytesReceived, 
			(LPDWORD)&pOverlapped->Flags, 
			&pOverlapped->Overlapped,
			NULL);
		if(nRecv == 0) {
			return SOCKET_ERROR;
		}
		return nRecv;
	}

	int SendTo(const char* lpBuf, int nBufLen, const SOCKADDR* lpSockAddr, int nSockAddrLen, int nFlags = 0)
	{
		ASSERT(nSockAddrLen==sizeof(SockAddr));
		ASSERT(send_overlapped_.NumberOfBytesSended >= send_overlapped_.Buffer.len);
		PER_IO_OPERATION_DATA* pOverlapped = &send_overlapped_;
		memset(&pOverlapped->Overlapped, 0, sizeof(WSAOVERLAPPED));
		pOverlapped->Buffer.buf	= (char*)lpBuf;
		pOverlapped->Buffer.len	= nBufLen; 
		pOverlapped->Flags = nFlags;
		pOverlapped->OperationType = IOCP_OPERATION_SEND;
		pOverlapped->NumberOfBytesSended = 0;
		send_addr_ = *(const SockAddr*)lpSockAddr;
		int nSend = WSASendTo((SOCKET)*this, 
			&pOverlapped->Buffer, 
			1, 
			(LPDWORD)&pOverlapped->NumberOfBytesSended, 
			(DWORD)pOverlapped->Flags, 
			lpSockAddr,
			nSockAddrLen,
			&pOverlapped->Overlapped,
			NULL);
		if(nSend == 0) {
			return SOCKET_ERROR;
		}
		return nSend;
	}

	int ReceiveFrom(char* lpBuf, int nBufLen, SOCKADDR* lpSockAddr, int* lpSockAddrLen, int nFlags = 0)
	{
		PER_IO_OPERATION_DATA* pOverlapped = &receive_overlapped_;
		memset(&pOverlapped->Overlapped,0,sizeof(WSAOVERLAPPED));
		pOverlapped->Buffer.buf = lpBuf;
		pOverlapped->Buffer.len = nBufLen;
		pOverlapped->Flags = nFlags;
		pOverlapped->OperationType = IOCP_OPERATION_RECEIVE;
		pOverlapped->NumberOfBytesReceived = 0;
		int nRecv = WSARecvFrom((SOCKET)*this, 
			&pOverlapped->Buffer, 
			1, 
			(LPDWORD)&pOverlapped->NumberOfBytesReceived, 
			(LPDWORD)&pOverlapped->Flags, 
			lpSockAddr,
			lpSockAddrLen,
			&pOverlapped->Overlapped,
			NULL);
		if(nRecv == 0) {
			return SOCKET_ERROR;
		}
		return nRecv;
	}

	inline void Select(int lEvent) {  
		int lAsyncEvent = 0;
		if(!(event_ & FD_READ) && (lEvent & FD_READ)) {
			lAsyncEvent |= FD_READ;
		}
		if(!(event_ & FD_WRITE) && (lEvent & FD_WRITE)) {
			lAsyncEvent |= FD_WRITE;
		}
		if(!(event_ & FD_ACCEPT) && (lEvent & FD_ACCEPT)) {
			lAsyncEvent |= FD_ACCEPT;
		}
		if(!Base::IsSelect(FD_IDLE) && (lEvent & FD_IDLE)) {
			lAsyncEvent |= FD_IDLE;
		}
		Base::Select(lEvent);
		if(lAsyncEvent & FD_READ) {
			Trigger(FD_READ, 0);
		}
		if(lAsyncEvent & FD_WRITE) {
			Trigger(FD_WRITE, 0);
		}
		if(lAsyncEvent & FD_ACCEPT) {
			Trigger(FD_ACCEPT, 0);
		}
	}
};

/*!
 *	@brief CompletionPortService 模板定义.
 *
 *	封装CompletionPortService，实现对CompletionPort模型封装
 */
template<class TService = Service>
class CompletionPortServiceT : public TService
{
	typedef TService Base;
protected:
	HANDLE hIocp_;
public:
	CompletionPortServiceT()
	{
		//SYSTEM_INFO SystemInfo = {0};
		//GetSystemInfo(&SystemInfo);
		hIocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1/*SystemInfo.dwNumberOfProcessors*/);
		ASSERT(hIocp_!=INVALID_HANDLE_VALUE);
	}

	~CompletionPortServiceT()
	{
		if(hIocp_!=INVALID_HANDLE_VALUE) {
			CloseHandle(hIocp_);
			hIocp_ = INVALID_HANDLE_VALUE;
		}
	}

	void Stop()
	{
		if(hIocp_!=INVALID_HANDLE_VALUE) {
			PostQueuedCompletionStatus(hIocp_, IOCP_OPERATION_EXIT, 0, NULL);
		}
		Base::Stop();
	}
	
	inline void PostNotify()
	{
		Base::PostNotify();
		PostQueuedCompletionStatus(hIocp_, IOCP_OPERATION_NOTIFY, (ULONG_PTR)0, NULL);
	}

	inline void PostNotify(void* data)
	{
		PostQueuedCompletionStatus(hIocp_, IOCP_OPERATION_NOTIFYDATA, (ULONG_PTR)data, NULL);
	}

protected:
	//
	virtual void OnNotifyData(void* data)
	{

	}

	virtual bool OnCompletion(BOOL bStatus, ULONG_PTR Key, DWORD dwTransfer, WSAOVERLAPPED* lpOverlapped)
	{
		return false;
	}
	//
	virtual void OnWait()
	{
		do {
		ULONG_PTR Key = 0;
		DWORD dwTransfer = 0;
		WSAOVERLAPPED *lpOverlapped = NULL;
		BOOL bStatus = GetQueuedCompletionStatus(
			hIocp_,
			&dwTransfer,
			(PULONG_PTR)&Key,
			(LPOVERLAPPED *)&lpOverlapped,
			Base::GetWaitingTimeOut()/*INFINITE*/);
		if(!bStatus) {
			DWORD dwErr = GetLastError();
			if (WAIT_TIMEOUT == dwErr) {
				break;
			} else {
				PRINTF("GetQueuedCompletionStatus Error:%d ", dwErr);
			}
		} 
		if (dwTransfer == IOCP_OPERATION_EXIT) { //
			PRINTF("GetQueuedCompletionStatus Eixt");
			return;
		}
		else if (dwTransfer == IOCP_OPERATION_NOTIFY) { //
			OnNotify();
		}
		else if (dwTransfer == IOCP_OPERATION_NOTIFYDATA) { //
			OnNotifyData((void*)Key);
		}
		else if(!OnCompletion(bStatus, Key, dwTransfer, lpOverlapped)) {
			break;
		}
		} while(true);
	}
};

typedef ThreadServiceT<CompletionPortServiceT<Service>> CompletionPortService;

/*!
 *	@brief CompletionPortSocketSet 模板定义.
 *
 *	封装CompletionPortSocketSet，实现对CompletionPort模型封装，最多管理uFD_SETSize数Socket
 */
template<class TService = CompletionPortService, class TSocket = SocketEx>
class CompletionPortSocketSetT : public SocketSetT<TService,TSocket>
{
	typedef SocketSetT<TService,TSocket> Base;
public:
	typedef TService Service;
	typedef TSocket Socket;
public:
	CompletionPortSocketSetT(int nMaxSocketCount):Base(nMaxSocketCount)
	{
		
	}

	~CompletionPortSocketSetT()
	{
		
	}

	int AddSocket(std::shared_ptr<Socket> sock_ptr, int evt = 0)
	{
		std::unique_lock<std::mutex> lock(mutex_);
		int i, j = sock_ptrs_.size();
		for (i = 0; i < j; i++)
		{
			if(sock_ptrs_[i]==NULL) {
				if (sock_ptr) {
					sock_count_++;
					sock_ptrs_[i] = sock_ptr;
					sock_ptr->AttachService(this);
					//SetHandleInformation((HANDLE) (SOCKET)*sock_ptr, HANDLE_FLAG_INHERIT, 0);
					HANDLE hIocp = CreateIoCompletionPort((HANDLE)(SOCKET)*sock_ptr, hIocp_, (ULONG_PTR)(i + 1), 0);
					ASSERT(hIocp);
					if (hIocp != INVALID_HANDLE_VALUE) {
#ifdef _DEBUG
						PRINTF("CreateIoCompletionPort: %ld by %ld", hIocp, hIocp_);
#endif
					} else {
						PRINTF("CreateIoCompletionPort Error:%d", ::GetLastError());
					}
					// if (family == AF_INET6) {
					// 	//uv_tcp_non_ifs_lsp_ipv6 = 1表示IPPROTO_IP协议使用真正地操作系统句柄，没有lsp封装
					// 	non_ifs_lsp = uv_tcp_non_ifs_lsp_ipv6;
					// } else {
					// 	//同理
					// 	non_ifs_lsp = uv_tcp_non_ifs_lsp_ipv4;
					// }

					// if (pSetFileCompletionNotificationModes &&
					// 	!(handle->flags & UV_HANDLE_EMULATE_IOCP) && !non_ifs_lsp) {
					// 	if (pSetFileCompletionNotificationModes((HANDLE) socket,
					// 		FILE_SKIP_SET_EVENT_ON_HANDLE |
					// 		FILE_SKIP_COMPLETION_PORT_ON_SUCCESS))//如果操作立刻完成，不再向iocp发送通知 
					// 	{
					// 	handle->flags |= UV_HANDLE_SYNC_BYPASS_IOCP;
					// 	} else if (GetLastError() != ERROR_INVALID_FUNCTION) {
					// 	return GetLastError();
					// 	}
					// }
					if(evt & FD_READ) {
						PostQueuedCompletionStatus(hIocp_, IOCP_OPERATION_TRYRECEIVE, (ULONG_PTR)(i + 1), NULL);
					}
					if(evt & FD_WRITE) {
						PostQueuedCompletionStatus(hIocp_, IOCP_OPERATION_TRYSEND, (ULONG_PTR)(i + 1), NULL);
					}
					if(evt & FD_ACCEPT) {
						PostQueuedCompletionStatus(hIocp_, IOCP_OPERATION_TRYACCEPT, (ULONG_PTR)(i + 1), NULL);
					}
					return i;
				} else {
					//测试可不可以增加Socket，返回true表示可以增加
					return i;
				}
				break;
			}
		}
		return -1;
	}
	template<class Ty = Socket>
	inline int AddConnect(std::shared_ptr<Ty> sock_ptr, u_short port)
	{
		int ret = AddSocket(std::static_pointer_cast<Socket>(sock_ptr));
		if(ret >= 0 && sock_ptr) {
			sock_ptr->Connect(port);
		}
		return ret;
	}
	inline int AddAccept(std::shared_ptr<Socket> sock_ptr)
	{
		return AddSocket(sock_ptr,FD_ACCEPT);
	}

protected:
	//
	virtual bool OnCompletion(BOOL bStatus, ULONG_PTR Key, DWORD dwTransfer, WSAOVERLAPPED* lpOverlap)
	{
		PER_IO_OPERATION_DATA* lpOverlapped = (PER_IO_OPERATION_DATA*)lpOverlap;
		int uFD_SETSize = sock_ptrs_.size();
		int Pos = Key;
		if (Pos > 0 && Pos <= uFD_SETSize) {
			std::unique_lock<std::mutex> lock(mutex_);
			std::shared_ptr<Socket> sock_ptr = sock_ptrs_[Pos - 1];
			lock.unlock();
			if(sock_ptr) {
				bool bContinue = false;
				switch (dwTransfer)
				{
				case IOCP_OPERATION_TRYRECEIVE:
				{
					bContinue = true;
					if (!sock_ptr->IsSelect(FD_READ)) {
						sock_ptr->Select(FD_READ);
					}
				}
				break;
				case IOCP_OPERATION_TRYSEND:
				{
					bContinue = true;
					if (!sock_ptr->IsSelect(FD_WRITE)) {
						sock_ptr->Select(FD_WRITE);
					}
				}
				break;
				case IOCP_OPERATION_TRYACCEPT:
				{
					bContinue = true;
					if (!sock_ptr->IsSelect(FD_ACCEPT)) {
						sock_ptr->Select(FD_ACCEPT);
					}
				}
				break;
				default:
				break;
				}
				if(bContinue) {
					return true;
				}
				if (!bStatus || !lpOverlapped) {
					PRINTF("GetQueuedCompletionStatus bStatus=%d lpOverlapped=%p", bStatus, lpOverlapped);
					if (sock_ptr->IsListenSocket()) {
						int nErrorCode = sock_ptr->GetLastError();
						sock_ptr->Trigger(FD_ACCEPT, nErrorCode);
					} else if (sock_ptr->IsSelect(FD_CONNECT)) {
						sock_ptr->RemoveSelect(FD_CONNECT);
						int nErrorCode = sock_ptr->GetLastError();
						sock_ptr->Trigger(FD_CONNECT, nErrorCode);
					} else {
						int nErrorCode = sock_ptr->GetLastError();
						sock_ptr->Trigger(FD_CLOSE, nErrorCode);
					}
					if(lpOverlapped && lpOverlapped->OperationType == IOCP_OPERATION_ACCEPT) {
						if(lpOverlapped->Sock != INVALID_SOCKET) {
							XSocket::Socket::Close(lpOverlapped->Sock);
							lpOverlapped->Sock = INVALID_SOCKET;
						}
					}
				} else {
					switch (lpOverlapped->OperationType)
					{	
					case IOCP_OPERATION_ACCEPT:
					{
						if(sock_ptr->IsSelect(FD_ACCEPT)) {
							// LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = nullptr;
							// DWORD dwBytes = 0;
							// do {
							// 	// 获取GetAcceptExSockAddrs函数指针
							// 	dwBytes = 0;
							// 	GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
							// 	if (0 != WSAIoctl((SOCKET)*this, SIO_GET_EXTENSION_FUNCTION_POINTER,
							// 					&GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
							// 					&lpfnGetAcceptExSockaddrs, sizeof(lpfnGetAcceptExSockaddrs), &dwBytes, NULL, NULL)) {
							// 		PRINTF("WSAIoctl GetAcceptExSockaddrs is failed. Error=%d", GetLastError());
							// 		break;
							// 	}
							// } while(false);
							// sock_ptr->SetSockOpt(
							// 	SOL_SOCKET
							// 	, SO_UPDATE_ACCEPT_CONTEXT
							// 	, (char*)&(lpOverlapped->Sock)
							// 	, sizeof(lpOverlapped->Sock));
							// SOCKADDR_IN *lpRemoteAddr = NULL, *lpLocalAddr = NULL;
							// int nRemoteAddrLen = sizeof(SOCKADDR_IN), nLocalAddrLen = sizeof(SOCKADDR_IN);
							// lpfnGetAcceptExSockaddrs(lpOverlapped->Buffer.buf, 0, 
							// 	sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, 
							// 	(LPSOCKADDR*)&lpLocalAddr, &nLocalAddrLen,
							// 	(LPSOCKADDR*)&lpRemoteAddr, &nRemoteAddrLen);
							//sprintf(pClientComKey->sIP, "%d", addrClient->sin_port );	//cliAdd.sin_port );
							sock_ptr->Trigger(FD_ACCEPT, lpOverlapped->Sock, nullptr, 0);
						}
						if (sock_ptr->IsSocket()) {
							if (sock_ptr->IsSelect(FD_ACCEPT)) {
								sock_ptr->Trigger(FD_ACCEPT, 0);
							}
						}
					}
					break;
					case IOCP_OPERATION_CONNECT:
					{
						sock_ptr->SetSockOpt(
							SOL_SOCKET,
							SO_UPDATE_CONNECT_CONTEXT,
							NULL,
							0);
						if(sock_ptr->IsSelect(FD_CONNECT)) {
							sock_ptr->RemoveSelect(FD_CONNECT);
							sock_ptr->Trigger(FD_CONNECT, sock_ptr->GetLastError());
						}
					}
					break;
					case IOCP_OPERATION_RECEIVE:
					{
						lpOverlapped->NumberOfBytesReceived = dwTransfer;
						if (dwTransfer) {
							if (sock_ptr->IsSelect(FD_READ)) {
								sock_ptr->Trigger(FD_READ, lpOverlapped->Buffer.buf, lpOverlapped->NumberOfBytesReceived, 0);
							}
							if (sock_ptr->IsSocket()) {
								if (sock_ptr->IsSelect(FD_READ)) {
									sock_ptr->Trigger(FD_READ, 0);
								}
							}
						} else {
							DWORD dwError = WSAGetLastError();
							PRINTF("GetQueuedCompletionStatus Recv WSAError:%d", dwError);
							sock_ptr->Trigger(FD_CLOSE, dwError);
						}
					}
					break;
					case IOCP_OPERATION_SEND:
					{
						lpOverlapped->NumberOfBytesSended = dwTransfer;
						if (dwTransfer) {
							if (sock_ptr->IsSelect(FD_WRITE)) {
								sock_ptr->Trigger(FD_WRITE, lpOverlapped->Buffer.buf, lpOverlapped->NumberOfBytesSended, 0);
							}
							if (sock_ptr->IsSocket()) {
								if (sock_ptr->IsSelect(FD_WRITE)) {
									sock_ptr->Trigger(FD_WRITE, 0);
								}
							}
						} else {
							DWORD dwError = WSAGetLastError();
							PRINTF("GetQueuedCompletionStatus Send WSAError:%d", dwError);
							sock_ptr->Trigger(FD_CLOSE, dwError);
						}
					}
					break;
					default:
					{
						ASSERT(0);
					}
					break;
					}
				}
			}
			return true;
		}
		return false;
	}
};

}

#endif//_H_XIOCPSOCKETEX_H_