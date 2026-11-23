// Derived from DiPlay
// (shared/src/main/java/com/shilapi/xcertplay/iap2/catalog/Iap2Endpoint.kt,
//  shared/src/main/java/com/shilapi/xcertplay/iap2/message/Iap2Messages.kt,
//  shared/src/main/java/com/shilapi/xcertplay/iap2/wire/Iap2CsmFramer.kt,
//  shared/src/main/java/com/shilapi/xcertplay/iap2/wire/Iap2WireCodec.kt),
// GPL-3.0, Copyright (c) DiPlay contributors.

#include "carplay/iap2_wire.hpp"

#include <sstream>

namespace carplay {
namespace iap2 {

namespace {

void setError(std::string* err, const std::string& msg) {
    if (err != NULL) {
        *err = msg;
    }
}

uint16_t readU16Be(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint32_t readU32Be(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

uint64_t readU64Be(const uint8_t* p) {
    uint64_t v = 0;
    for (size_t i = 0; i < 8; ++i) {
        v = (v << 8) | static_cast<uint64_t>(p[i]);
    }
    return v;
}

void writeU16Be(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xffU));
    out.push_back(static_cast<uint8_t>(v & 0xffU));
}

void writeU32Be(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xffU));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xffU));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xffU));
    out.push_back(static_cast<uint8_t>(v & 0xffU));
}

void writeU64Be(std::vector<uint8_t>& out, uint64_t v) {
    for (size_t i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((v >> ((7U - i) * 8U)) & 0xffU));
    }
}

bool decodeBinaryFlag(const ParameterList& grp,
                      uint16_t param_id,
                      const char* label,
                      bool& has_val,
                      bool& val_out,
                      std::string* error_out) {
    uint8_t raw = 0;
    if (!grp.readU8(param_id, raw)) {
        if (grp.findFirst(param_id) != NULL) {
            std::ostringstream oss;
            oss << label << " availability parameter has wrong length";
            setError(error_out, oss.str());
            return false;
        }
        has_val = false;
        val_out = false;
        return true;
    }
    if (raw > 1) {
        std::ostringstream oss;
        oss << label << " availability must be 0 or 1, got " << static_cast<unsigned int>(raw);
        setError(error_out, oss.str());
        return false;
    }
    has_val = true;
    val_out = (raw == 1);
    return true;
}

}  // namespace

void ParameterList::addRaw(uint16_t id, const std::vector<uint8_t>& payload) {
    params_.push_back(Parameter(id, payload));
}

void ParameterList::addVoid(uint16_t id) {
    params_.resize(params_.size() + 1);
    Parameter& p = params_.back();
    p.id = id;
    p.payload.clear();
}

void ParameterList::addU8(uint16_t id, uint8_t value) {
    params_.resize(params_.size() + 1);
    Parameter& p = params_.back();
    p.id = id;
    p.payload.clear();
    p.payload.push_back(value);
}

void ParameterList::addI8(uint16_t id, int8_t value) {
    addU8(id, static_cast<uint8_t>(value));
}

void ParameterList::addBool(uint16_t id, bool value) {
    addU8(id, value ? 1U : 0U);
}

void ParameterList::addU16(uint16_t id, uint16_t value) {
    std::vector<uint8_t> p;
    writeU16Be(p, value);
    params_.push_back(Parameter(id, p));
}

void ParameterList::addI16(uint16_t id, int16_t value) {
    addU16(id, static_cast<uint16_t>(value));
}

void ParameterList::addU32(uint16_t id, uint32_t value) {
    std::vector<uint8_t> p;
    writeU32Be(p, value);
    params_.push_back(Parameter(id, p));
}

void ParameterList::addI32(uint16_t id, int32_t value) {
    addU32(id, static_cast<uint32_t>(value));
}

void ParameterList::addU64(uint16_t id, uint64_t value) {
    std::vector<uint8_t> p;
    writeU64Be(p, value);
    params_.push_back(Parameter(id, p));
}

void ParameterList::addString(uint16_t id, const std::string& value) {
    std::vector<uint8_t> p(value.begin(), value.end());
    p.push_back(0);  // iAP2 strings are NUL-terminated UTF-8.
    params_.push_back(Parameter(id, p));
}

void ParameterList::addStrings(uint16_t id, const std::vector<std::string>& values) {
    std::vector<uint8_t> p;
    for (size_t i = 0; i < values.size(); ++i) {
        p.insert(p.end(), values[i].begin(), values[i].end());
        p.push_back(0);
    }
    params_.push_back(Parameter(id, p));
}

void ParameterList::addU16List(uint16_t id, const std::vector<uint16_t>& values) {
    std::vector<uint8_t> p;
    p.reserve(values.size() * 2U);
    for (size_t i = 0; i < values.size(); ++i) {
        writeU16Be(p, values[i]);
    }
    params_.push_back(Parameter(id, p));
}

void ParameterList::addGroup(uint16_t id, const ParameterList& group) {
    std::vector<uint8_t> encoded;
    if (group.encode(encoded, NULL)) {
        params_.push_back(Parameter(id, encoded));
    }
}

const Parameter* ParameterList::findFirst(uint16_t id) const {
    for (size_t i = 0; i < params_.size(); ++i) {
        if (params_[i].id == id) {
            return &params_[i];
        }
    }
    return NULL;
}

std::vector<const Parameter*> ParameterList::findAll(uint16_t id) const {
    std::vector<const Parameter*> matches;
    for (size_t i = 0; i < params_.size(); ++i) {
        if (params_[i].id == id) {
            matches.push_back(&params_[i]);
        }
    }
    return matches;
}

bool ParameterList::readU8(uint16_t id, uint8_t& out) const {
    const Parameter* p = findFirst(id);
    if (p == NULL || p->payload.size() != 1U) {
        return false;
    }
    out = p->payload[0];
    return true;
}

bool ParameterList::readI8(uint16_t id, int8_t& out) const {
    uint8_t raw = 0;
    if (!readU8(id, raw)) return false;
    out = static_cast<int8_t>(raw);
    return true;
}

bool ParameterList::readBool(uint16_t id, bool& out) const {
    uint8_t raw = 0;
    if (!readU8(id, raw)) return false;
    out = (raw != 0);
    return true;
}

bool ParameterList::readU16(uint16_t id, uint16_t& out) const {
    const Parameter* p = findFirst(id);
    if (p == NULL || p->payload.size() != 2U) {
        return false;
    }
    out = readU16Be(&p->payload[0]);
    return true;
}

bool ParameterList::readI16(uint16_t id, int16_t& out) const {
    uint16_t raw = 0;
    if (!readU16(id, raw)) return false;
    out = static_cast<int16_t>(raw);
    return true;
}

bool ParameterList::readU32(uint16_t id, uint32_t& out) const {
    const Parameter* p = findFirst(id);
    if (p == NULL || p->payload.size() != 4U) {
        return false;
    }
    out = readU32Be(&p->payload[0]);
    return true;
}

bool ParameterList::readI32(uint16_t id, int32_t& out) const {
    uint32_t raw = 0;
    if (!readU32(id, raw)) return false;
    out = static_cast<int32_t>(raw);
    return true;
}

bool ParameterList::readU64(uint16_t id, uint64_t& out) const {
    const Parameter* p = findFirst(id);
    if (p == NULL || p->payload.size() != 8U) {
        return false;
    }
    out = readU64Be(&p->payload[0]);
    return true;
}

bool ParameterList::readBytes(uint16_t id, std::vector<uint8_t>& out) const {
    const Parameter* p = findFirst(id);
    if (p == NULL) {
        return false;
    }
    out = p->payload;
    return true;
}

bool ParameterList::readString(uint16_t id, std::string& out) const {
    const Parameter* p = findFirst(id);
    if (p == NULL || p->payload.empty() || p->payload.back() != 0) {
        return false;
    }
    out.assign(p->payload.begin(), p->payload.end() - 1);
    return true;
}

bool ParameterList::readStrings(uint16_t id, std::vector<std::string>& out) const {
    const Parameter* p = findFirst(id);
    if (p == NULL) {
        return false;
    }
    out.clear();
    if (p->payload.empty()) {
        return true;
    }
    if (p->payload.back() != 0) {
        return false;
    }
    size_t start = 0;
    for (size_t i = 0; i < p->payload.size(); ++i) {
        if (p->payload[i] == 0) {
            out.push_back(std::string(p->payload.begin() + static_cast<ptrdiff_t>(start),
                                      p->payload.begin() + static_cast<ptrdiff_t>(i)));
            start = i + 1U;
        }
    }
    return true;
}

bool ParameterList::readU16List(uint16_t id, std::vector<uint16_t>& out) const {
    const Parameter* p = findFirst(id);
    if (p == NULL || (p->payload.size() % 2U) != 0) {
        return false;
    }
    out.clear();
    out.reserve(p->payload.size() / 2U);
    for (size_t i = 0; i < p->payload.size(); i += 2U) {
        out.push_back(readU16Be(&p->payload[i]));
    }
    return true;
}

bool ParameterList::readGroup(uint16_t id, ParameterList& group_out, std::string* error_out) const {
    const Parameter* p = findFirst(id);
    if (p == NULL) {
        setError(error_out, "missing group parameter");
        return false;
    }
    return decode(p->payload, group_out, error_out);
}

bool ParameterList::encode(std::vector<uint8_t>& out, std::string* error_out) const {
    out.clear();
    for (size_t i = 0; i < params_.size(); ++i) {
        const size_t total_len = kParamHeaderBytes + params_[i].payload.size();
        if (total_len > 0xffffU) {
            setError(error_out, "iAP2 parameter length exceeds 65535 bytes");
            out.clear();
            return false;
        }
        writeU16Be(out, static_cast<uint16_t>(total_len));
        writeU16Be(out, params_[i].id);
        out.insert(out.end(), params_[i].payload.begin(), params_[i].payload.end());
    }
    return true;
}

bool ParameterList::decode(const uint8_t* data,
                           size_t len,
                           ParameterList& list_out,
                           std::string* error_out) {
    list_out.clear();
    if (len > 0 && data == NULL) {
        setError(error_out, "null buffer with non-zero length");
        return false;
    }
    size_t pos = 0;
    while (pos < len) {
        if (pos + kParamHeaderBytes > len) {
            setError(error_out, "truncated iAP2 parameter header");
            list_out.clear();
            return false;
        }
        const uint16_t param_len = readU16Be(data + pos);
        const uint16_t param_id = readU16Be(data + pos + 2);
        if (param_len < kParamHeaderBytes) {
            setError(error_out, "iAP2 parameter length < 4");
            list_out.clear();
            return false;
        }
        if (pos + param_len > len) {
            setError(error_out, "truncated iAP2 parameter payload");
            list_out.clear();
            return false;
        }
        const size_t body_len = static_cast<size_t>(param_len) - kParamHeaderBytes;
        std::vector<uint8_t> body;
        if (body_len > 0) {
            body.assign(data + pos + kParamHeaderBytes, data + pos + param_len);
        }
        list_out.addRaw(param_id, body);
        pos += param_len;
    }
    return true;
}

bool ParameterList::decode(const std::vector<uint8_t>& bytes,
                           ParameterList& list_out,
                           std::string* error_out) {
    const uint8_t* ptr = bytes.empty() ? NULL : &bytes[0];
    return decode(ptr, bytes.size(), list_out, error_out);
}

bool Frame::encode(std::vector<uint8_t>& out, std::string* error_out) const {
    return CsmFramer::encodeFrame(*this, out, error_out);
}

bool Frame::build(uint16_t message_id,
                  const ParameterList& params,
                  Frame& frame_out,
                  std::string* error_out) {
    std::vector<uint8_t> body;
    if (!params.encode(body, error_out)) {
        return false;
    }
    if (kCsmHeaderBytes + body.size() > kCsmMaxFrameBytes) {
        setError(error_out, "iAP2 CSM frame exceeds 65535 bytes");
        return false;
    }
    frame_out.message_id = message_id;
    frame_out.payload = body;
    return true;
}

void CsmFramer::reset() {
    buffer_.clear();
}

bool CsmFramer::push(const uint8_t* data,
                     size_t len,
                     std::vector<Frame>& frames_out,
                     std::string* error_out) {
    if (len > 0 && data == NULL) {
        setError(error_out, "null input buffer");
        return false;
    }
    if (len > 0) {
        buffer_.insert(buffer_.end(), data, data + len);
    }

    size_t consumed = 0;
    while (buffer_.size() - consumed >= kCsmHeaderBytes) {
        const uint8_t* p = &buffer_[consumed];
        const uint16_t marker = readU16Be(p);
        if (marker != kCsmStartMarker) {
            setError(error_out, "invalid iAP2 CSM start marker (expected 0x4040)");
            buffer_.clear();
            return false;
        }
        const uint16_t total_len = readU16Be(p + 2);
        if (total_len < kCsmHeaderBytes) {
            setError(error_out, "invalid iAP2 CSM frame length < 6");
            buffer_.clear();
            return false;
        }
        if (buffer_.size() - consumed < total_len) {
            break;
        }
        const uint16_t msg_id = readU16Be(p + 4);
        std::vector<uint8_t> body;
        if (total_len > kCsmHeaderBytes) {
            body.assign(p + kCsmHeaderBytes, p + total_len);
        }
        frames_out.push_back(Frame(msg_id, body));
        consumed += total_len;
    }

    if (consumed > 0) {
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<ptrdiff_t>(consumed));
    }
    return true;
}

bool CsmFramer::encodeFrame(const Frame& frame,
                            std::vector<uint8_t>& out,
                            std::string* error_out) {
    const size_t total_len = kCsmHeaderBytes + frame.payload.size();
    if (total_len > kCsmMaxFrameBytes) {
        setError(error_out, "iAP2 CSM frame exceeds 65535 bytes");
        out.clear();
        return false;
    }
    out.clear();
    out.reserve(total_len);
    writeU16Be(out, kCsmStartMarker);
    writeU16Be(out, static_cast<uint16_t>(total_len));
    writeU16Be(out, frame.message_id);
    out.insert(out.end(), frame.payload.begin(), frame.payload.end());
    return true;
}

bool CsmFramer::splitForLink(const std::vector<uint8_t>& encoded_frame,
                             size_t max_packet_bytes,
                             std::vector<std::vector<uint8_t> >& packets_out,
                             std::string* error_out) {
    packets_out.clear();
    if (max_packet_bytes == 0) {
        setError(error_out, "max_packet_bytes must be > 0");
        return false;
    }
    if (encoded_frame.empty()) {
        return true;
    }
    size_t offset = 0;
    while (offset < encoded_frame.size()) {
        size_t chunk = encoded_frame.size() - offset;
        if (chunk > max_packet_bytes) {
            chunk = max_packet_bytes;
        }
        packets_out.push_back(std::vector<uint8_t>(
            encoded_frame.begin() + static_cast<ptrdiff_t>(offset),
            encoded_frame.begin() + static_cast<ptrdiff_t>(offset + chunk)));
        offset += chunk;
    }
    return true;
}

bool buildStartCarPlaySession(const StartSessionConfig& cfg,
                              Frame& frame_out,
                              std::string* error_out) {
    ParameterList root;

    if (!cfg.wired_ipv6_addresses.empty() || cfg.has_wired_reserved) {
        ParameterList wired;
        for (size_t i = 0; i < cfg.wired_ipv6_addresses.size(); ++i) {
            wired.addString(0, cfg.wired_ipv6_addresses[i]);
        }
        if (cfg.has_wired_reserved) {
            wired.addU32(1, cfg.wired_reserved);
        }
        root.addGroup(0, wired);
    }

    if (!cfg.wireless_ssid.empty() || !cfg.wireless_passphrase.empty() ||
        cfg.has_wireless_passphrase || cfg.has_wireless_channel ||
        !cfg.wireless_ips.empty() || cfg.has_wireless_security_type) {
        ParameterList wireless;
        if (!cfg.wireless_ssid.empty()) {
            wireless.addString(0, cfg.wireless_ssid);
        }
        const bool emit_empty_open_pass =
            cfg.has_wireless_security_type &&
            cfg.wireless_security_type == kWirelessSecurityNone &&
            !cfg.wireless_ssid.empty();
        if (!cfg.wireless_passphrase.empty() || cfg.has_wireless_passphrase || emit_empty_open_pass) {
            wireless.addString(1, cfg.wireless_passphrase);
        }
        if (cfg.has_wireless_channel) {
            wireless.addU8(2, cfg.wireless_channel);
        }
        for (size_t i = 0; i < cfg.wireless_ips.size(); ++i) {
            wireless.addString(3, cfg.wireless_ips[i]);
        }
        if (cfg.has_wireless_security_type) {
            wireless.addU8(4, cfg.wireless_security_type);
        }
        root.addGroup(1, wireless);
    }

    if (cfg.has_airplay_port) {
        root.addU32(2, cfg.airplay_port);
    }
    if (!cfg.device_identifier.empty()) {
        root.addString(3, cfg.device_identifier);
    }
    if (!cfg.public_key.empty()) {
        root.addString(4, cfg.public_key);
    }
    if (!cfg.source_version.empty()) {
        root.addString(5, cfg.source_version);
    }
    if (!cfg.sdk_version.empty()) {
        root.addString(6, cfg.sdk_version);
    }
    if (!cfg.cluster_asset_id.empty() || cfg.has_cluster_asset_version) {
        ParameterList cluster;
        if (!cfg.cluster_asset_id.empty()) {
            cluster.addString(0, cfg.cluster_asset_id);
        }
        if (cfg.has_cluster_asset_version) {
            cluster.addU32(1, cfg.cluster_asset_version);
        }
        root.addGroup(7, cluster);
    }
    if (cfg.has_mutual_auth) {
        root.addU8(8, cfg.mutual_auth);
    }

    return Frame::build(msg::kCarPlayStartSession, root, frame_out, error_out);
}

bool parseCarPlayAvailability(const Frame& frame,
                              CarPlayAvailability& out,
                              std::string* error_out) {
    if (frame.message_id != msg::kCarPlayAvailability) {
        setError(error_out, "expected CarPlayAvailability (0x4300)");
        return false;
    }
    ParameterList body;
    if (!ParameterList::decode(frame.payload, body, error_out)) {
        return false;
    }

    out = CarPlayAvailability();

    if (body.findFirst(0) != NULL) {
        ParameterList grp;
        if (!body.readGroup(0, grp, error_out)) return false;
        out.wired.present = true;
        if (!decodeBinaryFlag(grp, 0, "wired", out.wired.has_available, out.wired.available, error_out)) {
            return false;
        }
        out.wired.has_identifier = grp.readString(1, out.wired.identifier);
    }

    if (body.findFirst(1) != NULL) {
        ParameterList grp;
        if (!body.readGroup(1, grp, error_out)) return false;
        out.wireless.present = true;
        if (!decodeBinaryFlag(grp, 0, "wireless", out.wireless.has_available, out.wireless.available, error_out)) {
            return false;
        }
        out.wireless.has_identifier = grp.readString(1, out.wireless.identifier);
    }

    if (body.findFirst(2) != NULL) {
        ParameterList grp;
        if (!body.readGroup(2, grp, error_out)) return false;
        out.theme_assets.present = true;
        if (!decodeBinaryFlag(grp, 0, "themeAssets", out.theme_assets.has_available, out.theme_assets.available, error_out)) {
            return false;
        }
    }

    return true;
}

bool buildAccessoryWiFiConfiguration(const std::string& ssid,
                                     const std::string& passphrase,
                                     uint8_t channel,
                                     uint8_t security_type,
                                     const std::vector<uint8_t>* bssid,
                                     Frame& frame_out,
                                     std::string* error_out) {
    ParameterList root;
    if (bssid != NULL && !bssid->empty()) {
        root.addRaw(0, *bssid);
    }
    root.addString(1, ssid);
    root.addString(2, passphrase);
    root.addU8(3, security_type);
    root.addU8(4, channel);
    return Frame::build(msg::kAccessoryWiFiConfiguration, root, frame_out, error_out);
}

bool parseWiFiInformation(const Frame& frame,
                          WiFiInformation& out,
                          std::string* error_out) {
    if (frame.message_id != msg::kWiFiInformation) {
        setError(error_out, "expected WiFiInformation (0x5701)");
        return false;
    }
    ParameterList body;
    if (!ParameterList::decode(frame.payload, body, error_out)) {
        return false;
    }
    out = WiFiInformation();
    if (!body.readU8(0, out.status)) {
        setError(error_out, "WiFiInformation missing status parameter 0");
        return false;
    }
    if (out.status != 0) {
        return true;
    }
    out.has_security_type = body.readU8(1, out.security_type);
    out.has_ssid = body.readString(2, out.ssid);
    out.has_passphrase = body.readString(3, out.passphrase);
    return true;
}

bool parseWirelessCarPlayAvailability(const Frame& frame,
                                      bool& available_out,
                                      std::string* error_out) {
    if (frame.message_id != msg::kWirelessCarPlayUpdate) {
        setError(error_out, "expected WirelessCarPlayUpdate (0x4e0d)");
        return false;
    }
    ParameterList body;
    if (!ParameterList::decode(frame.payload, body, error_out)) {
        return false;
    }
    uint8_t val = 0;
    if (!body.readU8(0, val)) {
        setError(error_out, "WirelessCarPlayUpdate missing u8 param 0");
        return false;
    }
    if (val > 1) {
        setError(error_out, "WirelessCarPlayUpdate availability must be 0 or 1");
        return false;
    }
    available_out = (val == 1);
    return true;
}

bool parseDeviceTransportIdentifier(const Frame& frame,
                                    DeviceTransportIdentifier& out,
                                    std::string* error_out) {
    if (frame.message_id != msg::kDeviceTransportIdentifier) {
        setError(error_out, "expected DeviceTransportIdentifierNotification (0x4e0e)");
        return false;
    }
    ParameterList body;
    if (!ParameterList::decode(frame.payload, body, error_out)) {
        return false;
    }
    out = DeviceTransportIdentifier();
    out.has_bluetooth_mac = body.readString(0, out.bluetooth_mac);
    out.has_usb_transport_id = body.readString(1, out.usb_transport_id);
    return true;
}

bool buildPowerSourceUpdate(uint16_t available_current_ma,
                            bool has_charging,
                            bool charging,
                            Frame& frame_out,
                            std::string* error_out) {
    ParameterList root;
    root.addU16(0, available_current_ma);
    if (has_charging) {
        root.addU8(1, charging ? 1U : 0U);
    }
    return Frame::build(msg::kPowerSourceUpdate, root, frame_out, error_out);
}

bool buildLocationInformation(const std::string& nmea_sentence,
                              Frame& frame_out,
                              std::string* error_out) {
    if (nmea_sentence.empty()) {
        setError(error_out, "NMEA sentence must not be empty");
        return false;
    }
    ParameterList root;
    root.addString(0, nmea_sentence);
    return Frame::build(msg::kLocationInformation, root, frame_out, error_out);
}

bool buildStartNowPlayingUpdates(Frame& frame_out, std::string* error_out) {
    ParameterList media_attrs;
    media_attrs.addVoid(1);
    media_attrs.addVoid(4);
    media_attrs.addVoid(6);
    media_attrs.addVoid(12);
    media_attrs.addVoid(26);

    ParameterList playback_attrs;
    playback_attrs.addVoid(0);
    playback_attrs.addVoid(1);
    playback_attrs.addVoid(7);

    ParameterList root;
    root.addGroup(0, media_attrs);
    root.addGroup(1, playback_attrs);
    return Frame::build(msg::kStartNowPlayingUpdates, root, frame_out, error_out);
}

bool buildStartRouteGuidanceUpdates(uint16_t component_id,
                                    Frame& frame_out,
                                    std::string* error_out) {
    ParameterList root;
    root.addU16(0, component_id);
    root.addVoid(1);
    root.addVoid(2);
    return Frame::build(msg::kStartRouteGuidanceUpdates, root, frame_out, error_out);
}

bool buildStartPowerUpdates(Frame& frame_out, std::string* error_out) {
    ParameterList root;
    root.addVoid(4);
    root.addVoid(5);
    root.addVoid(6);
    return Frame::build(msg::kStartPowerUpdates, root, frame_out, error_out);
}

bool buildStartCommunicationsUpdates(Frame& frame_out, std::string* error_out) {
    ParameterList root;
    root.addVoid(0);
    root.addVoid(4);
    root.addVoid(5);
    return Frame::build(msg::kStartCommunicationsUpdates, root, frame_out, error_out);
}

bool buildStartCallStateUpdates(Frame& frame_out, std::string* error_out) {
    ParameterList root;
    root.addVoid(0);
    root.addVoid(1);
    root.addVoid(2);
    root.addVoid(3);
    root.addVoid(4);
    root.addVoid(11);
    return Frame::build(msg::kStartCallStateUpdates, root, frame_out, error_out);
}

bool buildStandardSubscriptions(std::vector<Frame>& frames_out, std::string* error_out) {
    frames_out.clear();
    Frame f;
    if (!buildStartNowPlayingUpdates(f, error_out)) return false;
    frames_out.push_back(f);
    if (!buildStartRouteGuidanceUpdates(42U, f, error_out)) return false;
    frames_out.push_back(f);
    if (!buildStartPowerUpdates(f, error_out)) return false;
    frames_out.push_back(f);
    if (!buildStartCommunicationsUpdates(f, error_out)) return false;
    frames_out.push_back(f);
    if (!buildStartCallStateUpdates(f, error_out)) return false;
    frames_out.push_back(f);
    return true;
}

}  // namespace iap2
}  // namespace carplay
