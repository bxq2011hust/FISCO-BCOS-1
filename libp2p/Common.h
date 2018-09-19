/*
    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Common.h
 * @author Gav Wood <i@gavwood.com>
 * @author Alex Leverington <nessence@gmail.com>
 * @date 2014
 *
 * Miscellanea required for the Host/Session/NodeTable classes.
 */

#pragma once

#include <set>
#include <string>
#include <vector>

// Make sure boost/asio.hpp is included before windows.h.
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/logic/tribool.hpp>

#include <libdevcore/Exceptions.h>
#include <libdevcore/Guards.h>
#include <libdevcore/RLP.h>
#include <libdevcore/easylog.h>
#include <libdevcrypto/Common.h>
#include <chrono>

namespace ba = boost::asio;
namespace bi = boost::asio::ip;

namespace dev
{
namespace p2p
{
extern unsigned c_defaultIPPort;

class NodeIPEndpoint;
class Node;
extern const NodeIPEndpoint UnspecifiedNodeIPEndpoint;
extern const Node UnspecifiedNode;

using NodeID = h512;

bool isPrivateAddress(bi::address const& _addressToCheck);
bool isPrivateAddress(std::string const& _addressToCheck);
bool isLocalHostAddress(bi::address const& _addressToCheck);
bool isLocalHostAddress(std::string const& _addressToCheck);
bool isPublicAddress(bi::address const& _addressToCheck);
bool isPublicAddress(std::string const& _addressToCheck);

struct NetworkStartRequired : virtual dev::Exception
{
};
struct InvalidPublicIPAddress : virtual dev::Exception
{
};
struct InvalidHostIPAddress : virtual dev::Exception
{
};

enum PacketType
{
    HelloPacket = 0,
    DisconnectPacket,
    PingPacket,
    PongPacket,
    GetPeersPacket,
    PeersPacket,
    UserPacket = 0x10
};

enum DisconnectReason
{
    DisconnectRequested = 0,
    TCPError,
    BadProtocol,
    UselessPeer,
    TooManyPeers,
    DuplicatePeer,
    IncompatibleProtocol,
    NullIdentity,
    ClientQuit,
    UnexpectedIdentity,
    LocalIdentity,
    PingTimeout,
    UserReason = 0x10,
    NoDisconnect = 0xffff
};

///< The protocolID of synchronous Package, related to Service::sendMessageByNodeID
static int16_t g_synchronousPackageProtocolID = 32767;

///< P2PExceptionType and g_P2PExceptionMsg used in P2PException
enum P2PExceptionType
{
    Success = 0,
    ProtocolError,
    NetworkTimeout,
    P2PExceptionTypeCnt
};

static std::string g_P2PExceptionMsg[P2PExceptionTypeCnt] = {
    "Success", "ProtocolError", "NetworkTimeout"};

enum PacketDecodeStatus
{
    PACKET_ERROR = -1,
    PACKET_INCOMPLETE = 0
};

struct Options
{
    uint32_t timeout;  /// < The timeout value of async function, in milliseconds.
};

class Message : public std::enable_shared_from_this<Message>
{
public:
    typedef std::shared_ptr<Message> Ptr;

    const static size_t HEADER_LENGTH = 12;
    const static size_t MAX_LENGTH = 1024 * 1024;  ///< The maximum length of data is 1M.

    Message() { m_buffer = std::make_shared<bytes>(); }

    virtual ~Message() {}

    uint32_t length() { return m_length; }
    void setLength(uint32_t _length) { m_length = _length; }

    int16_t protocolID() { return m_protocolID; }
    void setProtocolID(int16_t _protocolID) { m_protocolID = _protocolID; }
    uint16_t packetType() { return m_packetType; }
    void setPacketType(uint16_t _packetType) { m_packetType = _packetType; }

    uint32_t seq() { return m_seq; }
    void setSeq(uint32_t _seq) { m_seq = _seq; }

    std::shared_ptr<bytes> buffer() { return m_buffer; }
    void setBuffer(std::shared_ptr<bytes> _buffer) { m_buffer = _buffer; }

    bool isRequestPacket() { return (m_protocolID > 0); }
    int16_t getResponceProtocolID()
    {
        if (isRequestPacket())
            return -m_protocolID;
        else
            return 0;
    }

    ///< encode and decode logic implemented in send() and doRead() of session.cpp
    void encode(bytes& buffer);

    /// < If the decoding is successful, the length of the decoded data is returned; otherwise, 0 is
    /// returned.
    ssize_t decode(const byte* buffer, size_t size);

    void Print(std::string const& str)
    {
        LOG(INFO) << str << ",Message(" << m_length << "," << m_protocolID << "," << m_packetType
                  << "," << m_seq << ","
                  << std::string((const char*)m_buffer->data(), m_buffer->size()) << ")";
    }

private:
    uint32_t m_length = 0;      ///< m_length = HEADER_LENGTH + length(m_buffer)
    int16_t m_protocolID = 0;   ///< message type, the first two bytes of information, when greater
                                ///< than 0 is the ID of the request package.
    uint16_t m_packetType = 0;  ///< message sub type, the second two bytes of information
    uint32_t m_seq = 0;         ///< the message identify
    std::shared_ptr<bytes> m_buffer;  ///< message data
};

class P2PException : public std::exception
{
public:
    P2PException(){};
    P2PException(int _errorCode, const std::string& _msg) : m_errorCode(_errorCode), m_msg(_msg){};

    virtual int errorCode() { return m_errorCode; };
    virtual const char* what() const _GLIBCXX_USE_NOEXCEPT override { return m_msg.c_str(); };
    bool operator!() const { return m_errorCode == 0; }

private:
    int m_errorCode = 0;
    std::string m_msg = "";
};

#define CallbackFunc std::function<void(P2PException, Message::Ptr)>
struct ResponseCallback : public std::enable_shared_from_this<ResponseCallback>
{
    typedef std::shared_ptr<ResponseCallback> Ptr;

    CallbackFunc m_callbackFunc;
    std::shared_ptr<boost::asio::deadline_timer> m_timeoutHandler;
};

struct SessionCallback : public std::enable_shared_from_this<SessionCallback>
{
public:
    typedef std::shared_ptr<SessionCallback> Ptr;

    SessionCallback() { m_mutex.lock(); }

    void onResponse(P2PException _error, Message::Ptr _message)
    {
        m_error = _error;
        m_response = _message;
        m_mutex.unlock();
    }

    P2PException m_error;
    Message::Ptr m_response;
    std::mutex m_mutex;
};

inline bool isPermanentProblem(DisconnectReason _r)
{
    switch (_r)
    {
    case DuplicatePeer:
    case IncompatibleProtocol:
    case NullIdentity:
    case UnexpectedIdentity:
    case LocalIdentity:
        return true;
    default:
        return false;
    }
}

/// @returns the string form of the given disconnection reason.
std::string reasonOf(DisconnectReason _r);

using CapDesc = std::pair<std::string, u256>;
using CapDescSet = std::set<CapDesc>;
using CapDescs = std::vector<CapDesc>;


class HostResolver
{
public:
    static boost::asio::ip::address query(std::string);
};
enum PeerSlotType
{
    Egress,
    Ingress
};
struct NodeInfo
{
    NodeInfo() = default;
    NodeInfo(
        NodeID const& _id, std::string const& _address, unsigned _port, std::string const& _version)
      : id(_id), address(_address), port(_port), version(_version)
    {}

    std::string enode() const
    {
        return "enode://" + id.hex() + "@" + address + ":" + toString(port);
    }

    NodeID id;
    std::string address;
    unsigned port;
    std::string version;
};

/**
 * @brief IPv4,UDP/TCP endpoints.
 */
class NodeIPEndpoint
{
public:
    enum RLPAppend
    {
        StreamList,
        StreamInline
    };

    /// Setting true causes isAllowed to return true for all addresses. (Used by test fixtures)
    static bool test_allowLocal;

    NodeIPEndpoint() = default;
    NodeIPEndpoint(bi::address _addr, uint16_t _udp, uint16_t _tcp)
      : address(_addr), udpPort(_udp), tcpPort(_tcp)
    {}
    NodeIPEndpoint(std::string _host, uint16_t _udp, uint16_t _tcp)
      : udpPort(_udp), tcpPort(_tcp), host(_host)
    {
        address = HostResolver::query(host);
    }
    NodeIPEndpoint(RLP const& _r) { interpretRLP(_r); }
    NodeIPEndpoint(const NodeIPEndpoint& _nodeIPEndpoint)
    {
        address = _nodeIPEndpoint.address;
        udpPort = _nodeIPEndpoint.udpPort;
        tcpPort = _nodeIPEndpoint.tcpPort;
        host = _nodeIPEndpoint.host;
    }
    operator bi::udp::endpoint() const { return bi::udp::endpoint(address, udpPort); }
    operator bi::tcp::endpoint() const { return bi::tcp::endpoint(address, tcpPort); }

    operator bool() const { return !address.is_unspecified() && udpPort > 0 && tcpPort > 0; }

    bool isAllowed() const
    {
        return NodeIPEndpoint::test_allowLocal ? !address.is_unspecified() :
                                                 isPublicAddress(address);
    }
    bool operator==(NodeIPEndpoint const& _cmp) const
    {
        return address == _cmp.address && udpPort == _cmp.udpPort && tcpPort == _cmp.tcpPort;
    }
    bool operator!=(NodeIPEndpoint const& _cmp) const { return !operator==(_cmp); }
    bool operator<(const dev::p2p::NodeIPEndpoint& rhs) const
    {
        if (address < rhs.address)
        {
            return true;
        }

        return tcpPort < rhs.tcpPort;
    }
    void streamRLP(RLPStream& _s, RLPAppend _append = StreamList) const;
    void interpretRLP(RLP const& _r);
    std::string name() const
    {
        std::ostringstream os;
        os << address.to_string() << ":" << tcpPort << ":" << udpPort;
        return os.str();
    }
    bool isValid() { return (!address.to_string().empty()) && (tcpPort != 0); }
    // TODO: make private, give accessors and rename m_...
    bi::address address;
    uint16_t udpPort = 0;
    uint16_t tcpPort = 0;
    std::string host;
};

/*
 * Used by Host to pass negotiated information about a connection to a
 * new Peer Session; PeerSessionInfo is then maintained by Session and can
 * be queried for point-in-time status information via Host.
 */
struct PeerSessionInfo
{
    NodeID const id;
    std::string const host;
    std::chrono::steady_clock::duration lastPing;
    unsigned socketId;
    std::map<std::string, std::string> notes;
};

using PeerSessionInfos = std::vector<PeerSessionInfo>;

struct NodeSpec
{
    NodeSpec() {}

    /// Accepts user-readable strings of the form (enode://pubkey@)host({:port,:tcpport.udpport})
    NodeSpec(std::string const& _user);

    NodeSpec(std::string const& _addr, uint16_t _port, int _udpPort = -1)
      : m_address(_addr), m_tcpPort(_port), m_udpPort(_udpPort == -1 ? _port : (uint16_t)_udpPort)
    {}

    NodeID id() const { return m_id; }

    NodeIPEndpoint nodeIPEndpoint() const;

    std::string enode() const;

private:
    std::string m_address;
    uint16_t m_tcpPort = 0;
    uint16_t m_udpPort = 0;
    NodeID m_id;
};
class DeadlineOps
{
    class DeadlineOp
    {
    public:
        DeadlineOp(ba::io_service& _io, unsigned _msInFuture,
            std::function<void(boost::system::error_code const&)> const& _f)
          : m_timer(new ba::deadline_timer(_io))
        {
            m_timer->expires_from_now(boost::posix_time::milliseconds(_msInFuture));
            m_timer->async_wait(_f);
        }
        ~DeadlineOp()
        {
            if (m_timer)
                m_timer->cancel();
        }

        DeadlineOp(DeadlineOp&& _s) : m_timer(_s.m_timer.release()) {}
        DeadlineOp& operator=(DeadlineOp&& _s)
        {
            assert(&_s != this);

            m_timer.reset(_s.m_timer.release());
            return *this;
        }

        bool expired()
        {
            Guard l(x_timer);
            return m_timer->expires_from_now().total_nanoseconds() <= 0;
        }
        void wait()
        {
            Guard l(x_timer);
            m_timer->wait();
        }

    private:
        std::unique_ptr<ba::deadline_timer> m_timer;
        Mutex x_timer;
    };

public:
    DeadlineOps(ba::io_service& _io, unsigned _reapIntervalMs = 100)
      : m_io(_io), m_reapIntervalMs(_reapIntervalMs), m_stopped(false)
    {
        reap();
    }
    ~DeadlineOps() { stop(); }

    void schedule(
        unsigned _msInFuture, std::function<void(boost::system::error_code const&)> const& _f)
    {
        if (m_stopped)
            return;
        DEV_GUARDED(x_timers) m_timers.emplace_back(m_io, _msInFuture, _f);
    }

    void stop()
    {
        m_stopped = true;
        DEV_GUARDED(x_timers) m_timers.clear();
    }

    bool isStopped() const { return m_stopped; }

protected:
    void reap();

private:
    ba::io_service& m_io;
    unsigned m_reapIntervalMs;

    std::vector<DeadlineOp> m_timers;
    Mutex x_timers;

    std::atomic<bool> m_stopped;
};

}  // namespace p2p

/// Simple stream output for a NodeIPEndpoint.
std::ostream& operator<<(std::ostream& _out, dev::p2p::NodeIPEndpoint const& _ep);
}  // namespace dev

/// std::hash for asio::adress
namespace std
{
template <>
struct hash<bi::address>
{
    size_t operator()(bi::address const& _a) const
    {
        if (_a.is_v4())
            return std::hash<unsigned long>()(_a.to_v4().to_ulong());
        if (_a.is_v6())
        {
            auto const& range = _a.to_v6().to_bytes();
            return boost::hash_range(range.begin(), range.end());
        }
        if (_a.is_unspecified())
            return static_cast<size_t>(0x3487194039229152ull);  // Chosen by fair dice roll,
                                                                // guaranteed to be random
        return std::hash<std::string>()(_a.to_string());
    }
};

}  // namespace std
