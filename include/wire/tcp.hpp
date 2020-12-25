#ifndef CS120_TCP_HPP
#define CS120_TCP_HPP


#include <cstdint>

#include "utility.hpp"
#include "ipv4.hpp"


namespace cs120 {
enum class TCPOption : uint8_t {
    End = 0,
    NoOperation = 1,
    MaximumSegmentSize = 2,
    WindowScaleFactor = 3,
    SACKPermitted = 4,
    SACK = 5,
    Echo = 6,
    EchoReply = 7,
    Timestamp = 8,
};


class TCPOptionIter {
private:
    Slice<uint8_t> buffer;
    size_t index;

public:
    using Item = std::pair<TCPOption, Slice<uint8_t>>;

    explicit TCPOptionIter(Slice<uint8_t> buffer) : buffer{buffer}, index{0} {}

    Item next() {
        if (index + 1 >= buffer.size()) {
            return std::make_pair(TCPOption::End, Slice<uint8_t>{});
        }

        TCPOption option{buffer[index]};
        while (option == TCPOption::NoOperation) { option = TCPOption{buffer[++index]}; }

        size_t size = buffer[index + 1];

        if (index + size > buffer.size()) {
            return std::make_pair(TCPOption::End, Slice<uint8_t>{});
        } else {
            auto result = std::make_pair(option, buffer[Range{index}][Range{2, size}]);
            index += size;
            return result;
        }
    }

    bool end(const Item &item) const { return item.first == TCPOption::End; }
};


enum class TCPFlags {
    Sync,
    SyncAck,
    Ack,
    Fin,
    Reset,
    Error,
};

struct TCPHeader {
private:
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t sequence;
    uint32_t ack_number;
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
    uint8_t reserve: 4, offset: 4;
#elif __DARWIN_BYTE_ORDER == __DARWIN_BIG_ENDIAN
    uint8_t offset: 4, reserve: 4;
#else
#error "fix byte order"
#endif
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;

public:
    static constexpr uint8_t NONCE_SUM = 0x01;
    static constexpr uint8_t FLAGS_CWR = 0x80;
    static constexpr uint8_t FLAGS_ECE = 0x40;
    static constexpr uint8_t FLAGS_URGENT = 0x20;
    static constexpr uint8_t FLAGS_ACK = 0x10;
    static constexpr uint8_t FLAGS_PUSH = 0x08;
    static constexpr uint8_t FLAGS_RESET = 0x04;
    static constexpr uint8_t FLAGS_SYNC = 0x02;
    static constexpr uint8_t FLAGS_FIN = 0x01;

    static size_t max_payload(size_t mtu) {
        return IPV4Header::max_payload(mtu) - sizeof(TCPHeader);
    }

    TCPHeader(uint16_t src_port, uint16_t dest_port, uint32_t sequence, uint32_t ack_number,
              size_t header_length, bool nonce_sum, bool cwr, bool ece, bool urgent, bool ack,
              bool push, bool reset, bool sync, bool fin, uint16_t window) {
        set_src_port(src_port);
        set_dest_port(dest_port);
        set_sequence(sequence);
        set_ack_number(ack_number);
        set_header_length(header_length);
        set_nonce_sum(nonce_sum);
        set_flags(cwr, ece, urgent, ack, push, reset, sync, fin);
        set_window(window);
        set_checksum(0);
        set_urgent_ptr(0);
    }

    class Guard {
    private:
        const IPV4Header *ip_header;
        MutSlice<uint8_t> frame;
        MutSlice<uint8_t> inner;

    public:
        Guard() noexcept: ip_header{nullptr}, frame{}, inner{} {}

        Guard(const IPV4Header *ip_header, MutSlice<uint8_t> icmp_frame, MutSlice<uint8_t> inner) :
                ip_header{ip_header}, frame{icmp_frame} , inner{inner} {}

        Guard(Guard &&other) noexcept = default;

        Guard &operator=(Guard &&other) noexcept = default;

        MutSlice<uint8_t> &operator*() { return inner; }

        MutSlice<uint8_t> *operator->() { return &inner; }

        ~Guard() {
            auto *tcp_header = reinterpret_cast<TCPHeader *>(frame.begin());
            tcp_header->set_checksum(complement_checksum(*ip_header, frame));
        }
    };

    static Guard generate(MutSlice<uint8_t> frame, uint16_t identifier,
                          uint32_t src_ip, uint32_t dest_ip,
                          uint16_t src_port, uint16_t dest_port,
                          uint32_t sequence, uint32_t ack_number,
                          bool nonce_sum, bool cwr, bool ece, bool urgent, bool ack,
                          bool push, bool reset, bool sync, bool fin,
                          uint16_t window, size_t len) {
        size_t tcp_size = sizeof(TCPHeader) + len;

        auto tcp_frame = IPV4Header::generate(frame, identifier, IPV4Protocol::TCP,
                                              src_ip, dest_ip, tcp_size);

        if (tcp_frame.empty()) { return {}; }

        auto *tcp_header = reinterpret_cast<TCPHeader *>(tcp_frame.begin());

        new(tcp_header)TCPHeader{src_port, dest_port, sequence, ack_number,
                                 sizeof(TCPHeader), nonce_sum, cwr, ece, urgent, ack,
                                 push, reset, sync, fin, window};

        return Guard{
                reinterpret_cast<IPV4Header *>(frame.begin()),
                tcp_frame, tcp_frame[Range{sizeof(TCPHeader)}]
        };
    }

    static const TCPHeader *from_slice(Slice <uint8_t> data) {
        auto *result = reinterpret_cast<const TCPHeader *>(data.begin());
        if (data.size() < result->get_header_length() ||
            result->get_header_length() < sizeof(IPV4Header)) { return nullptr; }
        return result;
    }

    static TCPHeader *from_slice(MutSlice <uint8_t> data) {
        return const_cast<TCPHeader *>(from_slice(Slice<uint8_t>{data}));
    }

    uint16_t get_src_port() const { return ntohs(src_port); }

    void set_src_port(uint16_t value) { src_port = htons(value); }

    uint16_t get_dest_port() const { return ntohs(dest_port); }

    void set_dest_port(uint16_t value) { dest_port = htons(value); }

    uint32_t get_sequence() const { return ntohl(sequence); }

    void set_sequence(uint32_t value) { sequence = htonl(value); }

    uint32_t get_ack_number() const { return ntohl(ack_number); }

    void set_ack_number(uint32_t value) { ack_number = htonl(value); }

    size_t get_header_length() const { return offset * 4; }

    void set_header_length(size_t value) {
        if (value % 4 != 0 || value > 0b111100) { cs120_abort("invalid header length!"); }
        offset = value / 4;
    }

    bool get_nonce_sum() const { return (reserve & NONCE_SUM) > 0; }

    void set_nonce_sum(bool value) {
        reserve &= ~NONCE_SUM;
        if (value) { reserve |= NONCE_SUM; }
    }

    bool get_cwr() const { return (flags & FLAGS_CWR) > 0; }

    bool get_ece() const { return (flags & FLAGS_ECE) > 0; }

    bool get_urgent() const { return (flags & FLAGS_URGENT) > 0; }

    bool get_ack() const { return (flags & FLAGS_ACK) > 0; }

    bool get_push() const { return (flags & FLAGS_PUSH) > 0; }

    bool get_reset() const { return (flags & FLAGS_RESET) > 0; }

    bool get_sync() const { return (flags & FLAGS_SYNC) > 0; }

    bool get_fin() const { return (flags & FLAGS_FIN) > 0; }

    TCPFlags get_flags() const {
        if (get_fin()) {
            if (!get_sync() && !get_reset()) {
                return TCPFlags::Fin;
            } else {
                return TCPFlags::Error;
            }
        }

        if (get_reset()) {
            if (!get_sync() && !get_fin()) {
                return TCPFlags::Reset;
            } else {
                return TCPFlags::Error;
            }
        }

        if (get_sync()) {
            if (!get_push() && !get_urgent()) {
                if (get_ack()) {
                    return TCPFlags::SyncAck;
                } else {
                    return TCPFlags::Sync;
                }
            } else {
                return TCPFlags::Error;
            }
        }

        if (get_ack()) {
            return TCPFlags::Ack;
        } else {
            return TCPFlags::Error;
        }
    }

    void set_flags(bool cwr, bool ece, bool urgent, bool ack,
                   bool push, bool reset, bool sync, bool fin) {
        uint8_t value = 0;

        if (cwr) { value |= FLAGS_CWR; }
        if (ece) { value |= FLAGS_ECE; }
        if (urgent) { value |= FLAGS_URGENT; }
        if (ack) { value |= FLAGS_ACK; }
        if (push) { value |= FLAGS_PUSH; }
        if (reset) { value |= FLAGS_RESET; }
        if (sync) { value |= FLAGS_SYNC; }
        if (fin) { value |= FLAGS_FIN; }

        flags = value;
    }

    uint16_t get_window() const { return ntohs(window); }

    void set_window(uint16_t value) { window = htons(value); }

    uint16_t get_checksum() const { return checksum; }

    void set_checksum(uint16_t value) { checksum = value; }

    uint16_t get_urgent_ptr() const { return ntohs(urgent_ptr); }

    void set_urgent_ptr(uint16_t value) { urgent_ptr = htons(value); }

    void format() const {
        printf("TCP Header {\n");
        printf("\tsource port: %hu,\n", get_src_port());
        printf("\tdestination port: %hu,\n", get_dest_port());
        printf("\tsequence: %u,\n", get_sequence());
        printf("\tack number: %u,\n", get_ack_number());
        printf("\theader length: %zu,\n", get_header_length());
        printf("\tnonce sum: %s,\n", bool_to_string(get_nonce_sum()));
        printf("\tcwr: %s,\n", bool_to_string(get_cwr()));
        printf("\tece: %s,\n", bool_to_string(get_ece()));
        printf("\turgent: %s,\n", bool_to_string(get_urgent()));
        printf("\tack: %s,\n", bool_to_string(get_ack()));
        printf("\tpush: %s,\n", bool_to_string(get_push()));
        printf("\treset: %s,\n", bool_to_string(get_reset()));
        printf("\tsync: %s,\n", bool_to_string(get_sync()));
        printf("\tfin: %s,\n", bool_to_string(get_fin()));
        printf("\twindow: %hu,\n", get_window());
        printf("\turgent ptr: %hu,\n", get_urgent_ptr());
        printf("}\n");
    }
}__attribute__((packed));


cs120_static_inline std::tuple<TCPHeader *, MutSlice<uint8_t>, MutSlice<uint8_t>>
tcp_split(MutSlice<uint8_t> datagram) {
    auto *header = datagram.buffer_cast<TCPHeader>();
    if (header == nullptr) {
        return std::make_tuple(nullptr, MutSlice<uint8_t>{}, MutSlice<uint8_t>{});
    }
    auto option = datagram[Range{sizeof(IPV4Header), header->get_header_length()}];
    auto data = datagram[Range{header->get_header_length()}];
    return std::make_tuple(header, option, data);
}


cs120_static_inline std::tuple<const TCPHeader *, Slice<uint8_t>, Slice<uint8_t>>
tcp_split(Slice<uint8_t> datagram) {
    auto *header = datagram.buffer_cast<TCPHeader>();
    if (header == nullptr) {
        return std::make_tuple(nullptr, Slice<uint8_t>{}, Slice<uint8_t>{});
    }
    auto option = datagram[Range{sizeof(IPV4Header), header->get_header_length()}];
    auto data = datagram[Range{header->get_header_length()}];
    return std::make_tuple(header, option, data);
}
}


#endif //CS120_TCP_HPP
