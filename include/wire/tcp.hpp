#ifndef CS120_TCP_HPP
#define CS120_TCP_HPP


#include <cstdint>


namespace cs120 {
struct TCPHeader {
private:
    uint16_t source_port;
    uint16_t destination_port;
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

#define TH_FLAGS        (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
#define TH_ACCEPT       (TH_FIN|TH_SYN|TH_RST|TH_ACK)

    uint16_t get_source_port() const { return ntohs(source_port); }

    uint16_t get_destination_port() const { return ntohs(destination_port); }

    uint32_t get_sequence() const { return ntohl(sequence); }

    uint32_t get_ack_number() const { return ntohl(ack_number); }

    bool get_nonce_sum() const { return (reserve & NONCE_SUM) > 0; }

    bool get_cwr() const { return (flags & FLAGS_CWR) > 0; }

    bool get_ece() const { return (flags & FLAGS_ECE) > 0; }

    bool get_urgent() const { return (flags & FLAGS_URGENT) > 0; }

    bool get_ack() const { return (flags & FLAGS_ACK) > 0; }

    bool get_push() const { return (flags & FLAGS_PUSH) > 0; }

    bool get_reset() const { return (flags & FLAGS_RESET) > 0; }

    bool get_sync() const { return (flags & FLAGS_SYNC) > 0; }

    bool get_fin() const { return (flags & FLAGS_FIN) > 0; }

    uint16_t get_window() const { return ntohs(window); }

    uint16_t get_checksum() const { return checksum; }

    uint16_t get_urgent_ptr() const { return ntohs(urgent_ptr); }

    void format() const {
        printf("TCP Header {\n");
        printf("\tsource port: %d,\n", get_source_port());
        printf("\tdestination port: %d,\n", get_destination_port());
        printf("\tsequence: %d,\n", get_sequence());
        printf("\tack number: %d,\n", get_ack_number());
        printf("\tnonce sum: %s,\n", bool_to_string(get_nonce_sum()));
        printf("\tcwr: %s,\n", bool_to_string(get_cwr()));
        printf("\tece: %s,\n", bool_to_string(get_ece()));
        printf("\turgent: %s,\n", bool_to_string(get_urgent()));
        printf("\tack: %s,\n", bool_to_string(get_ack()));
        printf("\tpush: %s,\n", bool_to_string(get_push()));
        printf("\treset: %s,\n", bool_to_string(get_reset()));
        printf("\tsync: %s,\n", bool_to_string(get_sync()));
        printf("\tfin: %s,\n", bool_to_string(get_fin()));
        printf("\twindow: %d,\n", get_window());
//    printf("\tchecksum: %d,\n", get_check());
        printf("\turgent ptr: %d,\n", get_urgent_ptr());
        printf("}\n");
    }
};
}


#endif //CS120_TCP_HPP
