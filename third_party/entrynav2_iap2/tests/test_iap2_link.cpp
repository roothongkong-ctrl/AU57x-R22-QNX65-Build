#include "carplay/iap2_link.hpp"
#include "test_support.hpp"

#include <cstring>
#include <string>
#include <vector>

namespace iap2 = carplay::iap2;

typedef std::vector<uint8_t> Bytes;

namespace {

class RecordingSink : public iap2::ByteSink {
public:
    bool fail_next;
    Bytes stream;
    std::vector<Bytes> writes;

    RecordingSink() : fail_next(false) {}

    virtual bool sendBytes(const uint8_t* data, size_t len) {
        if (fail_next) {
            fail_next = false;
            return false;
        }
        Bytes chunk(data, data + len);
        writes.push_back(chunk);
        stream.insert(stream.end(), data, data + len);
        return true;
    }

    void clear() {
        stream.clear();
        writes.clear();
    }

    std::vector<iap2::LinkPacket> decodeAllPackets() const {
        std::vector<iap2::LinkPacket> pkts;
        size_t pos = 0;
        while (pos < stream.size()) {
            if (pos + iap2::kDetectSequenceBytes <= stream.size() &&
                std::memcmp(&stream[pos], iap2::kDetectSequence, iap2::kDetectSequenceBytes) == 0) {
                pos += iap2::kDetectSequenceBytes;
                continue;
            }
            if (pos + iap2::kLinkHeaderBytes <= stream.size() &&
                stream[pos] == 0xffU && stream[pos + 1U] == 0x5aU) {
                const uint16_t len = static_cast<uint16_t>(
                    (static_cast<uint16_t>(stream[pos + 2U]) << 8) | stream[pos + 3U]);
                iap2::LinkPacket pkt;
                if (len >= iap2::kLinkHeaderBytes && pos + len <= stream.size() &&
                    iap2::LinkPacket::decode(&stream[pos], len, pkt)) {
                    pkts.push_back(pkt);
                    pos += len;
                    continue;
                }
            }
            ++pos;
        }
        return pkts;
    }
};

struct DeliveredPayload {
    uint8_t session_id;
    Bytes data;
};

class RecordingListener : public iap2::LinkListener {
public:
    std::vector<iap2::LinkState> states;
    std::vector<iap2::SynPayload> connected_syns;
    std::vector<DeliveredPayload> payloads;
    std::vector<bool> resets;
    std::vector<std::string> errors;
    int sleep_count;

    RecordingListener() : sleep_count(0) {}

    virtual void onLinkStateChanged(iap2::LinkState new_state) {
        states.push_back(new_state);
    }
    virtual void onLinkConnected(const iap2::SynPayload& negotiated) {
        connected_syns.push_back(negotiated);
    }
    virtual void onLinkPayload(uint8_t session_id, const uint8_t* data, size_t len) {
        DeliveredPayload p;
        p.session_id = session_id;
        p.data.assign(data, data + len);
        payloads.push_back(p);
    }
    virtual void onLinkSleep() {
        ++sleep_count;
    }
    virtual void onLinkReset(bool by_peer) {
        resets.push_back(by_peer);
    }
    virtual void onLinkError(const std::string& reason) {
        errors.push_back(reason);
    }
};

Bytes encodeRawPacket(uint8_t control,
                      uint8_t seq,
                      uint8_t ack,
                      uint8_t session_id,
                      const Bytes& payload = Bytes()) {
    iap2::LinkPacket pkt;
    pkt.control = control;
    pkt.seq = seq;
    pkt.ack = ack;
    pkt.session_id = session_id;
    pkt.payload = payload;
    Bytes out;
    pkt.encode(out);
    return out;
}

Bytes encodeSynPacket(uint8_t control, uint8_t seq, uint8_t ack, const iap2::SynPayload& syn) {
    Bytes syn_bytes;
    syn.encode(syn_bytes);
    return encodeRawPacket(control, seq, ack, 0, syn_bytes);
}

void bringUpLink(iap2::Link& link,
                 RecordingSink& sink,
                 RecordingListener& listener,
                 uint8_t device_initial_seq,
                 const iap2::SynPayload& device_syn) {
    link.start();
    REQUIRE(link.state() == iap2::kLinkDetecting);
    REQUIRE(sink.stream == Bytes(iap2::kDetectSequence,
                                  iap2::kDetectSequence + iap2::kDetectSequenceBytes));
    sink.clear();

    // Device echoes detect.
    REQUIRE(link.feed(iap2::kDetectSequence, iap2::kDetectSequenceBytes));
    REQUIRE(link.state() == iap2::kLinkSynSent);
    std::vector<iap2::LinkPacket> sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    REQUIRE((sent[0].control & iap2::kCtrlSyn) != 0U);
    const uint8_t acc_seq = sent[0].seq;
    sink.clear();

    // Device sends SYN|ACK.
    const Bytes syn_ack = encodeSynPacket(
        static_cast<uint8_t>(iap2::kCtrlSyn | iap2::kCtrlAck),
        device_initial_seq, acc_seq, device_syn);
    REQUIRE(link.feed(syn_ack));
    REQUIRE(link.state() == iap2::kLinkConnected);
    REQUIRE(listener.connected_syns.size() == 1U);

    // Accessory sent final ACK.
    sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(iap2::kCtrlAck, sent[0].control);
    CHECK_EQ(device_initial_seq, sent[0].ack);
    sink.clear();
}

}  // namespace

TEST_CASE(packetCodecRoundTripsAndFailures) {
    // Header-only ACK packet.
    iap2::LinkPacket ack_pkt;
    ack_pkt.control = iap2::kCtrlAck;
    ack_pkt.seq = 0x12U;
    ack_pkt.ack = 0x34U;
    ack_pkt.session_id = 0U;

    Bytes wire;
    std::string err;
    REQUIRE(ack_pkt.encode(wire, &err));
    CHECK_EQ(9U, wire.size());
    CHECK_EQ(0xffU, wire[0]);
    CHECK_EQ(0x5aU, wire[1]);
    CHECK_EQ(0x00U, wire[2]);
    CHECK_EQ(0x09U, wire[3]);
    CHECK(iap2::verifyChecksum(&wire[0], 9U));

    iap2::LinkPacket decoded;
    REQUIRE(iap2::LinkPacket::decode(wire, decoded, &err));
    CHECK_EQ(ack_pkt.control, decoded.control);
    CHECK_EQ(ack_pkt.seq, decoded.seq);
    CHECK_EQ(ack_pkt.ack, decoded.ack);
    CHECK_EQ(ack_pkt.session_id, decoded.session_id);
    CHECK(decoded.payload.empty());

    // Payload packet.
    iap2::LinkPacket data_pkt;
    data_pkt.control = iap2::kCtrlAck;
    data_pkt.seq = 0xffU;
    data_pkt.ack = 0x05U;
    data_pkt.session_id = 10U;
    data_pkt.payload = test_support::hexToBytes("404000061d00");
    REQUIRE(data_pkt.encode(wire, &err));
    CHECK_EQ(16U, wire.size());
    REQUIRE(iap2::LinkPacket::decode(wire, decoded, &err));
    CHECK_EQ(data_pkt.session_id, decoded.session_id);
    CHECK_BYTES_EQ(data_pkt.payload, decoded.payload);

    // Corrupted header checksum.
    Bytes bad = wire;
    bad[8] ^= 0x01U;
    CHECK(!iap2::LinkPacket::decode(bad, decoded, &err));

    // Corrupted payload checksum.
    bad = wire;
    bad.back() ^= 0x01U;
    CHECK(!iap2::LinkPacket::decode(bad, decoded, &err));

    // Corrupted payload byte.
    bad = wire;
    bad[10] ^= 0x80U;
    CHECK(!iap2::LinkPacket::decode(bad, decoded, &err));

    // Wrong marker.
    bad = wire;
    bad[1] = 0x55U;
    CHECK(!iap2::LinkPacket::decode(bad, decoded, &err));

    // Length == 10 (invalid: payload checksum without payload).
    Bytes len10(10, 0);
    len10[0] = 0xffU;
    len10[1] = 0x5aU;
    len10[3] = 10U;
    len10[8] = iap2::computeChecksum(&len10[0], 8U);
    CHECK(!iap2::LinkPacket::decode(len10, decoded, &err));

    // Truncated packet.
    Bytes trunc(wire.begin(), wire.end() - 1);
    CHECK(!iap2::LinkPacket::decode(trunc, decoded, &err));
}

TEST_CASE(synPayloadCodecRoundTripAndValidation) {
    iap2::SynPayload syn;
    syn.version = 1;
    syn.max_outstanding = 5;
    syn.max_packet_length = 1024;
    syn.retransmission_timeout_ms = 1200;
    syn.cumulative_ack_timeout_ms = 60;
    syn.max_retransmissions = 8;
    syn.max_cumulative_acks = 2;
    syn.sessions.push_back(iap2::SynSession(10, iap2::kSessionTypeControl, 1));
    syn.sessions.push_back(iap2::SynSession(11, iap2::kSessionTypeFileTransfer, 1));
    syn.sessions.push_back(iap2::SynSession(12, iap2::kSessionTypeExternalAccessory, 2));

    Bytes raw;
    std::string err;
    REQUIRE(syn.encode(raw, &err));
    CHECK_EQ(19U, raw.size());

    iap2::SynPayload decoded;
    REQUIRE(iap2::SynPayload::decode(raw, decoded, &err));
    CHECK_EQ(1U, decoded.version);
    CHECK_EQ(5U, decoded.max_outstanding);
    CHECK_EQ(1024U, decoded.max_packet_length);
    CHECK_EQ(1200U, decoded.retransmission_timeout_ms);
    CHECK_EQ(60U, decoded.cumulative_ack_timeout_ms);
    CHECK_EQ(8U, decoded.max_retransmissions);
    CHECK_EQ(2U, decoded.max_cumulative_acks);
    REQUIRE(decoded.sessions.size() == 3U);
    CHECK_EQ(10U, decoded.sessions[0].id);
    CHECK_EQ(iap2::kSessionTypeFileTransfer, decoded.sessions[1].type);
    CHECK_EQ(2U, decoded.sessions[2].version);

    // Truncated & bad session-entry stride rejected.
    CHECK(!iap2::SynPayload::decode(Bytes(9, 0), decoded, &err));
    CHECK(!iap2::SynPayload::decode(Bytes(11, 0), decoded, &err));
}

TEST_CASE(detectBothDirectionsAndGarbageResync) {
    RecordingSink sink;
    RecordingListener listener;
    iap2::LinkConfig cfg;
    cfg.initial_seq = 0x20U;
    iap2::Link link(sink, listener, cfg);

    link.start();
    CHECK_EQ(iap2::kLinkDetecting, link.state());
    CHECK_BYTES_EQ(Bytes(iap2::kDetectSequence, iap2::kDetectSequence + 6U), sink.stream);
    sink.clear();

    // Feed garbage containing false 0xFF sync bytes, false 0xFF 0x55, false 0xFF 0x5A with bad checksum,
    // followed by a split detect sequence and coalesced SYN|ACK + data packet!
    Bytes stream = test_support::hexToBytes("001122ffff550102ff5a00094000000000deadbeef");
    stream.insert(stream.end(), iap2::kDetectSequence, iap2::kDetectSequence + 6U);

    iap2::SynPayload dev_syn = cfg.local_syn;
    const Bytes syn_ack = encodeSynPacket(
        static_cast<uint8_t>(iap2::kCtrlSyn | iap2::kCtrlAck), 0x80U, 0x20U, dev_syn);
    stream.insert(stream.end(), syn_ack.begin(), syn_ack.end());

    const Bytes app_payload = test_support::hexToBytes("404000061d00");
    const Bytes data_pkt = encodeRawPacket(iap2::kCtrlAck, 0x81U, 0x20U, 10U, app_payload);
    stream.insert(stream.end(), data_pkt.begin(), data_pkt.end());

    // Deliver 1 byte at a time!
    for (size_t i = 0; i < stream.size(); ++i) {
        REQUIRE(link.feed(&stream[i], 1U));
    }

    CHECK_EQ(iap2::kLinkConnected, link.state());
    CHECK(link.stats().resync_bytes_dropped >= 15U);
    CHECK(link.stats().bad_checksums >= 1U);
    REQUIRE(listener.payloads.size() == 1U);
    CHECK_EQ(10U, listener.payloads[0].session_id);
    CHECK_BYTES_EQ(app_payload, listener.payloads[0].data);

    // Test passive detect (device sends FF 55 02 00 EE 10 while accessory is kLinkIdle).
    RecordingSink sink2;
    RecordingListener listener2;
    iap2::Link link2(sink2, listener2, cfg);
    REQUIRE(link2.feed(iap2::kDetectSequence, 6U));
    CHECK_EQ(iap2::kLinkSynSent, link2.state());
    // Accessory must echo detect AND send SYN.
    REQUIRE(sink2.stream.size() >= 6U + iap2::kLinkHeaderBytes);
    CHECK(std::memcmp(&sink2.stream[0], iap2::kDetectSequence, 6U) == 0);
    CHECK_EQ(1U, sink2.decodeAllPackets().size());

    // Bad/iAP1 detect fails link cleanly.
    RecordingSink sink3;
    RecordingListener listener3;
    iap2::Link link3(sink3, listener3, cfg);
    link3.start();
    REQUIRE(link3.feed(iap2::kDetectBadSequence, iap2::kDetectBadSequenceBytes));
    CHECK_EQ(iap2::kLinkFailed, link3.state());
    CHECK(!listener3.errors.empty());
}

TEST_CASE(synNegotiationAndValidation) {
    RecordingSink sink;
    RecordingListener listener;
    iap2::LinkConfig cfg;
    cfg.local_syn.max_outstanding = 6;
    cfg.local_syn.max_packet_length = 1024;
    cfg.initial_seq = 1U;
    iap2::Link link(sink, listener, cfg);

    // Peer shrinks max_outstanding to 3 and max_packet_length to 128.
    iap2::SynPayload dev_syn = cfg.local_syn;
    dev_syn.max_outstanding = 3;
    dev_syn.max_packet_length = 128;
    bringUpLink(link, sink, listener, 10U, dev_syn);

    CHECK_EQ(3U, link.negotiatedSyn().max_outstanding);
    CHECK_EQ(128U, link.negotiatedSyn().max_packet_length);
    CHECK_EQ(118U, link.maxPayloadBytes());

    // Oversized payload (> 118 bytes) rejected; 118-byte payload accepted.
    CHECK(!link.sendPayload(10U, Bytes(119, 0xaaU)));
    CHECK(!link.sendPayload(0U, Bytes(10, 0xaaU)));
    CHECK(!link.sendPayload(99U, Bytes(10, 0xaaU)));  // unnegotiated session ID
    CHECK(link.sendPayload(10U, Bytes(118, 0xbbU)));
    sink.clear();

    // Duplicate SYN|ACK re-sends ACK without resetting.
    const Bytes dup_syn_ack = encodeSynPacket(
        static_cast<uint8_t>(iap2::kCtrlSyn | iap2::kCtrlAck), 10U, 1U, dev_syn);
    REQUIRE(link.feed(dup_syn_ack));
    CHECK_EQ(iap2::kLinkConnected, link.state());
    CHECK_EQ(1U, sink.decodeAllPackets().size());
    CHECK_EQ(iap2::kCtrlAck, sink.decodeAllPackets()[0].control);

    // Invalid SYN version triggers RST and kLinkFailed.
    RecordingSink sink_bad;
    RecordingListener listener_bad;
    iap2::Link link_bad(sink_bad, listener_bad, cfg);
    link_bad.start();
    link_bad.feed(iap2::kDetectSequence, 6U);
    sink_bad.clear();

    iap2::SynPayload bad_syn = dev_syn;
    bad_syn.version = 2;
    REQUIRE(link_bad.feed(encodeSynPacket(
        static_cast<uint8_t>(iap2::kCtrlSyn | iap2::kCtrlAck), 10U, 1U, bad_syn)));
    CHECK_EQ(iap2::kLinkFailed, link_bad.state());
    std::vector<iap2::LinkPacket> sent_bad = sink_bad.decodeAllPackets();
    REQUIRE(sent_bad.size() == 1U);
    CHECK((sent_bad[0].control & iap2::kCtrlRst) != 0U);
}

TEST_CASE(slidingWindowRetransmitAndMaxRetries) {
    RecordingSink sink;
    RecordingListener listener;
    iap2::LinkConfig cfg;
    cfg.initial_seq = 253U;  // Exercise 255 -> 0 wraparound!
    cfg.local_syn.max_outstanding = 2;
    cfg.local_syn.retransmission_timeout_ms = 500;
    cfg.local_syn.max_retransmissions = 2;
    iap2::Link link(sink, listener, cfg);

    link.tick(1000);
    bringUpLink(link, sink, listener, 40U, cfg.local_syn);

    // Queue 4 payloads; window=2 means seq 254 and 255 go out immediately, 2 stay pending.
    REQUIRE(link.sendPayload(10U, Bytes(1, 0x01U)));
    REQUIRE(link.sendPayload(10U, Bytes(1, 0x02U)));
    REQUIRE(link.sendPayload(10U, Bytes(1, 0x03U)));
    REQUIRE(link.sendPayload(10U, Bytes(1, 0x04U)));

    CHECK_EQ(2U, link.inFlightCount());
    CHECK_EQ(2U, link.pendingCount());
    std::vector<iap2::LinkPacket> sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 2U);
    CHECK_EQ(254U, sent[0].seq);
    CHECK_EQ(255U, sent[1].seq);
    sink.clear();

    // Before RTO: no retransmission.
    link.tick(1499);
    CHECK(sink.stream.empty());

    // At RTO (1000 + 500 = 1500): both unacked packets retransmit.
    link.tick(1500);
    sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 2U);
    CHECK_EQ(254U, sent[0].seq);
    CHECK_EQ(255U, sent[1].seq);
    CHECK_EQ(2ULL, link.stats().retransmissions);
    sink.clear();

    // ACK seq 254 -> slides window, transmits seq 0!
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 40U, 254U, 0U)));
    CHECK_EQ(2U, link.inFlightCount());
    CHECK_EQ(1U, link.pendingCount());
    sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(0U, sent[0].seq);
    CHECK_EQ(0x03U, sent[0].payload[0]);
    sink.clear();

    // ACK seq 0 (acks 255 and 0 together) -> transmits seq 1!
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 40U, 0U, 0U)));
    CHECK_EQ(1U, link.inFlightCount());
    CHECK_EQ(0U, link.pendingCount());
    sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(1U, sent[0].seq);
    sink.clear();

    // Now let seq 1 exceed max_retransmissions (2):
    // seq 1 was sent at t=1500; retry 1 at 2000, retry 2 at 2500, fail at 3000.
    link.tick(2000);
    CHECK_EQ(1U, sink.decodeAllPackets().size());
    sink.clear();
    link.tick(2500);
    CHECK_EQ(1U, sink.decodeAllPackets().size());
    sink.clear();
    link.tick(3000);
    CHECK_EQ(iap2::kLinkFailed, link.state());
    REQUIRE(sink.decodeAllPackets().size() == 1U);
    CHECK((sink.decodeAllPackets()[0].control & iap2::kCtrlRst) != 0U);
    CHECK(!listener.resets.empty() && !listener.resets.back());
}

TEST_CASE(cumulativeAckTimerAndPiggybacking) {
    RecordingSink sink;
    RecordingListener listener;
    iap2::LinkConfig cfg;
    cfg.local_syn.cumulative_ack_timeout_ms = 75;
    cfg.local_syn.max_cumulative_acks = 3;
    iap2::Link link(sink, listener, cfg);

    link.tick(0);
    bringUpLink(link, sink, listener, 10U, cfg.local_syn);

    // Receive 1 packet (seq 11) -> delayed ACK.
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 11U, 0U, 10U, Bytes(2, 0xaaU))));
    CHECK(sink.stream.empty());
    link.tick(74);
    CHECK(sink.stream.empty());
    link.tick(75);
    std::vector<iap2::LinkPacket> sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(iap2::kCtrlAck, sent[0].control);
    CHECK_EQ(11U, sent[0].ack);
    sink.clear();

    // Receive 3 packets (12, 13, 14) -> immediate ACK on the 3rd!
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 12U, 0U, 10U, Bytes(1, 1))));
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 13U, 0U, 10U, Bytes(1, 2))));
    CHECK(sink.stream.empty());
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 14U, 0U, 10U, Bytes(1, 3))));
    sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(14U, sent[0].ack);
    sink.clear();

    // Receive 1 packet (15), then immediately send outbound data -> piggybacks ACK 15!
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 15U, 0U, 10U, Bytes(1, 4))));
    CHECK(sink.stream.empty());
    REQUIRE(link.sendPayload(10U, Bytes(1, 0x55U)));
    sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(15U, sent[0].ack);
    sink.clear();
    // Cumulative ACK timer must NOT fire an extra standalone ACK.
    link.tick(200);
    CHECK(sink.stream.empty());
}

TEST_CASE(packetLossOutOfOrderEakAndDuplicates) {
    RecordingSink sink;
    RecordingListener listener;
    iap2::LinkConfig cfg;
    cfg.initial_seq = 0U;
    cfg.local_syn.max_outstanding = 4;
    cfg.local_syn.retransmission_timeout_ms = 1000;
    iap2::Link link(sink, listener, cfg);

    link.tick(0);
    bringUpLink(link, sink, listener, 254U, cfg.local_syn);  // expected rx = 255

    // Peer sends 255 (dropped!), then 0 and 1 arrive out of order.
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 0U, 0U, 10U, Bytes(1, 0xa0U))));
    CHECK(listener.payloads.empty());
    std::vector<iap2::LinkPacket> sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(static_cast<uint8_t>(iap2::kCtrlAck | iap2::kCtrlEak), sent[0].control);
    CHECK_EQ(254U, sent[0].ack);
    CHECK_BYTES_EQ(Bytes(1, 0U), sent[0].payload);
    sink.clear();

    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 1U, 0U, 10U, Bytes(1, 0xa1U))));
    CHECK(listener.payloads.empty());
    sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(254U, sent[0].ack);
    Bytes expected_eak;
    expected_eak.push_back(0U);
    expected_eak.push_back(1U);
    CHECK_BYTES_EQ(expected_eak, sent[0].payload);
    sink.clear();

    // Duplicate of buffered seq 0 and old seq 254 must NOT deliver.
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 0U, 0U, 10U, Bytes(1, 0xa0U))));
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 254U, 0U, 10U, Bytes(1, 0x99U))));
    CHECK(listener.payloads.empty());
    CHECK_EQ(2ULL, link.stats().duplicate_rx_packets);
    sink.clear();

    // Missing seq 255 arrives -> delivers 255, 0, 1 in order and ACKs 1!
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 255U, 0U, 10U, Bytes(1, 0xffU))));
    REQUIRE(listener.payloads.size() == 3U);
    CHECK_EQ(0xffU, listener.payloads[0].data[0]);
    CHECK_EQ(0xa0U, listener.payloads[1].data[0]);
    CHECK_EQ(0xa1U, listener.payloads[2].data[0]);
    sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(iap2::kCtrlAck, sent[0].control);
    CHECK_EQ(1U, sent[0].ack);
    sink.clear();

    // Outbound EAK handling: send seq 1, 2, 3. Peer EAKs [2, 3] with ack=0.
    REQUIRE(link.sendPayload(10U, Bytes(1, 0x11U)));  // seq 1
    REQUIRE(link.sendPayload(10U, Bytes(1, 0x22U)));  // seq 2
    REQUIRE(link.sendPayload(10U, Bytes(1, 0x33U)));  // seq 3
    sink.clear();

    Bytes peer_eak_list;
    peer_eak_list.push_back(2U);
    peer_eak_list.push_back(3U);
    REQUIRE(link.feed(encodeRawPacket(
        static_cast<uint8_t>(iap2::kCtrlAck | iap2::kCtrlEak), 2U, 0U, 0U, peer_eak_list)));
    sent = sink.decodeAllPackets();
    // Only gap packet seq 1 should be fast-retransmitted!
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(1U, sent[0].seq);
    CHECK_EQ(0x11U, sent[0].payload[0]);
    sink.clear();

    // Final ACK(3) clears all 3.
    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlAck, 2U, 3U, 0U)));
    CHECK_EQ(0U, link.inFlightCount());
}

TEST_CASE(rstAndSleepHandling) {
    RecordingSink sink;
    RecordingListener listener;
    iap2::LinkConfig cfg;
    iap2::Link link(sink, listener, cfg);
    bringUpLink(link, sink, listener, 5U, cfg.local_syn);

    // Send & receive SLP.
    REQUIRE(link.sendSleep());
    std::vector<iap2::LinkPacket> sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(iap2::kCtrlSlp, sent[0].control);
    sink.clear();

    REQUIRE(link.feed(encodeRawPacket(iap2::kCtrlSlp, 6U, 0U, 0U)));
    CHECK_EQ(1, listener.sleep_count);

    // Receive RST coalesced with a fresh detect sequence.
    Bytes rst_and_detect = encodeRawPacket(iap2::kCtrlRst, 0U, 0U, 0U);
    rst_and_detect.insert(rst_and_detect.end(), iap2::kDetectSequence, iap2::kDetectSequence + 6U);
    REQUIRE(link.feed(rst_and_detect));
    REQUIRE(listener.resets.size() == 1U);
    CHECK(listener.resets[0]);  // by_peer == true
    // Re-detect started immediately after peer RST!
    CHECK_EQ(iap2::kLinkSynSent, link.state());

    // Local reset sends RST.
    sink.clear();
    link.reset(true);
    CHECK_EQ(iap2::kLinkIdle, link.state());
    sent = sink.decodeAllPackets();
    REQUIRE(sent.size() == 1U);
    CHECK_EQ(iap2::kCtrlRst, sent[0].control);
}

TEST_CASE(diplayPresetsAndZeroAckMode) {
    // Wired USB preset: immediate detect + SYN, initial_seq=99, 3 sessions, zero-ack mode.
    {
        RecordingSink sink;
        RecordingListener listener;
        iap2::LinkConfig cfg = iap2::LinkConfig::wiredUsb();
        CHECK_EQ(99U, cfg.initial_seq);
        CHECK(cfg.send_syn_with_detect);
        CHECK_EQ(500U, cfg.syn_retry_interval_ms);
        CHECK_EQ(0U, cfg.local_syn.retransmission_timeout_ms);
        CHECK_EQ(0U, cfg.local_syn.max_retransmissions);
        REQUIRE(cfg.local_syn.sessions.size() == 3U);
        CHECK_EQ(iap2::kDefaultControlSessionId, cfg.local_syn.sessions[0].id);
        CHECK_EQ(iap2::kDefaultControlSessionVersion, cfg.local_syn.sessions[0].version);
        CHECK_EQ(iap2::kDefaultEaSessionId, cfg.local_syn.sessions[1].id);
        CHECK_EQ(iap2::kDefaultFileTransferSessionId, cfg.local_syn.sessions[2].id);

        iap2::Link link(sink, listener, cfg);
        link.tick(0);
        link.start();
        CHECK_EQ(iap2::kLinkSynSent, link.state());
        // Should have emitted detect + SYN in one go.
        REQUIRE(sink.stream.size() > iap2::kDetectSequenceBytes);
        CHECK(std::memcmp(&sink.stream[0], iap2::kDetectSequence, iap2::kDetectSequenceBytes) == 0);
        std::vector<iap2::LinkPacket> pkts = sink.decodeAllPackets();
        REQUIRE(pkts.size() == 1U);
        CHECK_EQ(iap2::kCtrlSyn, pkts[0].control);
        CHECK_EQ(99U, pkts[0].seq);
        sink.clear();

        // SYN retry fires at syn_retry_interval_ms = 500 even though local_syn RTO is 0.
        link.tick(499);
        CHECK(sink.stream.empty());
        link.tick(500);
        pkts = sink.decodeAllPackets();
        REQUIRE(pkts.size() == 1U);
        CHECK_EQ(iap2::kCtrlSyn, pkts[0].control);
        sink.clear();

        // Peer responds with zero-ack SYN|ACK.
        REQUIRE(link.feed(encodeSynPacket(
            static_cast<uint8_t>(iap2::kCtrlSyn | iap2::kCtrlAck), 10U, 99U, cfg.local_syn)));
        CHECK_EQ(iap2::kLinkConnected, link.state());
        sink.clear();

        // Transmit more than max_outstanding (4) packets without peer ACKs -> zero in-flight.
        for (int i = 0; i < 6; ++i) {
            CHECK(link.sendPayload(iap2::kDefaultControlSessionId, Bytes(2, static_cast<uint8_t>(i))));
        }
        CHECK_EQ(0U, link.inFlightCount());
        CHECK_EQ(0U, link.pendingCount());
        CHECK_EQ(6U, sink.decodeAllPackets().size());
        sink.clear();

        // Receiving data in zero-ack mode delivers payload without scheduling cumulative ACK.
        REQUIRE(link.feed(encodeRawPacket(
            iap2::kCtrlAck, 11U, 105U, iap2::kDefaultControlSessionId, Bytes(3, 0x42U))));
        REQUIRE(listener.payloads.size() == 1U);
        link.tick(5000);
        CHECK(sink.stream.empty());
    }

    // Wireless Bluetooth preset: waits for peer detect, keeps reliable timers.
    {
        RecordingSink sink;
        RecordingListener listener;
        iap2::LinkConfig cfg = iap2::LinkConfig::wirelessBluetooth();
        CHECK(!cfg.send_syn_with_detect);
        CHECK_EQ(4000U, cfg.local_syn.retransmission_timeout_ms);
        CHECK_EQ(500U, cfg.local_syn.cumulative_ack_timeout_ms);
        CHECK_EQ(4U, cfg.local_syn.max_retransmissions);
        CHECK_EQ(3U, cfg.local_syn.max_cumulative_acks);

        iap2::Link link(sink, listener, cfg);
        link.start();
        CHECK_EQ(iap2::kLinkDetecting, link.state());
        CHECK(sink.decodeAllPackets().empty());
    }

    // Wireless Type-130 tunnel preset: matches wired USB zero-ack initiator.
    {
        iap2::LinkConfig tun = iap2::LinkConfig::wirelessTunnel();
        CHECK(tun.send_syn_with_detect);
        CHECK_EQ(0U, tun.local_syn.retransmission_timeout_ms);
        CHECK_EQ(0U, tun.local_syn.max_retransmissions);
    }
}

int main() {
    return test_support::runAll("test_iap2_link");
}
