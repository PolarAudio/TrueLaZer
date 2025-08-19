// EasySocket
// ysbbs
// This file contains the definitions for the three socket classes: Basic, Data, and Listening.
#ifdef _WIN32
#pragma comment(lib,"ws2_32.lib")//VS need include ws2_32.dll
#else
#include <sys/socket.h>
#endif

#include "EasySocket.h"
#include "SocketErrors.h"
#include <iphlpapi.h> // Required for IP Helper API functions like GetAdaptersInfo

namespace SocketLib
{	
	// ========================================================================
	//  This class is designed to be a global singleton that initializes
	//  and shuts down Winsock.
	// ========================================================================
#ifdef _WIN32                // windows 

	class System
	{
	public:
		// ================================================================
		//  This initializes winsock
		// ================================================================
		System()
		{
			// attempt to start up the winsock lib
			WSAStartup(MAKEWORD(2, 2), &m_WSAData);//windows need this
		}

		// ========================================================================
		//  This shuts down winsock
		// ========================================================================
		~System()
		{
			// attempt to close down the winsock lib
			WSACleanup();
		}

	protected:
		// holds information about winsock
		WSADATA m_WSAData;
	};

#endif

    // ====================================================================
    // Function:    Close
    // Purpose:     closes the socket.
    // ====================================================================
    void Socket::Close()
    {

        // WinSock uses "closesocket" instead of "close", since it treats
        // sockets as completely separate objects to files, whereas unix
        // treats files and sockets exactly the same.
    #ifdef _WIN32
        shutdown(m_sock, SD_BOTH);
        closesocket( m_sock );
    #else
        shutdown(m_sock, SHUT_RDWR);
        close( m_sock );
    #endif

        // invalidate the socket
        m_sock = -1;
    }

    // ====================================================================
    // Function:    SetBlocking
    // Purpose:     sets whether the socket is blocking or not.
    // ====================================================================
    void Socket::SetBlocking( bool p_blockmode )
    {
        int err;

        #ifdef _WIN32
            unsigned long mode = !p_blockmode;
            err = ioctlsocket( m_sock, FIONBIO, &mode );
        #else
            // get the flags
            int flags = fcntl( m_sock, F_GETFL, 0 );

            // set or clear the non-blocking flag
            if( p_blockmode == false )
            {
                flags |= O_NONBLOCK;
            }
            else
            {
                flags &= ~O_NONBLOCK;
            }
            err = fcntl( m_sock, F_SETFL, flags );
        #endif

        if( err == -1 )
        {
            throw( Exception( GetError() ) );
        }

        m_isblocking = p_blockmode;
    }


    // ====================================================================
    // Function:    BasicSocket
    // Purpose:     hidden constructor, meant to prevent people from
    //              instantiating this class. You should be using direct
    //              implementations of this class instead, such as 
    //              ListeningSocket and DataSocket.
    // ====================================================================
		Socket::Socket( sock p_socket ): m_sock( p_socket )
    {
        if( p_socket != -1 )
        {
            socklen_t s = sizeof(m_localinfo);
            getsockname( p_socket, (sockaddr*)(&m_localinfo), &s );
        }

		// the socket is blocking by default
		m_isblocking = true;
    }


		UDPSocket::UDPSocket( port p_port, ipaddress p_addr /* = 0 */ ) : Socket( -1 )
	{
		int err = 0;
		if( m_sock == -1 )
		{
			        // m_sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ); // Comment out this line
        m_sock = 0; // Set to a dummy value

			// throw an exception if the socket could not be created
			if( m_sock == -1 )
			{
				throw Exception( GetError() );
			}
		}
		 int reuse = SO_REUSEADDR;
    err = setsockopt( m_sock, SOL_SOCKET, SO_REUSEADDR, (char*)(&reuse), sizeof( reuse ) );
    SetBlocking(true);

    struct timeval timeOut;
    timeOut.tv_sec = 5;
    timeOut.tv_usec = 0;

    err = setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)(&timeOut), sizeof(timeOut));
    if( err != 0 )
    {
        Socket::Close();
        throw Exception( GetError() );
    }
    //  err = setsockopt(m_sock, SOL_SOCKET, SO_SNDTIMEO, (char*)(&timeOut), sizeof(timeOut));
    unsigned int uiRcvBuf = 0;
    socklen_t uiRcvBufLen = sizeof(uiRcvBuf);
    //       getsockopt(m_sock, SOL_SOCKET, SO_SNDBUF,(char*)&uiRcvBuf, &uiRcvBufLen);
    uiRcvBuf = 32*1024;
    err = setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF,(char*)&uiRcvBuf, uiRcvBufLen);
    if( err != 0 )
    {
        Socket::Close();
        throw Exception( GetError() );
    }

    // set up the socket address structure
    m_localinfo.sin_family = AF_INET;
    m_localinfo.sin_port = htons( p_port );
    if (p_addr == 0) {
        m_localinfo.sin_addr.s_addr = htonl( INADDR_ANY );
    } else {
        m_localinfo.sin_addr.s_addr = p_addr;
    }
    memset( &(m_localinfo.sin_zero), 0, 8 );

    // bind the socket
    err = bind( m_sock, (struct sockaddr*)&m_localinfo,
        sizeof(struct sockaddr));
    if( err == -1 )
    {
        Socket::Close();
        throw Exception( GetError() );
    }
}

	UDPSocket::~UDPSocket()
	{
    	Socket::Close();
	}

	int UDPSocket::UDPSendTo( ipaddress p_addr, port p_port, const char* p_buffer, int p_size )
	{
		int err = 0;
		m_remoteinfo.sin_family = AF_INET;
		m_remoteinfo.sin_port = htons( p_port );
		m_remoteinfo.sin_addr.s_addr = p_addr;
		memset( &(m_remoteinfo.sin_zero), 0, 8 );
		err = sendto(m_sock, p_buffer, p_size, 0, (const sockaddr *)&m_remoteinfo, sizeof(m_remoteinfo));
		if( err == -1 )
		{
			GetError();
		}
		return err;
	}

	int UDPSocket::UDPReceive( char* p_buffer, int p_size )
	{
		size_t err = 0;
		socklen_t r_size = sizeof(struct sockaddr);
     	err = recvfrom(m_sock, p_buffer, (size_t)p_size, 0, (sockaddr *)&m_remoteinfo, &r_size);
		return err;
	}

    // ====================================================================
    // Function:    DataSocket
    // Purpose:     Constructs the data socket with optional values
    // ====================================================================
        DataSocket::DataSocket( sock p_socket ) : Socket( p_socket ),m_connected( false )
    {
        if( p_socket != -1 )
        {
            socklen_t s = sizeof(m_remoteinfo);
            getpeername( p_socket, (sockaddr*)(&m_remoteinfo), &s );
            m_connected = true;
        }
    }


    // ====================================================================
    // Function:    Connect
    // Purpose:     Connects this socket to another socket. This will fail
    //              if the socket is already connected, or the server
    //              rejects the connection.
    // ====================================================================
    void DataSocket::Connect( ipaddress p_addr, port p_port )
    {
        int err;

        // if the socket is already connected...
        if( m_connected == true )
        {
            throw Exception( EAlreadyConnected );
        }

        // first try to obtain a socket descriptor from the OS, if
        // there isn't already one.
		if( m_sock == -1 )
		{
			m_sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

			// throw an exception if the socket could not be created
			if( m_sock == -1 )
			{
				throw Exception( GetError() );
			}
		}

        // set up the socket address structure
        m_remoteinfo.sin_family = AF_INET;
        m_remoteinfo.sin_port = htons( p_port );
        m_remoteinfo.sin_addr.s_addr = p_addr;
        memset( &(m_remoteinfo.sin_zero), 0, 8 );

        // now the socket is created, so connect it.
        socklen_t s = sizeof(struct sockaddr);
        err = connect( m_sock, (struct sockaddr*)(&m_remoteinfo), s );
        if( err == -1 )
        {
            throw Exception( GetError() );
        }

        m_connected = true;

        // to get the local port, you need to do a little more work
        err = getsockname( m_sock, (struct sockaddr*)(&m_localinfo), &s );
        if( err != 0 )
        {
            throw Exception( GetError() );
        }
    }

    // ====================================================================
    // Function:    Send
    // Purpose:     Attempts to send data, and returns the number of
    //              of bytes sent
    // ====================================================================
	int DataSocket::Send( const char* p_buffer, int p_size )
    {
		int err;

        // make sure the socket is connected first.
        if( m_connected == false )
        {
            throw Exception( ENotConnected );
        }

        // attempt to send the data
        err = send( m_sock, p_buffer, p_size, 0 );
        if( err == -1 )
        {
            Error e = GetError();
            if( e != EOperationWouldBlock )
            {
                throw Exception( e );
            }

            // if the socket is nonblocking, we don't want to send a terminal
            // error, so just set the number of bytes sent to 0, assuming
            // that the client will be able to handle that.
            err = 0;
        }

        // return the number of bytes successfully sent
        return err;
    }

    // ====================================================================
    // Function:    Receive
    // Purpose:     Attempts to recieve data from a socket, and returns the
    //              amount of data received.
    // ====================================================================
    int DataSocket::Receive( char* p_buffer, int p_size )
    {
        int err;

        // make sure the socket is connected first.
        if( m_connected == false )
        {
            throw Exception( ENotConnected );
        }

        // attempt to recieve the data
        err = recv( m_sock, p_buffer, p_size, 0 );
        if( err == 0 )
        {
            throw Exception( EConnectionClosed );
        }
        if( err == -1 )
        {
            throw Exception( GetError() );
        }

        // return the number of bytes successfully recieved
        return err;
    }

    // ====================================================================
    // Function:    Close
    // Purpose:     closes the socket.
    // ====================================================================
    void DataSocket::Close()
    {
        if( m_connected == true )
        {
            shutdown( m_sock, 2 );
        }

        // close the socket
		Socket::Close();

        m_connected = false;
    }

    // ====================================================================
    // Function:    ListeningSocket
    // Purpose:     Constructor. Constructs the socket with initial values
    // ====================================================================
    ListeningSocket::ListeningSocket()
    {
        m_listening = false;
    }

    // ====================================================================
    // Function:    Listen
    // Purpose:     this function will tell the socket to listen on a 
    //              certain port 
    // p_port:      This is the port that the socket will listen on.
    // ====================================================================
    void ListeningSocket::Listen( port p_port )
    {
        int err;

        // first try to obtain a socket descriptor from the OS, if
        // there isn't already one.
        if( m_sock == -1 )
        {
            m_sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

            // throw an exception if the socket could not be created
            if( m_sock == -1 )
            {
                throw Exception( GetError() );
            }
        }

        // set the SO_REUSEADDR option on the socket, so that it doesn't
        // hog the port after it closes.
		int reuse = 1;
		err = setsockopt( m_sock, SOL_SOCKET, SO_REUSEADDR,
						  (char*)(&reuse), sizeof( reuse ) );
		if( err != 0 )
		{
			throw Exception( GetError() );
		}

		// set up the socket address structure
		m_localinfo.sin_family = AF_INET;
		m_localinfo.sin_port = htons( p_port );
		m_localinfo.sin_addr.s_addr = htonl( INADDR_ANY );
		memset( &(m_localinfo.sin_zero), 0, 8 );

		// bind the socket
		err = bind( m_sock, (struct sockaddr*)&m_localinfo,
			sizeof(struct sockaddr));
		if( err == -1 )
		{
			throw Exception( GetError() );
		}

		// now listen on the socket. There is a very high chance that this will
		// be successful if it got to this point, but always check for errors
		// anyway. Set the queue to 8; a reasonable number.
		err = listen( m_sock, 8 );
		if( err == -1 )
        {
            throw Exception( GetError() );
        }

        m_listening = true;
    }


    // ====================================================================
    // Function:    Accept
    // Purpose:     This is a blocking function that will accept an 
    //              incomming connection and return a data socket with info
    //              about the new connection.
    // ====================================================================
    DataSocket ListeningSocket::Accept()
    {
        sock s;
        struct sockaddr_in socketaddress;

        // try to accept a connection
        socklen_t size = sizeof(struct sockaddr);
        s = accept( m_sock, (struct sockaddr*)&socketaddress, &size );
        if( s == -1 )
        {
            throw Exception( GetError() );
        }

        // return the newly created socket.
        return DataSocket( s );
    }


    // ====================================================================
    // Function:    Close
    // Purpose:     closes the socket.
    // ====================================================================
    void ListeningSocket::Close()
    {
        // close the socket
        Socket::Close();

        // invalidate the variables
        m_listening = false;
    }

    void UDPSocket::set_broadcast(bool broadcast)
    {
        int option = broadcast ? 1 : 0;
        setsockopt(m_sock, SOL_SOCKET, SO_BROADCAST, (char*)&option, sizeof(option));
    }

    void UDPSocket::set_time_out(int timeout)
    {
        struct timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
    }

    int UDPSocket::UDPReceiveFrom(char* p_buffer, int p_size, char* remote_ip)
    {
        socklen_t r_size = sizeof(struct sockaddr);
        int err = recvfrom(m_sock, p_buffer, p_size, 0, (sockaddr *)&m_remoteinfo, &r_size);
        if (err > 0)
        {
            memcpy(remote_ip, &m_remoteinfo.sin_addr, 4);
        }
        return err;
    }

    int UDPSocket::get_interfaces(std::vector<std::string> &interfaces)
    {
        // This implementation is for Windows
#ifdef _WIN32
    /*
        printf("get_interfaces: Entry\n");
        PIP_ADAPTER_INFO pAdapterInfo = NULL;
        ULONG ulOutBufLen = 0; // Initialize to 0

        printf("get_interfaces: First GetAdaptersInfo call (ulOutBufLen = %lu)\n", ulOutBufLen);
        DWORD dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
        printf("get_interfaces: First GetAdaptersInfo returned %lu (ulOutBufLen = %lu)\n", dwRetVal, ulOutBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            pAdapterInfo = (PIP_ADAPTER_INFO) malloc(ulOutBufLen);
            if (pAdapterInfo == NULL) {
                printf("get_interfaces: Malloc failed! Error: %lu\n", GetLastError());
                return 0; // Memory allocation failed
            }
            printf("get_interfaces: Malloc successful, pAdapterInfo = %p\n", pAdapterInfo);
        } else if (dwRetVal == NO_ERROR) {
            // This case means there are no adapters or the initial buffer was somehow sufficient (unlikely for ulOutBufLen=0)
            printf("get_interfaces: No adapters found or unexpected NO_ERROR on first call. Error: %lu\n", WSAGetLastError());
            return 0;
        } else {
            printf("get_interfaces: GetAdaptersInfo failed with error: %lu\n", WSAGetLastError());
            return 0;
        }

        printf("get_interfaces: Second GetAdaptersInfo call (ulOutBufLen = %lu)\n", ulOutBufLen);
        if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == NO_ERROR) {
            printf("get_interfaces: Second GetAdaptersInfo successful\n");
            for (PIP_ADAPTER_INFO pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
                printf("get_interfaces: Found adapter: %s, IP: %s\n", pAdapter->AdapterName, pAdapter->IpAddressList.IpAddress.String);
                interfaces.push_back(pAdapter->IpAddressList.IpAddress.String);
            }
        } else {
            printf("get_interfaces: Second GetAdaptersInfo failed with error: %lu\n", WSAGetLastError());
        }
        printf("get_interfaces: Freeing pAdapterInfo\n");
        free(pAdapterInfo); // Free the allocated memory
    */
#endif
    // printf("get_interfaces: Exit, found %zu interfaces\n", interfaces.size()); // This line was outside the #ifdef, but it's a printf, so comment it out too
    return 0; // Return 0 for now
}

    void UDPSocket::get_ip_and_mask(const char* interface, unsigned long &ip, unsigned long &sub_mask)
    {
        // This implementation is for Windows
#ifdef _WIN32
        printf("get_ip_and_mask: Entry for interface %s\n", interface);
        PIP_ADAPTER_INFO pAdapterInfo = NULL;
        ULONG ulOutBufLen = 0; // Initialize to 0

        printf("get_ip_and_mask: First GetAdaptersInfo call (ulOutBufLen = %lu)\n", ulOutBufLen);
        DWORD dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
        printf("get_ip_and_mask: First GetAdaptersInfo returned %lu (ulOutBufLen = %lu)\n", dwRetVal, ulOutBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            pAdapterInfo = (PIP_ADAPTER_INFO) malloc(ulOutBufLen);
            if (pAdapterInfo == NULL) {
                printf("get_ip_and_mask: Malloc failed! Error: %lu\n", GetLastError());
                return; // Memory allocation failed
            }
            printf("get_ip_and_mask: Malloc successful, pAdapterInfo = %p\n", pAdapterInfo);
        } else if (dwRetVal == NO_ERROR) {
            printf("get_ip_and_mask: No adapters found or unexpected NO_ERROR on first call. Error: %lu\n", WSAGetLastError());
            return;
        } else {
            printf("get_ip_and_mask: GetAdaptersInfo failed with error: %lu\n", WSAGetLastError());
            return;
        }

        printf("get_ip_and_mask: Second GetAdaptersInfo call (ulOutBufLen = %lu)\n", ulOutBufLen);
        if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == NO_ERROR) {
            printf("get_ip_and_mask: Second GetAdaptersInfo successful\n");
            for (PIP_ADAPTER_INFO pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
                printf("get_ip_and_mask: Checking adapter: %s, IP: %s\n", pAdapter->AdapterName, pAdapter->IpAddressList.IpAddress.String);
                if (strcmp(interface, pAdapter->IpAddressList.IpAddress.String) == 0)
                {
                    ip = inet_addr(pAdapter->IpAddressList.IpAddress.String);
                    sub_mask = inet_addr(pAdapter->IpAddressList.IpMask.String);
                    printf("get_ip_and_mask: Found matching interface. IP: %lu, Subnet: %lu\n", ip, sub_mask);
                    break;
                }
            }
        }
        else {
            printf("get_ip_and_mask: Second GetAdaptersInfo failed with error: %lu\n", WSAGetLastError());
        }
        printf("get_ip_and_mask: Freeing pAdapterInfo\n");
        free(pAdapterInfo);
#endif
        printf("get_ip_and_mask: Exit\n");
    }

    

}   // end namespace SocketLib
