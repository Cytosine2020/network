#ifndef CS120_IPV4_SERVER_HPP
#define CS120_IPV4_SERVER_HPP


#include <type_traits>
#include <unordered_map>
#include <map>

#include "queue.hpp"
#include "wire/ipv4.hpp"


namespace cs120 {
struct IPV4FragmentTag {
    uint32_t src_ip, dest_ip;
    uint16_t identification;

    explicit IPV4FragmentTag(const IPV4Header &header) :
            src_ip{header.get_src_ip()}, dest_ip{header.get_dest_ip()},
            identification{header.get_identification()} {}

    bool operator==(const IPV4FragmentTag &other) const {
        return this->src_ip == other.src_ip &&
               this->dest_ip == other.dest_ip &&
               this->identification == other.identification;
    }
};
}


template<>
struct std::hash<cs120::IPV4FragmentTag> {
    size_t operator()(const cs120::IPV4FragmentTag &object) const {
        return std::hash<uint64_t>{}(static_cast<uint64_t>(object.src_ip) |
                                     (static_cast<uint64_t>(object.dest_ip) << 32)) ^
               std::hash<uint16_t>{}(object.identification);
    }
};


namespace cs120 {
// todo: handle ip option

template<typename T>
class IPV4FragmentSender {
    static_assert(std::is_same<typename T::Item, uint8_t>::value);

private:
    typename MPSCQueue<T>::Sender inner;
    size_t mtu;

public:
    IPV4FragmentSender() noexcept: inner{}, mtu{0} {}

    IPV4FragmentSender(typename MPSCQueue<T>::Sender &&inner, uint16_t mtu) :
            inner{std::move(inner)}, mtu{IPV4Header::max_payload(mtu)} {}

    typename MPSCQueue<T>::Sender &operator*() { return inner; }

    typename MPSCQueue<T>::Sender *operator->() { return &inner; }

    size_t send(uint8_t type_of_service, uint16_t identification, IPV4Protocol protocol,
                uint32_t src_ip, uint32_t dest_ip,
                size_t offset, bool do_not_fragment, bool more_fragment,
                uint8_t time_to_live, Slice<uint8_t> data) {
        for (size_t start = 0, size; start < data.size(); start += size) {
            size = std::min(mtu, data.size() - start);

            bool fragment = more_fragment || start + size < data.size();

            auto buffer = inner.try_send();
            if (buffer.none()) { return sizeof(IPV4Header) + start; }

            IPV4Header::generate((*buffer)[Range{}], type_of_service, identification, protocol,
                                 src_ip, dest_ip, offset + start, do_not_fragment, fragment,
                                 time_to_live, size)
                    .copy_from_slice(data[Range{start}][Range{0, size}]);
        }

        return sizeof(IPV4Header) + data.size();
    }

    size_t send(Slice<uint8_t> data) {
        auto[ip_header, ip_option, ip_data] = ipv4_split(data);
        if (ip_header == nullptr) { cs120_abort("invalid package!"); }

        return send(ip_header->get_type_of_service(), ip_header->get_identification(),
                    ip_header->get_protocol(), ip_header->get_src_ip(), ip_header->get_dest_ip(),
                    ip_header->get_fragment_offset(), ip_header->get_do_not_fragment(),
                    ip_header->get_more_fragment(), ip_header->get_time_to_live(), ip_data);
    }

    ~IPV4FragmentSender() = default;
};


class IPV4FragmentReceiver {
public:
    class ReceiverSlotGuard {
    private:
        Array<uint8_t> buffer;
        Slice<uint8_t> data;

    public:
        ReceiverSlotGuard() noexcept: buffer{}, data{} {}

        explicit ReceiverSlotGuard(Slice<uint8_t> data) : buffer{}, data{data} {}

        explicit ReceiverSlotGuard(Array<uint8_t> &&buffer) :
                buffer{std::move(buffer)}, data{this->buffer[Range{}]} {}

        Slice<uint8_t> &operator*() { return data; }

        Slice<uint8_t> *operator->() { return &data; }

        bool none() { return data.empty(); }
    };

private:
    class Fragment {
    public:
        enum class InsertResult {
            None,
            Complete,
            Error,
        };

    private:
        size_t size;
        Array<uint8_t> buffer;
        std::map<uint16_t, uint16_t> fragments;

    public:
        explicit Fragment(const IPV4Header &header) : size{0}, buffer{2048}, fragments{} {
            buffer[Range{0, sizeof(IPV4Header)}].copy_from_slice(header.into_slice());
        }

        InsertResult insert(const IPV4Header *ip_header, Slice<uint8_t> ip_option,
                            Slice<uint8_t> ip_data) {
            (void) ip_option;

            auto *header = reinterpret_cast<IPV4Header *>(buffer.begin());

            size_t offset = ip_header->get_fragment_offset();

            if (!ip_header->get_more_fragment()) {
                if (size == 0) {
                    size = offset + ip_data.size();
                } else if (size != offset + ip_data.size()) {
                    return InsertResult::Error;
                }
            }

            if (offset + ip_data.size() + sizeof(IPV4Header) > buffer.size() ||
                (size != 0 && size < offset + ip_data.size()) ||
                header->get_protocol() != ip_header->get_protocol()) {
                return InsertResult::Error;
            }

            if (header->get_time_to_live() > ip_header->get_time_to_live()) {
                header->set_time_to_live(ip_header->get_time_to_live());
            }

            size_t new_offset = offset, new_size = ip_data.size();

            auto after = fragments.upper_bound(offset + ip_data.size());
            if (after != fragments.begin()) {
                auto tmp = after;
                --tmp;
                if (tmp->first + tmp->second > offset + ip_data.size()) {
                    new_size = tmp->first + tmp->second - offset;
                }
            }

            auto before = fragments.upper_bound(offset);
            if (before != fragments.begin()) {
                auto tmp = before;
                --tmp;
                if (tmp->first + tmp->second >= offset) {
                    new_size += new_offset - tmp->first;
                    new_offset = tmp->first;
                    --before;
                }
            }

            fragments.erase(before, after);
            fragments.emplace(new_offset, new_size);

            buffer[Range{sizeof(IPV4Header) + offset}][Range{0, ip_data.size()}]
                    .copy_from_slice(ip_data);

            if (size != 0 && fragments.size() == 1) {
                auto ptr = fragments.begin();
                if (ptr->first == 0 && ptr->second == size) {
                    return InsertResult::Complete;
                }
            }

            return InsertResult::None;
        }

        Array<uint8_t> take() {
            auto *header = reinterpret_cast<IPV4Header *>(buffer.begin());

            header->set_header_length(sizeof(IPV4Header));
            header->set_total_length(sizeof(IPV4Header) + size);
            header->set_fragment(0, false, false);
            header->set_checksum(0);
            header->set_checksum(complement_checksum(header->into_slice()));

            return std::move(buffer);
        }
    };

    std::unordered_map<IPV4FragmentTag, Fragment> fragments;

public:
    IPV4FragmentReceiver() noexcept: fragments{} {}

    ReceiverSlotGuard recv(Slice<uint8_t> buffer) {
        auto[ip_header, ip_option, ip_data] = ipv4_split(buffer);
        if (ip_header == nullptr || complement_checksum(ip_header->into_slice()) != 0) {
            return ReceiverSlotGuard{};
        }

        if (ip_header->get_fragment_offset() == 0 && !ip_header->get_more_fragment()) {
            return ReceiverSlotGuard{buffer};
        }

        IPV4FragmentTag tag{*ip_header};

        auto ptr = fragments.find(tag);
        if (ptr == fragments.end()) {
            ptr = fragments.emplace(tag, Fragment{*ip_header}).first;
        }

        switch (ptr->second.insert(ip_header, ip_option, ip_data)) {
            case Fragment::InsertResult::None:
                break;
            case Fragment::InsertResult::Complete: {
                auto data = ptr->second.take();
                fragments.erase(ptr);
                return ReceiverSlotGuard{std::move(data)};
            }
            case Fragment::InsertResult::Error:
                fragments.erase(ptr);
                break;
            default:
                cs120_unreachable("");
        }

        return ReceiverSlotGuard{};
    }

    ~IPV4FragmentReceiver() = default;
};
}


#endif //CS120_IPV4_SERVER_HPP
