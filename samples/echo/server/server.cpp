// server.cpp : 定义控制台应用程序的入口点。
//

#include "../../samples.h"
#include "../../../XSocket/XSocketImpl.h"
#if USE_EPOLL
#include "../../../XSocket/XEPoll.h"
#elif USE_IOCP
#include "../../../XSocket/XCompletionPort.h"
#endif//
#if USE_OPENSSL
#include "../../../XSocket/XSSLImpl.h"
#endif
#include "../../../XSocket/XSimpleImpl.h"
using namespace XSocket;

#if USE_UDP

class server : public SocketExImpl<server,SelectUdpServerT<ThreadService,SimpleUdpSocketT<WorkSocketT<SelectSocketT<ThreadService,SocketEx>>,SockAddrType>>>
{
	typedef SocketExImpl<server,SelectUdpServerT<ThreadService,SimpleUdpSocketT<WorkSocketT<SelectSocketT<ThreadService,SocketEx>>,SockAddrType>>> Base;
protected:
	std::string addr_;
	u_short port_;
public:

	bool Start(const char* address, u_short port)
	{
		addr_ = address;
		port_ = port;
		Base::Start();
		return true;
	}

protected:
	//
	virtual bool OnInit()
	{
		bool ret = Base::OnInit();
		if(!ret) {
			return false;
		}
		if(port_ <= 0) {
			return false;
		}
		Open(AF_INETType,SOCK_DGRAM);
		SetSockOpt(SOL_SOCKET, SO_REUSEADDR, 1);
		SockAddrType stAddr = {0};
	#if USE_IPV6
		stAddr.sin6_family = AF_INET6;
		IpStr2IpAddr(addr_.c_str(),AF_INET6,&stAddr.sin6_addr);
		stAddr.sin6_port = htons((u_short)port_);
	#else
		stAddr.sin_family = AF_INET;
		stAddr.sin_addr.s_addr = Ip2N(Url2Ip(addr_.c_str()));
		stAddr.sin_port = htons((u_short)port_);
	#endif//
		Bind((const SOCKADDR*)&stAddr, sizeof(stAddr));
		Select(FD_READ);
		SetNonBlock();//设为非阻塞模式
		return true;
	}

	virtual void OnTerm()
	{
		//服务结束运行，释放资源
		if(Base::IsSocket()) {
#ifndef WIN32
			Base::ShutDown();
#endif
			Base::Trigger(FD_CLOSE, 0);
		}
	}

	virtual void OnRecvBuf(const char* lpBuf, int nBufLen, const SockAddrType & SockAddr)
	{
		char str[64] = {0};
		XSocket::Socket::SockAddr2Str((const SOCKADDR*)&SockAddr, sizeof(SockAddr), str, 64);
		PRINTF("recv[%s]:%.*s", str, nBufLen, lpBuf);
		PRINTF("echo[%s]:%.*s", str, nBufLen, lpBuf);
		SendBuf(lpBuf,nBufLen,SockAddr,SOCKET_PACKET_FLAG_TEMPBUF);
		Base::OnRecvBuf(lpBuf, nBufLen, SockAddr);
	}
};

#else

class worker;

class WorkEvent : public DealyEventBase
{
public:
	worker* dst = nullptr;
	int id;
	std::string buf;
#if USE_UDP
	SockAddrType addr;
#endif
	int flags;

	WorkEvent() {}
#if USE_UDP
	WorkEvent(worker* d, int id, const char* buf, int len, const SockAddrType& addr, int flag):dst(d),id(id),buf(buf,len),addr(addr),flags(flag){}
#else
	WorkEvent(worker* d, int id, const char* buf, int len, int flag):dst(d),id(id),buf(buf,len),flags(flag){}
#endif

	// inline int get_id() { return evt; }
	// inline const char* get_data() { return data.c_str(); }
	// inline int get_datalen() { return data.size(); }
	// inline int get_flags() { return flags; }
};
class WorkEventService : public
#if USE_EPOLL
EventServiceT<WorkEvent,EPollService>
#elif USE_IOCP
EventServiceT<WorkEvent,CompletionPortService>
#else
EventServiceT<WorkEvent,SelectService>
#endif//
{
public:

	inline worker* IsSocketEvent(const Event& evt)
	{
		return evt.dst;
	}
	inline bool IsActive(Event& evt) { return evt.IsActive(); }
	inline bool IsRepeat(Event& evt) { return evt.IsRepeat(); }
	inline void UpdateRepeat(Event& evt) { evt.Update(); }
};
typedef SimpleSocketEvtServiceT<WorkEventService> WorkService;


class WorkSocket;
//class WorkSocketSet;

#if USE_EPOLL
typedef EPollSocketSetT<WorkService,WorkSocket> WorkSocketSet;
#elif USE_IOCP
typedef CompletionPortSocketSetT<WorkService,WorkSocket> WorkSocketSet;
#else
typedef SelectSocketSetT<WorkService,WorkSocket> WorkSocketSet;
#endif//

class WorkSocket : public
#if USE_EPOLL
EPollSocketT<WorkSocketSet,SocketEx>
#elif USE_IOCP
CompletionPortSocketT<WorkSocketSet,SocketEx>
#else
SelectSocketT<WorkSocketSet,SocketEx>
#endif//
{

};

class worker : public 
#if USE_OPENSSL
SocketExImpl<worker,SimpleEvtSocketT<SSLWorkSocketT<SimpleSocketT<WorkSocketT<SSLSocketT<WorkSocket>>>>>>
#else 
SocketExImpl<worker,SimpleEvtSocketT<SimpleSocketT<WorkSocketT<WorkSocket>>>>
#endif
{
	typedef 
#if USE_OPENSSL
SocketExImpl<worker,SimpleEvtSocketT<SSLWorkSocketT<SimpleSocketT<WorkSocketT<SSLSocketT<WorkSocket>>>>>>
#else
SocketExImpl<worker,SimpleEvtSocketT<SimpleSocketT<WorkSocketT<WorkSocket>>>> 
#endif
Base;
protected:
	
public:
	worker()
	{
		ReserveRecvBufSize(DEFAULT_BUFSIZE);
		ReserveSendBufSize(DEFAULT_BUFSIZE);
	}

	~worker() 
	{
		
	}

public:
	inline void PostBuf(const char* lpBuf, int nBufLen, int nFlags = 0)
	{
		Post(Event(this,FD_WRITE,lpBuf,nBufLen,nFlags));
	}

	virtual void OnEvent(const Event& evt)
	{
		if(evt.id == FD_WRITE) {
			if(!IsSocket()) {
				return;
			}
			PRINTF("echo:%.*s", evt.buf.size(), evt.buf.c_str());
			SendBuf(evt.buf.c_str(),evt.buf.size());
		}
	}
protected:
	//
	virtual void OnIdle()
	{
		/*char lpBuf[DEFAULT_BUFSIZE+1];
		int nBufLen = 0;
		int nFlags = 0;
		nBufLen = Receive(lpBuf,DEFAULT_BUFSIZE,&nFlags);
		if (nBufLen<=0) {
			return;
		}
		lpBuf[nBufLen] = 0;
		PRINTF("%s", lpBuf);
		PRINTF("echo:%s", lpBuf);
		Send(lpBuf,nBufLen);*/
	}

	virtual void OnRecvBuf(const char* lpBuf, int nBufLen, int nFlags)
	{
		PRINTF("recv:%.*s", nBufLen, lpBuf);
		PostBuf(lpBuf,nBufLen,0);
		Base::OnRecvBuf(lpBuf,nBufLen,nFlags);
	}

};

class server 
#if 0
	: public SelectServerT<ThreadService,SocketExImpl<server,ListenSocketT<SocketEx>>,WorkSocketSet>
#else
	: public SocketManagerT<WorkSocketSet>
#endif//
{
#if 0
	typedef SelectServerT<ThreadService,SocketExImpl<server,ListenSocketT<SocketEx>>,WorkSocketSet> Base;
#else
	typedef SocketManagerT<WorkSocketSet> Base;
protected:
	class listener : public SocketExImpl<listener,SimpleEvtSocketT<ListenSocketExT<WorkSocket>>>
	, public std::enable_shared_from_this<listener>
	{
	protected:
		server* srv_;
	public:
		listener(server* srv):srv_(srv)
		{
		}

	protected:
		//
		virtual void OnAccept(SOCKET Sock, const SOCKADDR* lpSockAddr, int nSockAddrLen) 
		{
				//测试下还能不能再接收SOCKET
				if(srv_->AddSocket(NULL) < 0) {
					PRINTF("The connection was refused by the computer running select server because the maximum number of sessions has been exceeded.");
					XSocket::Socket::Close(Sock);
					return;
				}
				std::shared_ptr<worker> sock_ptr = std::make_shared<worker>();
				sock_ptr->Attach(Sock,SOCKET_ROLE_WORK);
				sock_ptr->SetNonBlock();//设为非阻塞模式
				int pos = srv_->AddSocket(sock_ptr, FD_READ|FD_OOB);
				if(pos >= 0) {
					//
				} else {
					PRINTF("The connection was refused by the computer running select server because the maximum number of sessions has been exceeded.");
					sock_ptr->Trigger(FD_CLOSE, 0);
				}
		}
	};
#endif
public:
#if 0
	server(int nMaxSocketCount = DEFAULT_MAX_SOCKET_COUNT):Base(nMaxSocketCount,DEFAULT_MAX_SOCKSET_COUNT)
#else
	server(int nMaxSocketCount = DEFAULT_MAX_SOCKET_COUNT):Base(nMaxSocketCount,DEFAULT_MAX_SOCKSET_COUNT)
#endif
	{
		SetWaitTimeOut(DEFAULT_WAIT_TIMEOUT);
	}

#if 1
	bool Start(const char* address, u_short port)
	{
		bool ret = Base::Start();
		if(!ret) {
			return false;
		}

		std::shared_ptr<listener> sock_ptr = std::make_shared<listener>(this);
		sock_ptr->Open(address);
		sock_ptr->SetSockOpt(SOL_SOCKET, SO_REUSEADDR, 1);
		sock_ptr->Bind(port);
		sock_ptr->Listen(1024);
		AddAccept(sock_ptr);

		return true;
	}
#else
protected:
	//
	virtual bool OnInit()
	{
		if(port_ <= 0) {
			return false;
		}
		Base::Open();
		Base::SetSockOpt(SOL_SOCKET, SO_REUSEADDR, 1);
		Base::Bind(address_.c_str(), port_);
		Base::Listen();
		return true;
	}

	virtual void OnTerm()
	{
		//服务结束运行，释放资源
		if(Base::IsSocket()) {
#ifndef WIN32
			Base::ShutDown();
#endif
			Base::Trigger(FD_CLOSE, 0);
		}
	}
#endif//
};

#endif//USE_UDP

#ifdef WIN32
int _tmain(int argc, _TCHAR* argv[])
#else
int main()
#endif//
{
#if USE_UDP
	Socket::Init();
#else
	worker::Init();
#if USE_OPENSSL
	TLSContextConfig tls_ctx_config = {0};
	tls_ctx_config.cert_file = "./ssl/dev.crt";
    tls_ctx_config.key_file = "./ssl/dev_nopass.key";
    tls_ctx_config.dh_params_file;
    tls_ctx_config.ca_cert_file = "./ssl/dev.crt";
    tls_ctx_config.ca_cert_dir = "./ssl";
    tls_ctx_config.protocols = "TLSv1.1 TLSv1.2";
    tls_ctx_config.ciphers;
    tls_ctx_config.ciphersuites;
    tls_ctx_config.prefer_server_ciphers;
	worker::Configure(&tls_ctx_config);
#endif
#endif
	server *s = new server();
	s->Start(DEFAULT_IP, DEFAULT_PORT);
	getchar();
	s->Stop();
	delete s;

#if USE_UDP
	Socket::Term();
#else
	worker::Term();
#endif
	return 0;
}

