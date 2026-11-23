//! R103: review-only transport boundary between original MU1005 R45 RFCOMM
//! wire frames and the exact, unmodified LuKa iap2-link engine.
//! The bridge DOES NOT register any BlueSDK service, establish Apple MFi,
//! control Wi-Fi or install anything on a vehicle.
use iap2_link::{LinkConfig, LinkEngine, LinkState};

pub const RF_MAX: usize = 5035;
pub const FRAME_HEADER: usize = 8;
pub const CONNECTED: u8 = 1;
pub const DATA: u8 = 2;
pub const DISCONNECTED: u8 = 3;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Frame {
    pub kind: u8,
    pub generation: u32,
    pub payload: Vec<u8>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BridgeError {
    Malformed,
    WrongGeneration,
    UnexpectedState,
    LinkDead,
    Terminated,
}

impl Frame {
    /// Strictly consumes one complete RFCOMM message from the pre-existing
    /// AU57x C bridge; no invented stream framing or alternate endian mode.
    pub fn decode(input: &[u8]) -> Result<Self, BridgeError> {
        if input.len() < FRAME_HEADER || input[1] != 0 {
            return Err(BridgeError::Malformed);
        }
        let kind = input[0];
        let len = u16::from_le_bytes([input[2], input[3]]) as usize;
        let generation = u32::from_le_bytes(input[4..8].try_into().unwrap());
        if generation == 0 || len > RF_MAX || input.len() != FRAME_HEADER + len {
            return Err(BridgeError::Malformed);
        }
        let payload = &input[FRAME_HEADER..];
        let valid = match kind {
            CONNECTED => len == 6 && payload.iter().any(|x| *x != 0)
                         && payload.iter().any(|x| *x != 0xff),
            DATA => len > 0,
            DISCONNECTED => len == 0,
            _ => false,
        };
        if !valid { return Err(BridgeError::Malformed); }
        Ok(Self { kind, generation, payload: payload.to_vec() })
    }

    pub fn encode(&self) -> Result<Vec<u8>, BridgeError> {
        if self.payload.len() > RF_MAX || self.generation == 0 {
            return Err(BridgeError::Malformed);
        }
        let mut out = Vec::with_capacity(FRAME_HEADER + self.payload.len());
        out.push(self.kind);
        out.push(0);
        out.extend_from_slice(&(self.payload.len() as u16).to_le_bytes());
        out.extend_from_slice(&self.generation.to_le_bytes());
        out.extend_from_slice(&self.payload);
        // Validate the exact bytes which will be handed back to the C bridge.
        Frame::decode(&out)?;
        Ok(out)
    }
}

pub struct ReviewBridge {
    generation: Option<u32>,
    link: Option<LinkEngine>,
    terminal: bool,
}

impl Default for ReviewBridge {
    fn default() -> Self { Self::new() }
}
impl ReviewBridge {
    pub fn new() -> Self { Self { generation: None, link: None, terminal: false } }
    pub fn is_terminal(&self) -> bool { self.terminal }
    pub fn link_state(&self) -> Option<LinkState> { self.link.as_ref().map(LinkEngine::state) }

    fn drain_link(&mut self) -> Result<Vec<Frame>, BridgeError> {
        let link = self.link.as_mut().ok_or(BridgeError::UnexpectedState)?;
        if link.state() == LinkState::Dead {
            self.terminal = true;
            return Err(BridgeError::LinkDead);
        }
        let bytes = link.take_output();
        let mut result = Vec::new();
        for chunk in bytes.chunks(RF_MAX) {
            if !chunk.is_empty() {
                result.push(Frame { kind: DATA, generation: self.generation.unwrap(), payload: chunk.to_vec() });
            }
        }
        Ok(result)
    }

    pub fn accept(&mut self, frame: Frame, now_ms: u64) -> Result<Vec<Frame>, BridgeError> {
        if self.terminal { return Err(BridgeError::Terminated); }
        if Frame::decode(&frame.encode()?)? != frame { return Err(BridgeError::Malformed); }
        match frame.kind {
            CONNECTED if self.link.is_none() => {
                self.generation = Some(frame.generation);
                let mut link = LinkEngine::new(LinkConfig::default());
                link.start(false, now_ms);
                self.link = Some(link);
                self.drain_link() // marker bytes, not MFi or CarPlay success
            }
            DATA if self.generation == Some(frame.generation) && self.link.is_some() => {
                self.link.as_mut().unwrap().feed(&frame.payload, now_ms);
                self.drain_link() // may emit SYN/LSP/ACK and link re-transmissions
            }
            DISCONNECTED if self.generation == Some(frame.generation) && self.link.is_some() => {
                self.link.as_mut().unwrap().feed_eof();
                self.terminal = true; // never reuse state for another phone/owner
                Ok(Vec::new())
            }
            CONNECTED | DATA | DISCONNECTED if self.generation.is_some() && self.generation != Some(frame.generation) => {
                Err(BridgeError::WrongGeneration)
            }
            _ => Err(BridgeError::UnexpectedState),
        }
    }

    /// Called by the host supervisor only.  Does not invent an OEM timer or
    /// resume a link after radio/socket disconnect.
    pub fn advance_time(&mut self, now_ms: u64) -> Result<Vec<Frame>, BridgeError> {
        if self.terminal { return Err(BridgeError::Terminated); }
        self.link.as_mut().ok_or(BridgeError::UnexpectedState)?.advance_time(now_ms);
        self.drain_link()
    }

    pub fn socket_lost(&mut self) {
        if let Some(link) = self.link.as_mut() { link.feed_eof(); }
        self.terminal = true;
    }
}

pub mod tcp_lab;
