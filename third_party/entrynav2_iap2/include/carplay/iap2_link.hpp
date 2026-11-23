#ifndef CARPLAY_IAP2_LINK_HPP
#define CARPLAY_IAP2_LINK_HPP

// Derived from DiPlay
// (shared/src/main/java/com/shilapi/xcertplay/transport/Iap2LinkEngine.kt,
//  shared/src/main/java/com/shilapi/xcertplay/transport/Iap2LinkChannel.kt),
// GPL-3.0, Copyright (c) DiPlay contributors.
//
// Transport-agnostic iAP2 Link Layer (detect, packet codec, SYN negotiation,
// reliable sequenced delivery with sliding window, retransmission, EAK, and RST).
//
// Pure sans-I/O state machine:
//   - Incoming transport bytes -> feed(data, len) -> LinkListener callbacks
//   - Outgoing session payloads -> sendPayload(session_id, data, len) -> ByteSink
//   - Timers (detect retry, SYN retry, data retransmission, cumulative ACK) -> tick(now_ms)
//
// No internal clock, sockets, or threads. Not thread-safe; callers serialize.

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

namespace carplay {
namespace iap2 {

// iAP2 detect sequence (6 bytes): FF 55 02 00 EE 10 (verified: DiPlay Iap2LinkEngine.kt:644)
static const size_t kDetectSequenceBytes = 6U;
extern const uint8_t kDetectSequence[kDetectSequenceBytes];

// Legacy/iAP1 fallback reject sequence (8 bytes): FF 55 04 00 02 04 EE 08 (unverified)
static const size_t kDetectBadSequenceBytes = 8U;
extern const uint8_t kDetectBadSequence[kDetectBadSequenceBytes];

// iAP2 Link packet constants (verified: DiPlay Iap2LinkEngine.kt:646-649, 653-654).
static const uint16_t kLinkStartMarker = 0xff5aU;
static const size_t kLinkHeaderBytes = 9U;
static const size_t kLinkPayloadChecksumBytes = 1U;
static const size_t kLinkPacketOverheadBytes = kLinkHeaderBytes + kLinkPayloadChecksumBytes;  // 10
static const size_t kLinkMinPacketWithPayloadBytes = kLinkPacketOverheadBytes + 1U;         // 11
static const size_t kLinkMaxPacketBytes = 0xffffU;

// Link control flags (byte 4 of header).
static const uint8_t kCtrlSyn = 0x80U;  // verified: DiPlay Iap2LinkEngine.kt:655
static const uint8_t kCtrlAck = 0x40U;  // verified: DiPlay Iap2LinkEngine.kt:656
static const uint8_t kCtrlEak = 0x20U;  // verified: DiPlay Iap2LinkEngine.kt:657
static const uint8_t kCtrlRst = 0x10U;  // verified: DiPlay Iap2LinkEngine.kt:658
static const uint8_t kCtrlSlp = 0x08U;  // unverified (not used in DiPlay)

// Link version & SYN session types (verified: DiPlay Iap2LinkEngine.kt:588-592, 650-652).
static const uint8_t kLinkVersion = 1U;
static const uint8_t kSessionTypeControl = 0U;
static const uint8_t kSessionTypeFileTransfer = 1U;
static const uint8_t kSessionTypeExternalAccessory = 2U;

static const size_t kSynFixedPayloadBytes = 10U;
static const size_t kSynSessionEntryBytes = 3U;

// Default SYN session IDs & versions (verified: DiPlay Iap2LinkEngine.kt:588-592,640-642; Iap2LinkChannel.kt:363,368).
static const uint8_t kDefaultControlSessionId = 10U;
static const uint8_t kDefaultEaSessionId = 11U;
static const uint8_t kDefaultFileTransferSessionId = 12U;
static const uint8_t kDefaultControlSessionVersion = 2U;
static const uint8_t kDefaultEaSessionVersion = 1U;
static const uint8_t kDefaultFileTransferSessionVersion = 2U;

/// Computes the 8-bit two's complement checksum: -(sum(data[0..len-1])) mod 256.
uint8_t computeChecksum(const uint8_t* data, size_t len);

/// Returns true iff len_with_checksum > 0 and sum(data[0..len_with_checksum-1]) mod 256 == 0.
bool verifyChecksum(const uint8_t* data, size_t len_with_checksum);

struct LinkPacket {
    uint8_t control;
    uint8_t seq;
    uint8_t ack;
    uint8_t session_id;
    std::vector<uint8_t> payload;

    LinkPacket() : control(0), seq(0), ack(0), session_id(0) {}

    bool encode(std::vector<uint8_t>& out, std::string* error_out = NULL) const;

    /// Decodes a complete single link packet (exact length match).
    static bool decode(const uint8_t* data,
                       size_t len,
                       LinkPacket& out,
                       std::string* error_out = NULL);
    static bool decode(const std::vector<uint8_t>& bytes,
                       LinkPacket& out,
                       std::string* error_out = NULL);
};

struct SynSession {
    uint8_t id;
    uint8_t type;     // kSessionTypeControl / FileTransfer / ExternalAccessory
    uint8_t version;

    SynSession() : id(0), type(kSessionTypeControl), version(1) {}
    SynSession(uint8_t session_id, uint8_t session_type, uint8_t session_version)
        : id(session_id), type(session_type), version(session_version) {}
};

struct SynPayload {
    uint8_t version;
    uint8_t max_outstanding;
    uint16_t max_packet_length;
    uint16_t retransmission_timeout_ms;
    uint16_t cumulative_ack_timeout_ms;
    uint8_t max_retransmissions;
    uint8_t max_cumulative_acks;
    std::vector<SynSession> sessions;

    // Defaults verified against DiPlay Iap2LinkEngine.kt:581-593, 662-664 & Iap2LinkChannel.kt:361-369.
    SynPayload()
        : version(kLinkVersion),
          max_outstanding(4),
          max_packet_length(kLinkMaxPacketBytes),
          retransmission_timeout_ms(4000U),
          cumulative_ack_timeout_ms(500U),
          max_retransmissions(4U),
          max_cumulative_acks(3U) {}

    bool encode(std::vector<uint8_t>& out, std::string* error_out = NULL) const;

    static bool decode(const uint8_t* data,
                       size_t len,
                       SynPayload& out,
                       std::string* error_out = NULL);
    static bool decode(const std::vector<uint8_t>& bytes,
                       SynPayload& out,
                       std::string* error_out = NULL);

    const SynSession* findSessionByType(uint8_t type) const;
    const SynSession* findSessionById(uint8_t id) const;
};

enum LinkState {
    kLinkIdle = 0,
    kLinkDetecting,
    kLinkSynSent,
    kLinkConnected,
    kLinkFailed
};

struct LinkConfig {
    SynPayload local_syn;
    uint8_t initial_seq;
    uint16_t detect_interval_ms;
    uint8_t max_detect_attempts;
    bool send_detect_on_start;
    bool send_syn_with_detect;   // True on wired USB & Wi-Fi tunnel (DiPlay wiredInitiator)
    bool echo_detect_if_unstarted;
    uint16_t syn_retry_interval_ms;  // 0 = fall back to local_syn.retransmission_timeout_ms
    uint8_t max_syn_retries;         // 0 = fall back to local_syn.max_retransmissions (when syn_retry_interval_ms == 0)
    size_t max_tx_queue;
    size_t max_rx_buffer_bytes;

    LinkConfig()
        : initial_seq(0),
          detect_interval_ms(1000),
          max_detect_attempts(5),
          send_detect_on_start(true),
          send_syn_with_detect(false),
          echo_detect_if_unstarted(true),
          syn_retry_interval_ms(0),
          max_syn_retries(0),
          max_tx_queue(64),
          max_rx_buffer_bytes(131072U) {
        local_syn.sessions.push_back(SynSession(
            kDefaultControlSessionId, kSessionTypeControl, kDefaultControlSessionVersion));
    }

    /// Wired USB CarPlay preset (verified: DiPlay Iap2LinkChannel.kt:361-377).
    /// Initiates SYN immediately alongside detect and uses zero-acknowledgement mode.
    static LinkConfig wiredUsb();

    /// Wireless Bluetooth RFCOMM preset (verified: DiPlay Iap2LinkChannel.kt:366-388).
    /// Waits for peer detect before sending SYN and keeps reliable ACKs/retransmissions enabled.
    static LinkConfig wirelessBluetooth();

    /// Wireless Wi-Fi AirPlay Type-130 tunnel preset (verified: DiPlay Iap2LinkChannel.kt:394-399).
    /// Initiates SYN alongside detect and uses zero-acknowledgement mode over reliable TCP/AirPlay.
    static LinkConfig wirelessTunnel();
};

struct LinkStats {
    uint64_t tx_packets;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t retransmissions;
    uint64_t bad_checksums;
    uint64_t resync_bytes_dropped;
    uint64_t duplicate_rx_packets;
    uint64_t out_of_order_rx_packets;
    uint64_t eaks_sent;
    uint64_t eaks_received;
    uint64_t resets;

    LinkStats()
        : tx_packets(0),
          tx_bytes(0),
          rx_packets(0),
          rx_bytes(0),
          retransmissions(0),
          bad_checksums(0),
          resync_bytes_dropped(0),
          duplicate_rx_packets(0),
          out_of_order_rx_packets(0),
          eaks_sent(0),
          eaks_received(0),
          resets(0) {}
};

/// Abstract outbound byte sink implemented by USB bulk / Bluetooth RFCOMM transports.
class ByteSink {
public:
    virtual ~ByteSink() {}
    virtual bool sendBytes(const uint8_t* data, size_t len) = 0;
};

class LinkListener {
public:
    virtual ~LinkListener() {}
    virtual void onLinkStateChanged(LinkState /*new_state*/) {}
    virtual void onLinkConnected(const SynPayload& /*negotiated*/) {}
    virtual void onLinkPayload(uint8_t /*session_id*/, const uint8_t* /*data*/, size_t /*len*/) {}
    virtual void onLinkSleep() {}
    virtual void onLinkReset(bool /*by_peer*/) {}
    virtual void onLinkError(const std::string& /*reason*/) {}
};

class Link {
public:
    Link(ByteSink& sink, LinkListener& listener, const LinkConfig& config = LinkConfig());

    /// Starts the link layer (enters kLinkDetecting and sends detect sequence if configured).
    void start();

    /// Resets all link queues/windows and returns to kLinkIdle (sending RST if active and send_rst).
    void reset(bool send_rst = true);

    /// Feeds incoming raw bytes from the transport. Handles detect, split/coalesced packets,
    /// garbage resynchronization, handshake, ACKs, EAKs, data delivery, SLP, and RST.
    /// Returns false only if data is NULL with len > 0 or rx buffer overflows.
    bool feed(const uint8_t* data, size_t len);
    bool feed(const std::vector<uint8_t>& bytes);

    /// Queues a session payload for reliable sequenced delivery.
    bool sendPayload(uint8_t session_id, const uint8_t* data, size_t len);
    bool sendPayload(uint8_t session_id, const std::vector<uint8_t>& bytes);

    /// Sends a Sleep (SLP) control packet when connected.
    bool sendSleep();

    /// Drives detect retries, SYN retries, data retransmissions, and cumulative ACKs.
    void tick(int64_t now_ms);

    LinkState state() const { return state_; }
    const LinkConfig& config() const { return config_; }
    const SynPayload& negotiatedSyn() const { return negotiated_syn_; }
    const LinkStats& stats() const { return stats_; }
    size_t maxPayloadBytes() const;
    size_t inFlightCount() const { return tx_inflight_.size(); }
    size_t pendingCount() const { return tx_pending_.size(); }

private:
    Link(const Link&) = delete;
    Link& operator=(const Link&) = delete;

    struct InFlightPacket {
        uint8_t seq;
        uint8_t session_id;
        std::vector<uint8_t> payload;
        int64_t last_tx_ms;
        bool has_tx_time;
        uint8_t retries;
        bool selectively_acked;

        InFlightPacket()
            : seq(0),
              session_id(0),
              last_tx_ms(0),
              has_tx_time(false),
              retries(0),
              selectively_acked(false) {}
    };

    struct PendingPayload {
        uint8_t session_id;
        std::vector<uint8_t> payload;
    };

    struct RxBufferedPacket {
        uint8_t seq;
        uint8_t session_id;
        std::vector<uint8_t> payload;
    };

    void setState(LinkState next);
    void clearTransferState();
    bool transmitRaw(const uint8_t* data, size_t len);
    bool sendControlPacket(uint8_t control,
                           uint8_t seq,
                           uint8_t ack,
                           uint8_t session_id,
                           const std::vector<uint8_t>* payload);
    bool sendDetect();
    bool sendSyn();
    bool sendAckOnly();
    bool sendEak();
    void drainTxPending();
    void handleDetect();
    void handlePacket(const LinkPacket& pkt);
    void handleSynAck(const LinkPacket& pkt);
    void handleSynOnly(const LinkPacket& pkt);
    void handleDataPacket(const LinkPacket& pkt);
    void processAck(uint8_t ack_num);
    void processEak(const LinkPacket& pkt);
    bool negotiateSyn(const SynPayload& peer, SynPayload& negotiated_out, std::string* error_out) const;
    void failLink(const std::string& reason, bool send_rst);
    void dropFrontRx(size_t count);

    ByteSink& sink_;
    LinkListener& listener_;
    LinkConfig config_;
    LinkState state_;
    SynPayload negotiated_syn_;
    LinkStats stats_;

    std::vector<uint8_t> rx_buffer_;
    bool has_time_;
    int64_t last_now_ms_;

    // Detect & SYN handshake tracking
    bool detect_sent_;
    int64_t detect_last_tx_ms_;
    bool has_detect_time_;
    uint8_t detect_attempts_;

    int64_t syn_last_tx_ms_;
    bool has_syn_time_;
    uint8_t syn_retries_;
    bool peer_syn_seen_;
    uint8_t peer_syn_seq_;
    bool passive_syn_ack_sent_;

    // TX sliding window
    uint8_t tx_unacked_seq_;
    uint8_t tx_next_seq_;
    uint8_t last_tx_seq_;
    std::deque<InFlightPacket> tx_inflight_;
    std::deque<PendingPayload> tx_pending_;

    // RX sliding window & cumulative ACK
    uint8_t rx_expected_seq_;
    uint8_t last_rx_in_order_;
    std::vector<RxBufferedPacket> rx_reorder_;
    uint16_t unacked_rx_count_;
    bool cum_ack_pending_;
    int64_t cum_ack_since_ms_;
    bool has_cum_ack_time_;
};

}  // namespace iap2
}  // namespace carplay

#endif  // CARPLAY_IAP2_LINK_HPP
