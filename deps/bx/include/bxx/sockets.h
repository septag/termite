#pragma once

#include "../bx/bx.h"
#include "../bx/allocator.h"
#include "../bx/readerwriter.h"

#include "string.h"

#if BX_PLATFORM_WINDOWS
#   include <WinSock2.h>
namespace bx
{
    typedef SOCKET SocketId;
}

#   define SOCK_NULL INVALID_SOCKET
#   define SOCK_ERROR SOCKET_ERROR
#else
namespace bx
{
    typedef int SocketId;
}

#   define SOCK_NULL -1
#   define SOCK_ERROR -1
#endif

namespace bx
{
    enum class SocketType
    {
        UDP,
        TCP
    };

    enum class SocketAddrType
    {
        IPv4,
        IPv6
    };

    class SocketAddr
    {
    public:
        SocketAddrType type;
        int port;

        union AddrIP
        {
            struct IPv4
            {
                uint32_t ip;
            } v4;

            struct IPv6
            {
                uint8_t ip[16];
            } v6;
        } addr;

    public:
        SocketAddr(uint32_t ip4, int port);
        SocketAddr(uint8_t ip6[16], int port);

        String64 ipToString() const;
        bool isValid() const;
    };

    class Socket
    {
    protected:
        Socket();
        virtual ~Socket();

    public:
        bool setBufferedWrite(int size, AllocatorI* alloc);
        void setBufferedWritePtr(void* buffer, int size);

        bool pollRead(int timeoutMs) const;
        bool pollWrite(int timeoutMs) const;

        void close();
        bool isOpen() const;

        virtual void flush() = 0;

    protected:
        uint8_t* m_buffer;
        int m_bufferSize;
        int m_bufferOffset;
        AllocatorI* m_alloc;
        SocketId m_sock;
    };

    class SocketTCP : public Socket, ReaderI, WriterI
    {
    public:
        SocketTCP();
        virtual ~SocketTCP();

        bool listen(int port);
        SocketTCP accept();
        bool connect(const SocketAddr& addr);
        const SocketAddr& getPeerAddr() const { return m_peerAddr; }

        int32_t read(void* data, int32_t size, Error* err = nullptr) override;
        int32_t write(const void* data, int32_t size, Error* err = nullptr) override;

        void flush() override;

    private:
        SocketAddr m_peerAddr;
    };

    class SocketUDP : public Socket, ReaderI, WriterI
    {
    public:
        SocketUDP();
        virtual ~SocketUDP();

        bool bind(int port);
        void setRemoteAddr(const SocketAddr& addr);
        const SocketAddr& getRemoteAddr() const { return m_remoteAddr; }

        int32_t read(void* data, int32_t size, Error* err = nullptr) override;
        int32_t write(const void* data, int32_t size, Error* err = nullptr) override;

        void flush() override;

    private:
        SocketAddr m_remoteAddr;
    };

    // Globals
    const char* getHostName();
    String64 resolveDNS(const char* server_name);
    uint32_t strToIp4(const char* addr);
    SocketAddr::AddrIP::IPv6 strToIp6(const char* addr);
}

#ifdef BX_IMPLEMENT_SOCKETS
#   undef BX_IMPLEMENT_SOCKETS
#   include <cassert>

#if !BX_PLATFORM_WINDOWS
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#   include <netdb.h>

#   define closesocket ::close
#   define INET6_ADDR(a)   a.__in6_u.__u6_addr8
#else
#   include <Ws2tcpip.h>

#define inet_ntop InetNtop
#   define INET6_ADDR(a)   a.u.Byte
 typedef int socklen_t;

static const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    struct sockaddr_storage ss;
    unsigned long s = size;

    bx::memSet(&ss, 0x00, sizeof(ss));
    ss.ss_family = af;

    switch (af) {
    case AF_INET:
        ((sockaddr_in*)&ss)->sin_addr = *(struct in_addr*)src;
        break;
    case AF_INET6:
        ((sockaddr_in6*)&ss)->sin6_addr = *(struct in6_addr*)src;
        break;
    default:
        return NULL;
    }
    // cannot direclty use &size because of strict aliasing rules
    return (WSAAddressToString((sockaddr*)&ss, sizeof(ss), NULL, dst, &s) == 0) ? dst : NULL;
}

namespace bx {
    typedef int socklen_t;
}
#endif

namespace bx
{
    SocketAddr::SocketAddr(uint32_t ip4, int port)
    {
        this->type = SocketAddrType::IPv4;
        bx::memSet(this->addr.v6.ip, 0x00, sizeof(this->addr.v6.ip));
        this->addr.v4.ip = ip4;
        this->port = port;
    }

    SocketAddr::SocketAddr(uint8_t ip6[], int port)
    {
        this->type = SocketAddrType::IPv6;
        memcpy(this->addr.v6.ip, ip6, sizeof(this->addr.v6.ip));
        this->port = port;
    }

    String64 SocketAddr::ipToString() const
    {
        String64 r;
        if (this->type == SocketAddrType::IPv4) {
            in_addr a;
            a.s_addr = htonl(this->addr.v4.ip);
            inet_ntop(AF_INET, &a, r.getBuffer(), sizeof(r));
        } else if (this->type == SocketAddrType::IPv6) {
            in6_addr a;
            memcpy(INET6_ADDR(a), this->addr.v6.ip, sizeof(this->addr.v6.ip));
            inet_ntop(AF_INET6, &a, r.getBuffer(), sizeof(r));
        }
        return r;
    }

    bool SocketAddr::isValid() const
    {
        if (this->type == SocketAddrType::IPv4) {
            return this->addr.v4.ip != 0;
        } else if (this->type == SocketAddrType::IPv6) {
            const char zero[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            return memcmp(this->addr.v6.ip, zero, sizeof(this->addr.v6.ip)) == 0;
        }
        return false;
    }

    uint32_t strToIp4(const char* addr)
    {
        in_addr a;
        inet_pton(AF_INET, addr, &a);
        return (uint32_t)ntohl(a.s_addr);
    }

    SocketAddr::AddrIP::IPv6 strToIp6(const char* addr)
    {
        in6_addr a;
        inet_pton(AF_INET6, addr, &a);
        SocketAddr::AddrIP::IPv6 r;
        memcpy(r.ip, INET6_ADDR(a), sizeof(r.ip));
        return r;

    }

    // Socket
    Socket::Socket()
    {
        m_buffer = nullptr;
        m_bufferSize = 0;
        m_bufferOffset = 0;
        m_alloc = nullptr;
        m_sock = SOCK_NULL;
    }

    Socket::~Socket()
    {
        assert(m_buffer == nullptr);
        assert(m_sock == SOCK_NULL);
    }

    bool Socket::setBufferedWrite(int size, AllocatorI* alloc)
    {
        assert(alloc);
        assert(size);

        m_buffer = (uint8_t*)BX_ALLOC(alloc, size);
        if (!m_buffer)
            return false;

        m_bufferSize = size;
        m_bufferOffset = 0;

        return true;
    }

    void Socket::setBufferedWritePtr(void* buffer, int size)
    {
        assert(buffer);
        assert(size);

        m_buffer = (uint8_t*)buffer;
        m_bufferSize = size;
        m_bufferOffset = 0;
    }

    bool Socket::pollRead(int timeoutMs) const
    {
        assert(m_sock != SOCK_NULL);
        SocketId sock = m_sock;

        timeval tmout = { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(sock, &readset);

        if (select((int)(sock + 1), &readset, NULL, NULL, &tmout) > 0 && FD_ISSET(sock, &readset))
            return true;

        return false;
    }

    bool Socket::pollWrite(int timeoutMs) const
    {
        assert(m_sock != SOCK_NULL);
        SocketId sock = m_sock;

        timeval tmout = { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
        fd_set writeset;
        FD_ZERO(&writeset);
        FD_SET(sock, &writeset);

        if (select((int)(sock + 1), NULL, &writeset, NULL, &tmout) > 0 && FD_ISSET(sock, &writeset))
            return true;

        return false;
    }

    void Socket::close()
    {
        flush();

        if (m_buffer && m_alloc)
            BX_FREE(m_alloc, m_buffer);
        m_buffer = nullptr;
        m_alloc = nullptr;

        if (m_sock != SOCK_NULL) {
            closesocket(m_sock);
            m_sock = SOCK_NULL;
        }
    }

    bool Socket::isOpen() const
    {
        return m_sock != SOCK_NULL;
    }

    // SocketTCP
    SocketTCP::SocketTCP() : m_peerAddr((uint32_t)0, 0)
    {

    }

    SocketTCP::~SocketTCP()
    {
    }

    bool SocketTCP::listen(int port)
    {
        assert(m_sock == SOCK_NULL);

        // Create socket
        m_sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_sock == SOCK_NULL)
            return false;

        // Listen
        sockaddr_in addr;
        bx::memSet(&addr, 0x00, sizeof(addr));

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (::bind(m_sock, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERROR) {
            close();
            return false;
        }

        if (::listen(m_sock, 1) == SOCK_ERROR) {
            close();
            return false;
        }

        return true;
    }

    SocketTCP SocketTCP::accept()
    {
        SocketTCP r;
        sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        r.m_sock = ::accept(m_sock, (sockaddr*)&addr, &addrlen);

        // Save Peer address
        if (r.isOpen())
            r.m_peerAddr = SocketAddr(ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port));

        return r;
    }

    bool SocketTCP::connect(const SocketAddr& addr)
    {
        assert(m_sock == SOCK_NULL);

        if (addr.type == SocketAddrType::IPv4) {
            // Create IP4 socket
            m_sock = ::socket(AF_INET, SOCK_STREAM, 0);
            if (m_sock == SOCK_NULL)
                return false;

            // Connect
            sockaddr_in sa;
            bx::memSet(&sa, 0x00, sizeof(sa));
            sa.sin_family = AF_INET;
            sa.sin_port = htons(addr.port);
            sa.sin_addr.s_addr = htonl(addr.addr.v4.ip);
            int r = ::connect(m_sock, (const sockaddr*)&sa, sizeof(sa));

            if (r != SOCK_ERROR)
                m_peerAddr = addr;
            else
                close();

            return r != SOCK_ERROR;
        } else if (addr.type == SocketAddrType::IPv6) {
            // Create IP6 socket
            m_sock = ::socket(AF_INET6, SOCK_STREAM, 0);
            if (m_sock == SOCK_NULL)
                return false;

            // Connect
            sockaddr_in6 sa;
            bx::memSet(&sa, 0x00, sizeof(sa));
            sa.sin6_family = AF_INET6;
            sa.sin6_port = htons(addr.port);
            memcpy(INET6_ADDR(sa.sin6_addr), addr.addr.v6.ip, sizeof(addr.addr.v6.ip));
            int r = ::connect(m_sock, (const sockaddr*)&sa, sizeof(sa));

            if (r != SOCK_ERROR)
                m_peerAddr = addr;
            else
                close();

            return r != SOCK_ERROR;
        }

        assert(0);
        return false;
    }

    int32_t SocketTCP::read(void* data, int32_t size, Error* err)
    {
        if (m_sock == SOCK_NULL)
            return -1;

        return (int)::recv(m_sock, (char*)data, size, 0);
    }

    int32_t SocketTCP::write(const void* data, int32_t size, Error* err)
    {
        if (m_sock == SOCK_NULL)
            return -1;

        // Buffered write
        if (m_buffer) {
            if (m_bufferSize >= (m_bufferOffset + size)) {
                memcpy(m_buffer + m_bufferOffset, data, size);
                m_bufferOffset += size;
                // Reached the end, flush
                if (m_bufferOffset == m_bufferSize)
                    flush();
            } else if (m_bufferSize < size) {
                flush();
                return (int)::send(m_sock, (const char*)data, size, 0);
            } else {
                int remain = m_bufferSize - m_bufferOffset;
                memcpy(m_buffer + m_bufferOffset, data, remain);
                flush();
                memcpy(m_buffer, (uint8_t*)data + remain, size - remain);
                m_bufferOffset += (size - remain);
            }
        } else {
            return (int)::send(m_sock, (const char*)data, size, 0);
        }

        return 0;
    }

    void SocketTCP::flush()
    {
        if (m_bufferOffset && m_sock != SOCK_NULL) {
            ::send(m_sock, (const char*)m_buffer, m_bufferOffset, 0);
            m_bufferOffset = 0;
        }
    }

    // SocketUDP
    SocketUDP::SocketUDP() : m_remoteAddr((uint32_t)0, 0)
    {
    }

    SocketUDP::~SocketUDP()
    {
    }

    bool SocketUDP::bind(int port)
    {
        assert(m_sock == SOCK_NULL);

        // Create socket
        m_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (m_sock == SOCK_NULL)
            return false;

        // Bind
        sockaddr_in addr;
        bx::memSet(&addr, 0x00, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (::bind(m_sock, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERROR)
            return false;

        return true;
    }

    void SocketUDP::setRemoteAddr(const SocketAddr& addr)
    {
        m_remoteAddr = addr;
    }

    int32_t SocketUDP::read(void* data, int32_t size, Error* err)
    {
        if (m_sock == SOCK_NULL)
            return -1;

        sockaddr_storage addr;
        bx::memSet(&addr, 0x00, sizeof(sockaddr_storage));
        socklen_t addrlen = sizeof(addr);

        int r = (int)::recvfrom(m_sock, (char*)data, size, 0, (sockaddr*)&addr, &addrlen);

        // Update remote address
        if (r) {
            if (addr.ss_family == AF_INET) {
                m_remoteAddr = SocketAddr(((sockaddr_in*)&addr)->sin_addr.s_addr, 
                                          ntohs(((sockaddr_in*)&addr)->sin_port));
            } else if (addr.ss_family == AF_INET6) {
                m_remoteAddr = SocketAddr(INET6_ADDR(((sockaddr_in6*)&addr)->sin6_addr), 
                                          ntohs(((sockaddr_in6*)&addr)->sin6_port));
            }
        }

        return r;
    }

    int32_t SocketUDP::write(const void* data, int32_t size, Error* err)
    {
        if (!m_remoteAddr.isValid())
            return -1;

        // Create socket
        if (m_sock == SOCK_NULL) {
            m_sock = ::socket(m_remoteAddr.type == SocketAddrType::IPv4 ? AF_INET : AF_INET6, SOCK_DGRAM, 0);
            if (m_sock == SOCK_NULL)
                return -1;
        }

        int write_bytes = size;

        // Prepare remote address for Berkley socket
        sockaddr_storage addr;
        bx::memSet(&addr, 0x00, sizeof(addr));
        socklen_t addr_len;
        if (m_remoteAddr.type == SocketAddrType::IPv4) {
            addr.ss_family = AF_INET;
            ((sockaddr_in*)&addr)->sin_addr.s_addr = htonl(m_remoteAddr.addr.v4.ip);
            ((sockaddr_in*)&addr)->sin_port = htons(m_remoteAddr.port);
            addr_len = sizeof(sockaddr_in);
        } else {
            addr.ss_family = AF_INET6;
            memcpy(INET6_ADDR(((sockaddr_in6*)&addr)->sin6_addr), m_remoteAddr.addr.v6.ip, sizeof(m_remoteAddr.addr.v6.ip));
            ((sockaddr_in6*)&addr)->sin6_port = htons(m_remoteAddr.port);
            addr_len = sizeof(sockaddr_in6);
        }

        // Buffered write
        if (m_buffer) {
            if (m_bufferSize >= (m_bufferOffset + write_bytes)) {
                memcpy(m_buffer + m_bufferOffset, data, write_bytes);
                m_bufferOffset += write_bytes;
                // Reached the end, flush
                if (m_bufferOffset == m_bufferSize)
                    flush();
            } else if (m_bufferSize < write_bytes) {
                flush();
                return (int)::sendto(m_sock, (const char*)data, write_bytes, 0, (sockaddr*)&addr, addr_len);
            } else {
                int remain = m_bufferSize - m_bufferOffset;
                memcpy(m_buffer + m_bufferOffset, data, remain);
                flush();
                memcpy(m_buffer, (uint8_t*)data + remain, write_bytes - remain);
                m_bufferOffset += (write_bytes - remain);
            }
        } else {
            return (int)::sendto(m_sock, (const char*)data, write_bytes, 0, (sockaddr*)&addr, addr_len);
        }
        return 0;
    }

    void SocketUDP::flush()
    {
        if (!m_remoteAddr.isValid())
            return;

        if (m_bufferOffset && m_sock != SOCK_NULL) {
            // Prepare remote address for Berkley socket
            sockaddr_storage addr;
            socklen_t addr_len;
            bx::memSet(&addr, 0x00, sizeof(addr));
            if (m_remoteAddr.type == SocketAddrType::IPv4) {
                addr.ss_family = AF_INET;
                ((sockaddr_in*)&addr)->sin_addr.s_addr = htonl(m_remoteAddr.addr.v4.ip);
                ((sockaddr_in*)&addr)->sin_port = htons(m_remoteAddr.port);
                addr_len = sizeof(sockaddr_in);
            } else {
                addr.ss_family = AF_INET6;
                memcpy(INET6_ADDR(((sockaddr_in6*)&addr)->sin6_addr), m_remoteAddr.addr.v6.ip, 
                       sizeof(m_remoteAddr.addr.v6.ip));
                ((sockaddr_in6*)&addr)->sin6_port = htons(m_remoteAddr.port);
                addr_len = sizeof(sockaddr_in6);
            }

            // Send
            ::sendto(m_sock, (const char*)m_buffer, m_bufferOffset, 0, (sockaddr*)&addr, addr_len);
            m_bufferOffset = 0;
        }
    }


}   // namespace: bx

#endif  // if BX_IMPLEMENT_SOCKETS

