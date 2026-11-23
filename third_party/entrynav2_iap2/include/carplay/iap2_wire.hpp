#ifndef CARPLAY_IAP2_WIRE_HPP
#define CARPLAY_IAP2_WIRE_HPP

// Derived from DiPlay
// (shared/src/main/java/com/shilapi/xcertplay/iap2/catalog/Iap2Endpoint.kt,
//  shared/src/main/java/com/shilapi/xcertplay/iap2/message/Iap2Messages.kt,
//  shared/src/main/java/com/shilapi/xcertplay/iap2/wire/Iap2CsmFramer.kt,
//  shared/src/main/java/com/shilapi/xcertplay/iap2/wire/Iap2WireCodec.kt,
//  shared/src/main/java/com/shilapi/xcertplay/transport/Iap2VehicleStatus.kt),
// GPL-3.0, Copyright (c) DiPlay contributors.

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace carplay {
namespace iap2 {

static const uint16_t kCsmStartMarker = 0x4040U;   // verified: DiPlay Iap2CsmFramer.kt
static const size_t kCsmHeaderBytes = 6U;          // verified: DiPlay Iap2CsmFramer.kt
static const size_t kCsmMaxFrameBytes = 0xffffU;   // verified: DiPlay Iap2CsmFramer.kt:157
static const size_t kParamHeaderBytes = 4U;        // verified: DiPlay Iap2Wire.kt

/// iAP2 Control Session Message (CSM) endpoint IDs.
namespace msg {
// Identification (verified: DiPlay Iap2Endpoint.kt:70-177)
static const uint16_t kStartIdentification = 0x1d00U;
static const uint16_t kIdentificationInformation = 0x1d01U;
static const uint16_t kIdentificationAccepted = 0x1d02U;
static const uint16_t kIdentificationRejected = 0x1d03U;
static const uint16_t kCancelIdentification = 0x1d05U;
static const uint16_t kIdentificationInformationUpdate = 0x1d06U;

// Accessory Authentication (verified: DiPlay Iap2Endpoint.kt:179-223)
static const uint16_t kRequestAuthenticationCertificate = 0xaa00U;
static const uint16_t kAuthenticationCertificate = 0xaa01U;
static const uint16_t kRequestAuthenticationChallengeResponse = 0xaa02U;
static const uint16_t kAuthenticationResponse = 0xaa03U;
static const uint16_t kAuthenticationFailed = 0xaa04U;
static const uint16_t kAuthenticationSucceeded = 0xaa05U;
static const uint16_t kAuthenticationCertificateSerial = 0xaa06U;

// CarPlay Connection Request (verified: DiPlay Iap2Endpoint.kt:260-337)
static const uint16_t kCarPlayAvailability = 0x4300U;
static const uint16_t kCarPlayStartSession = 0x4301U;

// Wi-Fi Sharing (verified: DiPlay Iap2Endpoint.kt:351-387)
static const uint16_t kRequestWiFiInformation = 0x5700U;
static const uint16_t kWiFiInformation = 0x5701U;
static const uint16_t kRequestAccessoryWiFiConfiguration = 0x5702U;
static const uint16_t kAccessoryWiFiConfiguration = 0x5703U;

// Communications / Call State (verified: DiPlay Iap2Endpoint.kt:459-482,646-647)
static const uint16_t kStartCallStateUpdates = 0x4154U;
static const uint16_t kCallStateUpdate = 0x4155U;
static const uint16_t kStopCallStateUpdates = 0x4156U;
static const uint16_t kStartCommunicationsUpdates = 0x4157U;
static const uint16_t kCommunicationsUpdate = 0x4158U;
static const uint16_t kStopCommunicationsUpdates = 0x4159U;

// Device Notifications & Wireless CarPlay (verified: DiPlay Iap2Endpoint.kt:484-532)
static const uint16_t kDeviceInformationUpdate = 0x4e09U;
static const uint16_t kDeviceLanguageUpdate = 0x4e0aU;
static const uint16_t kDeviceTimeUpdate = 0x4e0bU;
static const uint16_t kDeviceUuidUpdate = 0x4e0cU;
static const uint16_t kWirelessCarPlayUpdate = 0x4e0dU;
static const uint16_t kDeviceTransportIdentifier = 0x4e0eU;

// Now Playing (verified: DiPlay Iap2Endpoint.kt:447-458, Iap2IdentificationClient.kt:435)
static const uint16_t kStartNowPlayingUpdates = 0x5000U;
static const uint16_t kNowPlayingUpdate = 0x5001U;
static const uint16_t kStopNowPlayingUpdates = 0x5002U;

// Route Guidance / Navigation (verified: DiPlay Iap2Endpoint.kt:435-446,643-645)
static const uint16_t kStartRouteGuidanceUpdates = 0x5200U;
static const uint16_t kRouteGuidanceUpdate = 0x5201U;
static const uint16_t kRouteGuidanceManeuverUpdate = 0x5202U;
static const uint16_t kStopRouteGuidanceUpdates = 0x5203U;
static const uint16_t kLaneGuidanceInfoUpdate = 0x5204U;

// Vehicle Status (verified: DiPlay Iap2VehicleStatus.kt:47-49)
static const uint16_t kStartVehicleStatusUpdates = 0xa100U;
static const uint16_t kVehicleStatusUpdate = 0xa101U;
static const uint16_t kStopVehicleStatusUpdates = 0xa102U;

// Power (verified: DiPlay Iap2Endpoint.kt:415-433)
static const uint16_t kStartPowerUpdates = 0xae00U;       // ACCESSORY_TO_IPHONE
static const uint16_t kPowerUpdate = 0xae01U;             // IPHONE_TO_ACCESSORY
static const uint16_t kStopPowerUpdates = 0xae02U;        // ACCESSORY_TO_IPHONE
static const uint16_t kPowerSourceUpdate = 0xae03U;       // ACCESSORY_TO_IPHONE
static const uint16_t kAccessoryPowerUpdate = 0xae05U;    // ACCESSORY_TO_IPHONE
static const uint16_t kAccessoryResetBaseCurrent = 0xae06U; // ACCESSORY_TO_IPHONE

// External Accessory (verified: DiPlay Iap2Endpoint.kt:534-551,688)
static const uint16_t kExternalAccessoryOpenSession = 0xea00U;
static const uint16_t kExternalAccessoryCloseSession = 0xea01U;
static const uint16_t kExternalAccessoryRequestAppLaunch = 0xea02U;
static const uint16_t kExternalAccessoryStatusSession = 0xea03U;

// Location (verified: DiPlay Iap2Endpoint.kt:395-413)
static const uint16_t kStartLocationInformation = 0xfffaU;
static const uint16_t kLocationInformation = 0xfffbU;
static const uint16_t kStopLocationInformation = 0xfffcU;
}  // namespace msg

/// Wireless 0x5703 / 0x4301 Wi-Fi security values (verified: DiPlay Iap2WirelessControlClient.kt:290-296).
enum WirelessSecurity {
    kWirelessSecurityNone = 0,
    kWirelessSecurityWep = 1,
    kWirelessSecurityWpaWpa2 = 2,
    kWirelessSecurityWpa3Transition = 3,
    kWirelessSecurityWpa3Only = 4
};

struct Parameter {
    uint16_t id;
    std::vector<uint8_t> payload;

    Parameter() : id(0) {}
    Parameter(uint16_t param_id, const std::vector<uint8_t>& data)
        : id(param_id), payload(data) {}
};

class ParameterList {
public:
    void clear() { params_.clear(); }
    bool empty() const { return params_.empty(); }
    size_t size() const { return params_.size(); }
    const std::vector<Parameter>& items() const { return params_; }

    void addRaw(uint16_t id, const std::vector<uint8_t>& payload);
    void addVoid(uint16_t id);
    void addU8(uint16_t id, uint8_t value);
    void addI8(uint16_t id, int8_t value);
    void addBool(uint16_t id, bool value);
    void addU16(uint16_t id, uint16_t value);
    void addI16(uint16_t id, int16_t value);
    void addU32(uint16_t id, uint32_t value);
    void addI32(uint16_t id, int32_t value);
    void addU64(uint16_t id, uint64_t value);
    void addString(uint16_t id, const std::string& value);
    void addStrings(uint16_t id, const std::vector<std::string>& values);
    void addU16List(uint16_t id, const std::vector<uint16_t>& values);
    void addGroup(uint16_t id, const ParameterList& group);

    const Parameter* findFirst(uint16_t id) const;
    std::vector<const Parameter*> findAll(uint16_t id) const;

    bool readU8(uint16_t id, uint8_t& out) const;
    bool readI8(uint16_t id, int8_t& out) const;
    bool readBool(uint16_t id, bool& out) const;
    bool readU16(uint16_t id, uint16_t& out) const;
    bool readI16(uint16_t id, int16_t& out) const;
    bool readU32(uint16_t id, uint32_t& out) const;
    bool readI32(uint16_t id, int32_t& out) const;
    bool readU64(uint16_t id, uint64_t& out) const;
    bool readBytes(uint16_t id, std::vector<uint8_t>& out) const;
    bool readString(uint16_t id, std::string& out) const;
    bool readStrings(uint16_t id, std::vector<std::string>& out) const;
    bool readU16List(uint16_t id, std::vector<uint16_t>& out) const;
    bool readGroup(uint16_t id, ParameterList& group_out, std::string* error_out = NULL) const;

    bool encode(std::vector<uint8_t>& out, std::string* error_out = NULL) const;

    static bool decode(const uint8_t* data,
                       size_t len,
                       ParameterList& list_out,
                       std::string* error_out = NULL);
    static bool decode(const std::vector<uint8_t>& bytes,
                       ParameterList& list_out,
                       std::string* error_out = NULL);

private:
    std::vector<Parameter> params_;
};

struct Frame {
    uint16_t message_id;
    std::vector<uint8_t> payload;

    Frame() : message_id(0) {}
    Frame(uint16_t id, const std::vector<uint8_t>& body)
        : message_id(id), payload(body) {}

    bool encode(std::vector<uint8_t>& out, std::string* error_out = NULL) const;
    static bool build(uint16_t message_id,
                      const ParameterList& params,
                      Frame& frame_out,
                      std::string* error_out = NULL);
};

/// Stateful iAP2 Control Session Message (0x4040) framer for USB/BT byte streams.
class CsmFramer {
public:
    void reset();
    size_t bufferedBytes() const { return buffer_.size(); }

    bool push(const uint8_t* data,
              size_t len,
              std::vector<Frame>& frames_out,
              std::string* error_out = NULL);

    static bool encodeFrame(const Frame& frame,
                            std::vector<uint8_t>& out,
                            std::string* error_out = NULL);

    static bool splitForLink(const std::vector<uint8_t>& encoded_frame,
                             size_t max_packet_bytes,
                             std::vector<std::vector<uint8_t> >& packets_out,
                             std::string* error_out = NULL);

private:
    std::vector<uint8_t> buffer_;
};

struct AvailabilityState {
    bool present;
    bool has_available;
    bool available;
    bool has_identifier;
    std::string identifier;

    AvailabilityState()
        : present(false), has_available(false), available(false), has_identifier(false) {}
};

struct CarPlayAvailability {
    AvailabilityState wired;
    AvailabilityState wireless;
    AvailabilityState theme_assets;
};

struct StartSessionConfig {
    // Wired (USB NCM IPv6)
    std::vector<std::string> wired_ipv6_addresses;
    bool has_wired_reserved;
    uint32_t wired_reserved;

    // Wireless (Wi-Fi AP / STA)
    std::string wireless_ssid;
    std::string wireless_passphrase;
    bool has_wireless_passphrase;
    bool has_wireless_channel;
    uint8_t wireless_channel;
    std::vector<std::string> wireless_ips;
    bool has_wireless_security_type;
    uint8_t wireless_security_type;

    // Common AirPlay endpoint fields
    bool has_airplay_port;
    uint32_t airplay_port;
    std::string device_identifier;
    std::string public_key;
    std::string source_version;
    std::string sdk_version;
    std::string cluster_asset_id;
    bool has_cluster_asset_version;
    uint32_t cluster_asset_version;
    bool has_mutual_auth;
    uint8_t mutual_auth;

    StartSessionConfig()
        : has_wired_reserved(false),
          wired_reserved(0),
          has_wireless_passphrase(false),
          has_wireless_channel(false),
          wireless_channel(0),
          has_wireless_security_type(false),
          wireless_security_type(0),
          has_airplay_port(false),
          airplay_port(7000),
          has_cluster_asset_version(false),
          cluster_asset_version(0),
          has_mutual_auth(false),
          mutual_auth(0) {}
};

struct WiFiInformation {
    uint8_t status;
    bool has_security_type;
    uint8_t security_type;
    bool has_ssid;
    std::string ssid;
    bool has_passphrase;
    std::string passphrase;

    WiFiInformation()
        : status(0), has_security_type(false), security_type(0), has_ssid(false), has_passphrase(false) {}
    bool ok() const { return status == 0; }
};

struct DeviceTransportIdentifier {
    bool has_bluetooth_mac;
    std::string bluetooth_mac;
    bool has_usb_transport_id;
    std::string usb_transport_id;

    DeviceTransportIdentifier() : has_bluetooth_mac(false), has_usb_transport_id(false) {}
};

bool buildStartCarPlaySession(const StartSessionConfig& cfg,
                              Frame& frame_out,
                              std::string* error_out = NULL);

bool parseCarPlayAvailability(const Frame& frame,
                              CarPlayAvailability& out,
                              std::string* error_out = NULL);

bool buildAccessoryWiFiConfiguration(const std::string& ssid,
                                     const std::string& passphrase,
                                     uint8_t channel,
                                     uint8_t security_type,
                                     const std::vector<uint8_t>* bssid,
                                     Frame& frame_out,
                                     std::string* error_out = NULL);

bool parseWiFiInformation(const Frame& frame,
                          WiFiInformation& out,
                          std::string* error_out = NULL);

bool parseWirelessCarPlayAvailability(const Frame& frame,
                                      bool& available_out,
                                      std::string* error_out = NULL);

bool parseDeviceTransportIdentifier(const Frame& frame,
                                    DeviceTransportIdentifier& out,
                                    std::string* error_out = NULL);

bool buildPowerSourceUpdate(uint16_t available_current_ma,
                            bool has_charging,
                            bool charging,
                            Frame& frame_out,
                            std::string* error_out = NULL);

bool buildLocationInformation(const std::string& nmea_sentence,
                              Frame& frame_out,
                              std::string* error_out = NULL);

// Standard DiPlay control subscription builders (verified: DiPlay Iap2Messages.kt:278-313).
bool buildStartNowPlayingUpdates(Frame& frame_out, std::string* error_out = NULL);
bool buildStartRouteGuidanceUpdates(uint16_t component_id,
                                    Frame& frame_out,
                                    std::string* error_out = NULL);
bool buildStartPowerUpdates(Frame& frame_out, std::string* error_out = NULL);
bool buildStartCommunicationsUpdates(Frame& frame_out, std::string* error_out = NULL);
bool buildStartCallStateUpdates(Frame& frame_out, std::string* error_out = NULL);
bool buildStandardSubscriptions(std::vector<Frame>& frames_out, std::string* error_out = NULL);

}  // namespace iap2
}  // namespace carplay

#endif  // CARPLAY_IAP2_WIRE_HPP
