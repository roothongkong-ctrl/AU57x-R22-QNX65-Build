// Derived from DiPlay
// (shared/src/main/java/com/shilapi/xcertplay/transport/Iap2LinkEngine.kt,
//  shared/src/main/java/com/shilapi/xcertplay/transport/Iap2LinkChannel.kt),
// GPL-3.0, Copyright (c) DiPlay contributors.

#include "carplay/iap2_link.hpp"

#include <cstring>
#include <sstream>

namespace carplay {
namespace iap2 {

const uint8_t kDetectSequence[kDetectSequenceBytes] = {
    0xffU, 0x55U, 0x02U, 0x00U, 0xeeU, 0x10U
};

const uint8_t kDetectBadSequence[kDetectBadSequenceBytes] = {
    0xffU, 0x55U, 0x04U, 0x00U, 0x02U, 0x04U, 0xeeU, 0x08U
};

namespace {

void setError(std::string* err, const std::string& msg) {
    if (err != NULL) {
        *err = msg;
    }
}

uint16_t readU16Be(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

void writeU16Be(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xffU));
    out.push_back(static_cast<uint8_t>(v & 0xffU));
}

bool isValidSessionType(uint8_t type) {
    return type == kSessionTypeControl ||
           type == kSessionTypeFileTransfer ||
           type == kSessionTypeExternalAccessory;
}

}  // namespace

uint8_t computeChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    if (data != NULL) {
        for (size_t i = 0; i < len; ++i) {
            sum = static_cast<uint8_t>(sum + data[i]);
        }
    }
    return static_cast<uint8_t>(0U - sum);
}

bool verifyChecksum(const uint8_t* data, size_t len_with_checksum) {
    if (data == NULL || len_with_checksum == 0U) {
        return false;
    }
    uint8_t sum = 0;
    for (size_t i = 0; i < len_with_checksum; ++i) {
        sum = static_cast<uint8_t>(sum + data[i]);
    }
    return sum == 0U;
}

bool LinkPacket::encode(std::vector<uint8_t>& out, std::string* error_out) const {
    const size_t total_len = payload.empty()
        ? kLinkHeaderBytes
        : (kLinkPacketOverheadBytes + payload.size());
    if (total_len > kLinkMaxPacketBytes) {
        setError(error_out, "iAP2 link packet exceeds 65535 bytes");
        out.clear();
        return false;
    }

    out.clear();
    out.reserve(total_len);
    writeU16Be(out, kLinkStartMarker);
    writeU16Be(out, static_cast<uint16_t>(total_len));
    out.push_back(control);
    out.push_back(seq);
    out.push_back(ack);
    out.push_back(session_id);
    out.push_back(computeChecksum(&out[0], kLinkHeaderBytes - 1U));

    if (!payload.empty()) {
        out.insert(out.end(), payload.begin(), payload.end());
        out.push_back(computeChecksum(&payload[0], payload.size()));
    }
    return true;
}

bool LinkPacket::decode(const uint8_t* data,
                        size_t len,
                        LinkPacket& out,
                        std::string* error_out) {
    if (data == NULL || len < kLinkHeaderBytes) {
        setError(error_out, "truncated iAP2 link header");
        return false;
    }
    if (readU16Be(data) != kLinkStartMarker) {
        setError(error_out, "invalid iAP2 link start marker (expected 0xFF5A)");
        return false;
    }
    if (!verifyChecksum(data, kLinkHeaderBytes)) {
        setError(error_out, "iAP2 link header checksum mismatch");
        return false;
    }
    const uint16_t pkt_len = readU16Be(data + 2);
    if (pkt_len != len) {
        setError(error_out, "iAP2 link packet length mismatch");
        return false;
    }
    if (pkt_len != kLinkHeaderBytes && pkt_len < kLinkMinPacketWithPayloadBytes) {
        setError(error_out, "invalid iAP2 link packet length");
        return false;
    }

    out.control = data[4];
    out.seq = data[5];
    out.ack = data[6];
    out.session_id = data[7];
    out.payload.clear();

    if (pkt_len > kLinkHeaderBytes) {
        const size_t body_with_csum = static_cast<size_t>(pkt_len) - kLinkHeaderBytes;
        if (!verifyChecksum(data + kLinkHeaderBytes, body_with_csum)) {
            setError(error_out, "iAP2 link payload checksum mismatch");
            out.payload.clear();
            return false;
        }
        const size_t payload_len = body_with_csum - kLinkPayloadChecksumBytes;
        out.payload.assign(data + kLinkHeaderBytes, data + kLinkHeaderBytes + payload_len);
    }
    return true;
}

bool LinkPacket::decode(const std::vector<uint8_t>& bytes,
                        LinkPacket& out,
                        std::string* error_out) {
    const uint8_t* ptr = bytes.empty() ? NULL : &bytes[0];
    return decode(ptr, bytes.size(), out, error_out);
}

bool SynPayload::encode(std::vector<uint8_t>& out, std::string* error_out) const {
    const size_t total = kSynFixedPayloadBytes + sessions.size() * kSynSessionEntryBytes;
    if (kLinkPacketOverheadBytes + total > kLinkMaxPacketBytes) {
        setError(error_out, "SYN payload exceeds maximum link packet length");
        out.clear();
        return false;
    }
    out.clear();
    out.reserve(total);
    out.push_back(version);
    out.push_back(max_outstanding);
    writeU16Be(out, max_packet_length);
    writeU16Be(out, retransmission_timeout_ms);
    writeU16Be(out, cumulative_ack_timeout_ms);
    out.push_back(max_retransmissions);
    out.push_back(max_cumulative_acks);
    for (size_t i = 0; i < sessions.size(); ++i) {
        out.push_back(sessions[i].id);
        out.push_back(sessions[i].type);
        out.push_back(sessions[i].version);
    }
    return true;
}

bool SynPayload::decode(const uint8_t* data,
                        size_t len,
                        SynPayload& out,
                        std::string* error_out) {
    if (data == NULL || len < kSynFixedPayloadBytes) {
        setError(error_out, "SYN payload shorter than 10 bytes");
        return false;
    }
    const size_t rem = len - kSynFixedPayloadBytes;
    if ((rem % kSynSessionEntryBytes) != 0U) {
        setError(error_out, "SYN session table not a multiple of 3 bytes");
        return false;
    }
    out.version = data[0];
    out.max_outstanding = data[1];
    out.max_packet_length = readU16Be(data + 2);
    out.retransmission_timeout_ms = readU16Be(data + 4);
    out.cumulative_ack_timeout_ms = readU16Be(data + 6);
    out.max_retransmissions = data[8];
    out.max_cumulative_acks = data[9];
    out.sessions.clear();
    const size_t count = rem / kSynSessionEntryBytes;
    out.sessions.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const size_t off = kSynFixedPayloadBytes + i * kSynSessionEntryBytes;
        out.sessions.push_back(SynSession(data[off], data[off + 1U], data[off + 2U]));
    }
    return true;
}

bool SynPayload::decode(const std::vector<uint8_t>& bytes,
                        SynPayload& out,
                        std::string* error_out) {
    const uint8_t* ptr = bytes.empty() ? NULL : &bytes[0];
    return decode(ptr, bytes.size(), out, error_out);
}

const SynSession* SynPayload::findSessionByType(uint8_t type) const {
    for (size_t i = 0; i < sessions.size(); ++i) {
        if (sessions[i].type == type) {
            return &sessions[i];
        }
    }
    return NULL;
}

const SynSession* SynPayload::findSessionById(uint8_t id) const {
    for (size_t i = 0; i < sessions.size(); ++i) {
        if (sessions[i].id == id) {
            return &sessions[i];
        }
    }
    return NULL;
}

LinkConfig LinkConfig::wiredUsb() {
    LinkConfig cfg;
    cfg.initial_seq = 99U;  // DiPlay Iap2LinkEngine.kt:659
    cfg.detect_interval_ms = 1000U;
    cfg.max_detect_attempts = 5U;
    cfg.send_detect_on_start = true;
    cfg.send_syn_with_detect = true;
    cfg.echo_detect_if_unstarted = true;
    cfg.syn_retry_interval_ms = 500U;  // DiPlay Iap2LinkEngine.kt:661
    cfg.max_syn_retries = 10U;
    cfg.local_syn.version = kLinkVersion;
    cfg.local_syn.max_outstanding = 4U;
    cfg.local_syn.max_packet_length = kLinkMaxPacketBytes;
    cfg.local_syn.retransmission_timeout_ms = 0U;   // zeroAcknowledgements (Iap2LinkChannel.kt:364)
    cfg.local_syn.cumulative_ack_timeout_ms = 0U;
    cfg.local_syn.max_retransmissions = 0U;
    cfg.local_syn.max_cumulative_acks = 0U;
    cfg.local_syn.sessions.clear();
    cfg.local_syn.sessions.push_back(SynSession(
        kDefaultControlSessionId, kSessionTypeControl, kDefaultControlSessionVersion));
    cfg.local_syn.sessions.push_back(SynSession(
        kDefaultEaSessionId, kSessionTypeExternalAccessory, kDefaultEaSessionVersion));
    cfg.local_syn.sessions.push_back(SynSession(
        kDefaultFileTransferSessionId, kSessionTypeFileTransfer, kDefaultFileTransferSessionVersion));
    return cfg;
}

LinkConfig LinkConfig::wirelessBluetooth() {
    LinkConfig cfg;
    cfg.initial_seq = 99U;
    cfg.detect_interval_ms = 1000U;
    cfg.max_detect_attempts = 5U;
    cfg.send_detect_on_start = true;
    cfg.send_syn_with_detect = false;  // Waits for peer detect (Iap2LinkChannel.kt:380-388)
    cfg.echo_detect_if_unstarted = true;
    cfg.syn_retry_interval_ms = 500U;
    cfg.max_syn_retries = 10U;
    cfg.local_syn.version = kLinkVersion;
    cfg.local_syn.max_outstanding = 4U;
    cfg.local_syn.max_packet_length = kLinkMaxPacketBytes;
    cfg.local_syn.retransmission_timeout_ms = 4000U;
    cfg.local_syn.cumulative_ack_timeout_ms = 500U;
    cfg.local_syn.max_retransmissions = 4U;
    cfg.local_syn.max_cumulative_acks = 3U;
    cfg.local_syn.sessions.clear();
    cfg.local_syn.sessions.push_back(SynSession(
        kDefaultControlSessionId, kSessionTypeControl, kDefaultControlSessionVersion));
    cfg.local_syn.sessions.push_back(SynSession(
        kDefaultEaSessionId, kSessionTypeExternalAccessory, kDefaultEaSessionVersion));
    cfg.local_syn.sessions.push_back(SynSession(
        kDefaultFileTransferSessionId, kSessionTypeFileTransfer, kDefaultFileTransferSessionVersion));
    return cfg;
}

LinkConfig LinkConfig::wirelessTunnel() {
    // Type-130 AirPlay tunnel matches wired initiator + zero-ack (Iap2LinkChannel.kt:394-399).
    return wiredUsb();
}

Link::Link(ByteSink& sink, LinkListener& listener, const LinkConfig& config)
    : sink_(sink),
      listener_(listener),
      config_(config),
      state_(kLinkIdle),
      negotiated_syn_(config.local_syn),
      has_time_(false),
      last_now_ms_(0),
      detect_sent_(false),
      detect_last_tx_ms_(0),
      has_detect_time_(false),
      detect_attempts_(0),
      syn_last_tx_ms_(0),
      has_syn_time_(false),
      syn_retries_(0),
      peer_syn_seen_(false),
      peer_syn_seq_(0),
      passive_syn_ack_sent_(false),
      tx_unacked_seq_(config.initial_seq),
      tx_next_seq_(config.initial_seq),
      last_tx_seq_(config.initial_seq),
      rx_expected_seq_(0),
      last_rx_in_order_(0),
      unacked_rx_count_(0),
      cum_ack_pending_(false),
      cum_ack_since_ms_(0),
      has_cum_ack_time_(false) {}

void Link::setState(LinkState next) {
    if (state_ != next) {
        state_ = next;
        listener_.onLinkStateChanged(next);
    }
}

void Link::clearTransferState() {
    detect_sent_ = false;
    detect_last_tx_ms_ = 0;
    has_detect_time_ = false;
    detect_attempts_ = 0;
    syn_last_tx_ms_ = 0;
    has_syn_time_ = false;
    syn_retries_ = 0;
    peer_syn_seen_ = false;
    peer_syn_seq_ = 0;
    passive_syn_ack_sent_ = false;
    tx_unacked_seq_ = config_.initial_seq;
    tx_next_seq_ = config_.initial_seq;
    last_tx_seq_ = config_.initial_seq;
    tx_inflight_.clear();
    tx_pending_.clear();
    rx_expected_seq_ = 0;
    last_rx_in_order_ = 0;
    rx_reorder_.clear();
    unacked_rx_count_ = 0;
    cum_ack_pending_ = false;
    cum_ack_since_ms_ = 0;
    has_cum_ack_time_ = false;
}

void Link::start() {
    clearTransferState();
    rx_buffer_.clear();
    negotiated_syn_ = config_.local_syn;
    setState(kLinkDetecting);
    if (config_.send_detect_on_start) {
        sendDetect();
    }
    if (config_.send_syn_with_detect && state_ == kLinkDetecting) {
        setState(kLinkSynSent);
        sendSyn();
    }
}

void Link::reset(bool send_rst) {
    const bool was_active = (state_ == kLinkSynSent || state_ == kLinkConnected);
    if (send_rst && was_active) {
        sendControlPacket(kCtrlRst, last_tx_seq_, 0, 0, NULL);
        ++stats_.resets;
    }
    clearTransferState();
    rx_buffer_.clear();
    negotiated_syn_ = config_.local_syn;
    setState(kLinkIdle);
}

size_t Link::maxPayloadBytes() const {
    const uint16_t max_pkt = (state_ == kLinkConnected)
        ? negotiated_syn_.max_packet_length
        : config_.local_syn.max_packet_length;
    if (max_pkt <= kLinkPacketOverheadBytes) {
        return 0U;
    }
    return static_cast<size_t>(max_pkt) - kLinkPacketOverheadBytes;
}

bool Link::transmitRaw(const uint8_t* data, size_t len) {
    if (len == 0U) {
        return true;
    }
    ++stats_.tx_packets;
    stats_.tx_bytes += len;
    if (!sink_.sendBytes(data, len)) {
        listener_.onLinkError("ByteSink::sendBytes failed");
        return false;
    }
    return true;
}

bool Link::sendControlPacket(uint8_t control,
                             uint8_t seq,
                             uint8_t ack,
                             uint8_t session_id,
                             const std::vector<uint8_t>* payload) {
    LinkPacket pkt;
    pkt.control = control;
    pkt.seq = seq;
    pkt.ack = ack;
    pkt.session_id = session_id;
    if (payload != NULL) {
        pkt.payload = *payload;
    }
    std::vector<uint8_t> wire;
    std::string err;
    if (!pkt.encode(wire, &err)) {
        listener_.onLinkError(err);
        return false;
    }
    return transmitRaw(&wire[0], wire.size());
}

bool Link::sendDetect() {
    detect_sent_ = true;
    ++detect_attempts_;
    detect_last_tx_ms_ = last_now_ms_;
    has_detect_time_ = has_time_;
    return transmitRaw(kDetectSequence, kDetectSequenceBytes);
}

bool Link::sendSyn() {
    std::vector<uint8_t> syn_bytes;
    std::string err;
    if (!config_.local_syn.encode(syn_bytes, &err)) {
        listener_.onLinkError(err);
        return false;
    }
    last_tx_seq_ = config_.initial_seq;
    tx_unacked_seq_ = config_.initial_seq;
    tx_next_seq_ = static_cast<uint8_t>(config_.initial_seq + 1U);
    syn_last_tx_ms_ = last_now_ms_;
    has_syn_time_ = has_time_;
    return sendControlPacket(kCtrlSyn, config_.initial_seq, 0, 0, &syn_bytes);
}

bool Link::sendAckOnly() {
    return sendControlPacket(kCtrlAck, last_tx_seq_, last_rx_in_order_, 0, NULL);
}

bool Link::sendEak() {
    std::vector<uint8_t> eak_seqs;
    eak_seqs.reserve(rx_reorder_.size());
    for (size_t i = 0; i < rx_reorder_.size(); ++i) {
        eak_seqs.push_back(rx_reorder_[i].seq);
    }
    ++stats_.eaks_sent;
    return sendControlPacket(static_cast<uint8_t>(kCtrlAck | kCtrlEak),
                             last_tx_seq_, last_rx_in_order_, 0, &eak_seqs);
}

bool Link::sendSleep() {
    if (state_ != kLinkConnected) {
        return false;
    }
    return sendControlPacket(kCtrlSlp, last_tx_seq_, last_rx_in_order_, 0, NULL);
}

bool Link::sendPayload(uint8_t session_id, const uint8_t* data, size_t len) {
    if (state_ != kLinkConnected) {
        return false;
    }
    if (session_id == 0U || len == 0U || data == NULL) {
        return false;
    }
    if (!negotiated_syn_.sessions.empty() &&
        negotiated_syn_.findSessionById(session_id) == NULL) {
        return false;
    }
    const size_t max_pl = maxPayloadBytes();
    if (len > max_pl) {
        return false;
    }
    if (tx_pending_.size() >= config_.max_tx_queue) {
        return false;
    }

    PendingPayload pending;
    pending.session_id = session_id;
    pending.payload.assign(data, data + len);
    tx_pending_.push_back(pending);
    drainTxPending();
    return true;
}

bool Link::sendPayload(uint8_t session_id, const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        return false;
    }
    return sendPayload(session_id, &bytes[0], bytes.size());
}

void Link::drainTxPending() {
    if (state_ != kLinkConnected) {
        return;
    }
    const size_t window = negotiated_syn_.max_outstanding > 0U
        ? static_cast<size_t>(negotiated_syn_.max_outstanding)
        : 1U;
    const bool zero_retx = (negotiated_syn_.max_retransmissions == 0U &&
                            negotiated_syn_.retransmission_timeout_ms == 0U);
    while (!tx_pending_.empty() && (zero_retx || tx_inflight_.size() < window)) {
        const PendingPayload& front = tx_pending_.front();
        const uint8_t seq = tx_next_seq_;
        const uint8_t session_id = front.session_id;
        const std::vector<uint8_t> payload = front.payload;

        last_tx_seq_ = seq;
        tx_next_seq_ = static_cast<uint8_t>(tx_next_seq_ + 1U);

        // Outbound data carries ACK = last_rx_in_order_, satisfying any pending cumulative ACK.
        unacked_rx_count_ = 0;
        cum_ack_pending_ = false;
        has_cum_ack_time_ = false;

        if (zero_retx) {
            tx_unacked_seq_ = tx_next_seq_;
            tx_pending_.pop_front();
            sendControlPacket(kCtrlAck, seq, last_rx_in_order_, session_id, &payload);
        } else {
            InFlightPacket inflight;
            inflight.seq = seq;
            inflight.session_id = session_id;
            inflight.payload = payload;
            inflight.last_tx_ms = last_now_ms_;
            inflight.has_tx_time = has_time_;
            inflight.retries = 0;
            inflight.selectively_acked = false;

            tx_inflight_.push_back(inflight);
            tx_pending_.pop_front();

            const InFlightPacket& sent = tx_inflight_.back();
            sendControlPacket(kCtrlAck, sent.seq, last_rx_in_order_, sent.session_id, &sent.payload);
        }
    }
}

void Link::dropFrontRx(size_t count) {
    if (count >= rx_buffer_.size()) {
        rx_buffer_.clear();
    } else if (count > 0U) {
        rx_buffer_.erase(rx_buffer_.begin(),
                         rx_buffer_.begin() + static_cast<ptrdiff_t>(count));
    }
}

bool Link::feed(const uint8_t* data, size_t len) {
    if (len > 0U && data == NULL) {
        return false;
    }
    if (len > 0U) {
        if (rx_buffer_.size() + len > config_.max_rx_buffer_bytes) {
            rx_buffer_.clear();
            listener_.onLinkError("iAP2 link rx buffer overflow");
            return false;
        }
        rx_buffer_.insert(rx_buffer_.end(), data, data + len);
        stats_.rx_bytes += len;
    }

    while (!rx_buffer_.empty()) {
        // Resynchronise to the next 0xFF sync byte.
        size_t sync_pos = 0;
        while (sync_pos < rx_buffer_.size() && rx_buffer_[sync_pos] != 0xffU) {
            ++sync_pos;
        }
        if (sync_pos > 0U) {
            stats_.resync_bytes_dropped += sync_pos;
            dropFrontRx(sync_pos);
            if (rx_buffer_.empty()) {
                break;
            }
        }

        if (rx_buffer_.size() < 2U) {
            break;
        }

        const uint8_t second = rx_buffer_[1];
        if (second == 0x55U) {
            if (rx_buffer_.size() < kDetectSequenceBytes) {
                break;
            }
            if (std::memcmp(&rx_buffer_[0], kDetectSequence, kDetectSequenceBytes) == 0) {
                dropFrontRx(kDetectSequenceBytes);
                ++stats_.rx_packets;
                handleDetect();
                continue;
            }
            if (rx_buffer_[2] == 0x04U) {
                if (rx_buffer_.size() < kDetectBadSequenceBytes) {
                    break;
                }
                if (std::memcmp(&rx_buffer_[0], kDetectBadSequence, kDetectBadSequenceBytes) == 0) {
                    dropFrontRx(kDetectBadSequenceBytes);
                    ++stats_.rx_packets;
                    failLink("peer sent iAP1/bad detect sequence", false);
                    continue;
                }
            }
            ++stats_.resync_bytes_dropped;
            dropFrontRx(1U);
            continue;
        }

        if (second == 0x5aU) {
            if (rx_buffer_.size() < kLinkHeaderBytes) {
                break;
            }
            if (!verifyChecksum(&rx_buffer_[0], kLinkHeaderBytes)) {
                ++stats_.bad_checksums;
                ++stats_.resync_bytes_dropped;
                dropFrontRx(1U);
                continue;
            }
            const uint16_t pkt_len = readU16Be(&rx_buffer_[2]);
            if (pkt_len < kLinkHeaderBytes ||
                pkt_len == kLinkPacketOverheadBytes ||
                pkt_len > config_.max_rx_buffer_bytes) {
                ++stats_.resync_bytes_dropped;
                dropFrontRx(1U);
                continue;
            }
            if (rx_buffer_.size() < pkt_len) {
                break;
            }
            if (pkt_len > kLinkHeaderBytes &&
                !verifyChecksum(&rx_buffer_[kLinkHeaderBytes],
                                static_cast<size_t>(pkt_len) - kLinkHeaderBytes)) {
                ++stats_.bad_checksums;
                ++stats_.resync_bytes_dropped;
                dropFrontRx(1U);
                continue;
            }
            if (state_ == kLinkConnected && pkt_len > config_.local_syn.max_packet_length) {
                dropFrontRx(pkt_len);
                listener_.onLinkError("received link packet exceeding max_packet_length");
                continue;
            }
            LinkPacket pkt;
            std::string err;
            if (!LinkPacket::decode(&rx_buffer_[0], pkt_len, pkt, &err)) {
                ++stats_.bad_checksums;
                ++stats_.resync_bytes_dropped;
                dropFrontRx(1U);
                continue;
            }
            dropFrontRx(pkt_len);
            ++stats_.rx_packets;
            handlePacket(pkt);
            continue;
        }

        // Not 0xFF 0x55 or 0xFF 0x5A: drop leading 0xFF and resync.
        ++stats_.resync_bytes_dropped;
        dropFrontRx(1U);
    }

    return true;
}

bool Link::feed(const std::vector<uint8_t>& bytes) {
    const uint8_t* ptr = bytes.empty() ? NULL : &bytes[0];
    return feed(ptr, bytes.size());
}

void Link::handleDetect() {
    if (state_ == kLinkIdle) {
        if (!config_.echo_detect_if_unstarted) {
            return;
        }
        clearTransferState();
        negotiated_syn_ = config_.local_syn;
        setState(kLinkDetecting);
        sendDetect();
        setState(kLinkSynSent);
        sendSyn();
        return;
    }
    if (state_ == kLinkDetecting) {
        if (!detect_sent_) {
            sendDetect();
        }
        setState(kLinkSynSent);
        sendSyn();
        return;
    }
    if (state_ == kLinkSynSent) {
        // Peer re-sent detect while waiting for SYN|ACK: re-send SYN.
        sendSyn();
        return;
    }
    if (state_ == kLinkConnected) {
        // Peer restarted link from scratch via detect sequence.
        clearTransferState();
        negotiated_syn_ = config_.local_syn;
        listener_.onLinkReset(true);
        setState(kLinkDetecting);
        sendDetect();
        setState(kLinkSynSent);
        sendSyn();
    }
}

void Link::handlePacket(const LinkPacket& pkt) {
    if ((pkt.control & kCtrlRst) != 0U) {
        ++stats_.resets;
        clearTransferState();
        negotiated_syn_ = config_.local_syn;
        setState(kLinkIdle);
        listener_.onLinkReset(true);
        return;
    }

    if ((pkt.control & kCtrlSyn) != 0U) {
        if ((pkt.control & kCtrlAck) != 0U) {
            handleSynAck(pkt);
        } else {
            handleSynOnly(pkt);
        }
        return;
    }

    if ((pkt.control & kCtrlSlp) != 0U) {
        if (state_ == kLinkConnected) {
            if ((pkt.control & kCtrlAck) != 0U) {
                processAck(pkt.ack);
            }
            listener_.onLinkSleep();
        }
        return;
    }

    if (state_ == kLinkSynSent && passive_syn_ack_sent_ && (pkt.control & kCtrlAck) != 0U) {
        if (pkt.ack == config_.initial_seq) {
            passive_syn_ack_sent_ = false;
            tx_unacked_seq_ = static_cast<uint8_t>(config_.initial_seq + 1U);
            setState(kLinkConnected);
            listener_.onLinkConnected(negotiated_syn_);
            if (state_ == kLinkConnected && (pkt.control & kCtrlEak) == 0U && !pkt.payload.empty()) {
                handleDataPacket(pkt);
            }
        }
        return;
    }

    if (state_ != kLinkConnected) {
        return;
    }

    if ((pkt.control & kCtrlAck) != 0U) {
        processAck(pkt.ack);
    }
    if ((pkt.control & kCtrlEak) != 0U) {
        processEak(pkt);
        return;
    }
    if (!pkt.payload.empty()) {
        handleDataPacket(pkt);
    }
}

bool Link::negotiateSyn(const SynPayload& peer,
                        SynPayload& negotiated_out,
                        std::string* error_out) const {
    if (peer.version != kLinkVersion) {
        setError(error_out, "unsupported iAP2 link version");
        return false;
    }
    if (peer.max_outstanding == 0U || peer.max_outstanding > 127U) {
        setError(error_out, "invalid SYN max_outstanding");
        return false;
    }
    if (peer.max_packet_length < kLinkMinPacketWithPayloadBytes) {
        setError(error_out, "invalid SYN max_packet_length");
        return false;
    }
    if (peer.retransmission_timeout_ms == 0U && peer.max_retransmissions > 0U) {
        setError(error_out, "invalid SYN retransmission_timeout_ms == 0");
        return false;
    }
    if (peer.sessions.empty()) {
        setError(error_out, "SYN contains no sessions");
        return false;
    }
    for (size_t i = 0; i < peer.sessions.size(); ++i) {
        const SynSession& s = peer.sessions[i];
        if (s.id == 0U || !isValidSessionType(s.type)) {
            setError(error_out, "SYN contains invalid session entry");
            return false;
        }
        for (size_t j = i + 1U; j < peer.sessions.size(); ++j) {
            if (peer.sessions[j].id == s.id) {
                setError(error_out, "SYN contains duplicate session id");
                return false;
            }
        }
    }

    negotiated_out = peer;
    if (config_.local_syn.max_outstanding < negotiated_out.max_outstanding) {
        negotiated_out.max_outstanding = config_.local_syn.max_outstanding;
    }
    if (config_.local_syn.max_packet_length < negotiated_out.max_packet_length) {
        negotiated_out.max_packet_length = config_.local_syn.max_packet_length;
    }
    return true;
}

void Link::handleSynAck(const LinkPacket& pkt) {
    if (state_ == kLinkConnected && peer_syn_seen_ &&
        pkt.seq == peer_syn_seq_ && pkt.ack == config_.initial_seq) {
        // Duplicate SYN|ACK (our final handshake ACK was likely lost): re-send ACK.
        sendAckOnly();
        return;
    }
    if (state_ != kLinkSynSent) {
        return;
    }
    if (pkt.ack != config_.initial_seq) {
        return;
    }

    SynPayload peer_syn;
    std::string err;
    if (!SynPayload::decode(pkt.payload, peer_syn, &err)) {
        failLink(err, true);
        return;
    }
    SynPayload negotiated;
    if (!negotiateSyn(peer_syn, negotiated, &err)) {
        failLink(err, true);
        return;
    }

    negotiated_syn_ = negotiated;
    peer_syn_seen_ = true;
    peer_syn_seq_ = pkt.seq;
    rx_expected_seq_ = static_cast<uint8_t>(pkt.seq + 1U);
    last_rx_in_order_ = pkt.seq;
    tx_unacked_seq_ = static_cast<uint8_t>(config_.initial_seq + 1U);
    tx_next_seq_ = tx_unacked_seq_;
    last_tx_seq_ = config_.initial_seq;
    passive_syn_ack_sent_ = false;

    setState(kLinkConnected);
    sendAckOnly();
    listener_.onLinkConnected(negotiated_syn_);
}

void Link::handleSynOnly(const LinkPacket& pkt) {
    if (state_ == kLinkConnected) {
        // Peer initiated a fresh SYN while we were connected: reset and reply SYN|ACK.
        clearTransferState();
        listener_.onLinkReset(true);
    } else if (state_ != kLinkIdle && state_ != kLinkDetecting && state_ != kLinkSynSent) {
        return;
    }

    SynPayload peer_syn;
    std::string err;
    if (!SynPayload::decode(pkt.payload, peer_syn, &err)) {
        failLink(err, true);
        return;
    }
    SynPayload negotiated;
    if (!negotiateSyn(peer_syn, negotiated, &err)) {
        failLink(err, true);
        return;
    }

    negotiated_syn_ = negotiated;
    peer_syn_seen_ = true;
    peer_syn_seq_ = pkt.seq;
    rx_expected_seq_ = static_cast<uint8_t>(pkt.seq + 1U);
    last_rx_in_order_ = pkt.seq;
    last_tx_seq_ = config_.initial_seq;
    tx_unacked_seq_ = config_.initial_seq;
    tx_next_seq_ = static_cast<uint8_t>(config_.initial_seq + 1U);
    passive_syn_ack_sent_ = true;
    syn_last_tx_ms_ = last_now_ms_;
    has_syn_time_ = has_time_;

    setState(kLinkSynSent);
    std::vector<uint8_t> syn_bytes;
    if (!negotiated_syn_.encode(syn_bytes, &err)) {
        failLink(err, true);
        return;
    }
    sendControlPacket(static_cast<uint8_t>(kCtrlSyn | kCtrlAck),
                      config_.initial_seq, pkt.seq, 0, &syn_bytes);
}

void Link::processAck(uint8_t ack_num) {
    if (tx_inflight_.empty()) {
        return;
    }
    const uint8_t inflight_count = static_cast<uint8_t>(tx_next_seq_ - tx_unacked_seq_);
    const uint8_t dist = static_cast<uint8_t>(ack_num - tx_unacked_seq_);
    if (dist >= inflight_count) {
        return;  // Duplicate old ACK or outside current window.
    }
    const size_t acked_count = static_cast<size_t>(dist) + 1U;
    for (size_t i = 0; i < acked_count && !tx_inflight_.empty(); ++i) {
        tx_inflight_.pop_front();
        tx_unacked_seq_ = static_cast<uint8_t>(tx_unacked_seq_ + 1U);
    }
    drainTxPending();
}

void Link::processEak(const LinkPacket& pkt) {
    ++stats_.eaks_received;
    if (tx_inflight_.empty() || pkt.payload.empty()) {
        return;
    }

    // Track highest in-flight index that was selectively acknowledged.
    int highest_eak_idx = -1;
    for (size_t p = 0; p < pkt.payload.size(); ++p) {
        const uint8_t s = pkt.payload[p];
        for (size_t i = 0; i < tx_inflight_.size(); ++i) {
            if (tx_inflight_[i].seq == s) {
                tx_inflight_[i].selectively_acked = true;
                if (static_cast<int>(i) > highest_eak_idx) {
                    highest_eak_idx = static_cast<int>(i);
                }
                break;
            }
        }
    }

    // Retransmit unacknowledged gap packets preceding the highest EAKed sequence number.
    for (int i = 0; i < highest_eak_idx; ++i) {
        InFlightPacket& inflight = tx_inflight_[static_cast<size_t>(i)];
        if (!inflight.selectively_acked) {
            if (inflight.retries >= negotiated_syn_.max_retransmissions) {
                failLink("data packet max retransmissions exceeded", true);
                return;
            }
            ++inflight.retries;
            ++stats_.retransmissions;
            inflight.last_tx_ms = last_now_ms_;
            inflight.has_tx_time = has_time_;
            unacked_rx_count_ = 0;
            cum_ack_pending_ = false;
            has_cum_ack_time_ = false;
            sendControlPacket(kCtrlAck, inflight.seq, last_rx_in_order_,
                              inflight.session_id, &inflight.payload);
        }
    }
}

void Link::handleDataPacket(const LinkPacket& pkt) {
    if (pkt.session_id == 0U) {
        return;
    }
    const uint8_t rx_window = negotiated_syn_.max_outstanding > 0U
        ? negotiated_syn_.max_outstanding
        : 1U;
    const uint8_t dist = static_cast<uint8_t>(pkt.seq - rx_expected_seq_);

    if (dist >= 128U) {
        // Duplicate of an already-acknowledged/in-order packet.
        ++stats_.duplicate_rx_packets;
        if (!rx_reorder_.empty()) {
            sendEak();
        } else {
            sendAckOnly();
        }
        return;
    }

    if (dist >= rx_window) {
        // Beyond our receive window; ignore without advancing ACK.
        return;
    }

    if (dist > 0U) {
        // Out-of-order packet within receive window.
        for (size_t i = 0; i < rx_reorder_.size(); ++i) {
            if (rx_reorder_[i].seq == pkt.seq) {
                ++stats_.duplicate_rx_packets;
                sendEak();
                return;
            }
        }
        RxBufferedPacket entry;
        entry.seq = pkt.seq;
        entry.session_id = pkt.session_id;
        entry.payload = pkt.payload;
        size_t insert_pos = rx_reorder_.size();
        for (size_t i = 0; i < rx_reorder_.size(); ++i) {
            const uint8_t existing_dist = static_cast<uint8_t>(rx_reorder_[i].seq - rx_expected_seq_);
            if (dist < existing_dist) {
                insert_pos = i;
                break;
            }
        }
        rx_reorder_.insert(rx_reorder_.begin() + static_cast<ptrdiff_t>(insert_pos), entry);
        ++stats_.out_of_order_rx_packets;
        sendEak();
        unacked_rx_count_ = 0;
        cum_ack_pending_ = false;
        has_cum_ack_time_ = false;
        return;
    }

    // In-order packet (dist == 0).
    last_rx_in_order_ = pkt.seq;
    rx_expected_seq_ = static_cast<uint8_t>(pkt.seq + 1U);
    ++unacked_rx_count_;

    listener_.onLinkPayload(pkt.session_id, &pkt.payload[0], pkt.payload.size());
    if (state_ != kLinkConnected) {
        return;
    }

    bool gap_filled = false;
    while (!rx_reorder_.empty() && rx_reorder_.front().seq == rx_expected_seq_) {
        const RxBufferedPacket buffered = rx_reorder_.front();
        rx_reorder_.erase(rx_reorder_.begin());
        last_rx_in_order_ = buffered.seq;
        rx_expected_seq_ = static_cast<uint8_t>(buffered.seq + 1U);
        ++unacked_rx_count_;
        gap_filled = true;

        listener_.onLinkPayload(buffered.session_id,
                                buffered.payload.empty() ? NULL : &buffered.payload[0],
                                buffered.payload.size());
        if (state_ != kLinkConnected) {
            return;
        }
    }

    if (unacked_rx_count_ > 0U) {
        const bool hit_max_cum = (negotiated_syn_.max_cumulative_acks > 0U &&
                                  unacked_rx_count_ >= negotiated_syn_.max_cumulative_acks);
        if (gap_filled || hit_max_cum) {
            if (!rx_reorder_.empty()) {
                sendEak();
            } else {
                sendAckOnly();
            }
            unacked_rx_count_ = 0;
            cum_ack_pending_ = false;
            has_cum_ack_time_ = false;
        } else if (negotiated_syn_.max_cumulative_acks == 0U &&
                   negotiated_syn_.cumulative_ack_timeout_ms == 0U) {
            unacked_rx_count_ = 0;
        } else if (!cum_ack_pending_) {
            cum_ack_pending_ = true;
            cum_ack_since_ms_ = last_now_ms_;
            has_cum_ack_time_ = has_time_;
        }
    }
}

void Link::failLink(const std::string& reason, bool send_rst) {
    if (send_rst) {
        sendControlPacket(kCtrlRst, last_tx_seq_, 0, 0, NULL);
        ++stats_.resets;
    }
    clearTransferState();
    rx_buffer_.clear();
    setState(kLinkFailed);
    listener_.onLinkReset(false);
    listener_.onLinkError(reason);
}

void Link::tick(int64_t now_ms) {
    has_time_ = true;
    last_now_ms_ = now_ms;

    if (state_ == kLinkDetecting) {
        if (!has_detect_time_) {
            detect_last_tx_ms_ = now_ms;
            has_detect_time_ = true;
            return;
        }
        if (config_.detect_interval_ms > 0U &&
            now_ms - detect_last_tx_ms_ >= static_cast<int64_t>(config_.detect_interval_ms)) {
            if (config_.max_detect_attempts > 0U &&
                detect_attempts_ >= config_.max_detect_attempts) {
                failLink("iAP2 detect max attempts exceeded", false);
                return;
            }
            sendDetect();
            detect_last_tx_ms_ = now_ms;
        }
        return;
    }

    if (state_ == kLinkSynSent && !passive_syn_ack_sent_) {
        if (!has_syn_time_) {
            syn_last_tx_ms_ = now_ms;
            has_syn_time_ = true;
            return;
        }
        const uint16_t syn_rto = (config_.syn_retry_interval_ms > 0U)
            ? config_.syn_retry_interval_ms
            : config_.local_syn.retransmission_timeout_ms;
        const uint8_t syn_max = (config_.syn_retry_interval_ms > 0U && config_.max_syn_retries > 0U)
            ? config_.max_syn_retries
            : config_.local_syn.max_retransmissions;
        if (syn_rto > 0U &&
            now_ms - syn_last_tx_ms_ >= static_cast<int64_t>(syn_rto)) {
            if (syn_retries_ >= syn_max) {
                failLink("iAP2 SYN max retransmissions exceeded", true);
                return;
            }
            ++syn_retries_;
            ++stats_.retransmissions;
            sendSyn();
            syn_last_tx_ms_ = now_ms;
        }
        return;
    }

    if (state_ != kLinkConnected) {
        return;
    }

    if (cum_ack_pending_) {
        if (!has_cum_ack_time_) {
            cum_ack_since_ms_ = now_ms;
            has_cum_ack_time_ = true;
        } else if (now_ms - cum_ack_since_ms_ >= static_cast<int64_t>(negotiated_syn_.cumulative_ack_timeout_ms)) {
            if (!rx_reorder_.empty()) {
                sendEak();
            } else {
                sendAckOnly();
            }
            unacked_rx_count_ = 0;
            cum_ack_pending_ = false;
            has_cum_ack_time_ = false;
        }
    }

    const int64_t rto = static_cast<int64_t>(negotiated_syn_.retransmission_timeout_ms);
    const uint8_t max_retx = negotiated_syn_.max_retransmissions;
    for (size_t i = 0; i < tx_inflight_.size(); ++i) {
        InFlightPacket& inflight = tx_inflight_[i];
        if (inflight.selectively_acked) {
            continue;
        }
        if (!inflight.has_tx_time) {
            inflight.last_tx_ms = now_ms;
            inflight.has_tx_time = true;
            continue;
        }
        if (rto > 0 && now_ms - inflight.last_tx_ms >= rto) {
            if (inflight.retries >= max_retx) {
                failLink("iAP2 data packet max retransmissions exceeded", true);
                return;
            }
            ++inflight.retries;
            ++stats_.retransmissions;
            inflight.last_tx_ms = now_ms;
            unacked_rx_count_ = 0;
            cum_ack_pending_ = false;
            has_cum_ack_time_ = false;
            sendControlPacket(kCtrlAck, inflight.seq, last_rx_in_order_,
                              inflight.session_id, &inflight.payload);
        }
    }
}

}  // namespace iap2
}  // namespace carplay
