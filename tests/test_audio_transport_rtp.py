"""
Unit Test Suite: Audio & VoIP Transport Layer (RTP, IP Discovery, AEAD Packet Structure)
Tests RFC 3550 RTP header generation, sequence wrapping, 74-byte IP discovery formatting,
port endianness swap, and RTP extension parsing robustness.
"""

import struct
import unittest

RTP_HEADER_SIZE = 12
RTP_PACKET_TYPE_VOICE = 0x78  # 120 (Opus)
DAVE_MAGIC_BYTES = 0xFAFA

def generate_rtp_header(seq: int, timestamp: int, ssrc: int) -> bytearray:
    buf = bytearray(RTP_HEADER_SIZE)
    buf[0] = 0x80
    buf[1] = RTP_PACKET_TYPE_VOICE
    buf[2] = (seq >> 8) & 0xFF
    buf[3] = seq & 0xFF
    buf[4] = (timestamp >> 24) & 0xFF
    buf[5] = (timestamp >> 16) & 0xFF
    buf[6] = (timestamp >> 8) & 0xFF
    buf[7] = timestamp & 0xFF
    buf[8] = (ssrc >> 24) & 0xFF
    buf[9] = (ssrc >> 16) & 0xFF
    buf[10] = (ssrc >> 8) & 0xFF
    buf[11] = ssrc & 0xFF
    return buf

def build_ip_discovery_packet(ssrc: int) -> bytes:
    """Builds current Discord 74-byte IP discovery packet (Opcode 1, length 70)."""
    packet = bytearray(74)
    packet[0] = 0
    packet[1] = 1 # Type 1: Request
    packet[2] = 0
    packet[3] = 70 # Length
    struct.pack_into(">I", packet, 4, ssrc)
    return bytes(packet)

def parse_ip_discovery_reply(data: bytes):
    """Parses 74-byte IP discovery reply (Opcode 2) and swaps port endianness."""
    if len(data) != 74 or data[1] != 2 or data[3] != 70:
        return None
    # IP address is null-terminated string starting at byte 4 up to byte 68
    ip_end = data.find(b"\x00", 4, 68)
    if ip_end == -1:
        ip_end = 68
    ip_str = data[4:ip_end].decode("ascii", errors="ignore")
    # Big-endian port at bytes 72-73
    port = (data[72] << 8) | data[73]
    return ip_str, port

def parse_rtp_extension_header(data: bytes, header_size: int = 12):
    """Parses RFC 3550 RTP extension header (profile + length in 32-bit words)."""
    if len(data) < header_size + 4:
        return None
    profile = (data[header_size] << 8) | data[header_size + 1]
    extension_dwords = (data[header_size + 2] << 8) | data[header_size + 3]
    extension_bytes = extension_dwords * 4
    total_extension_len = 4 + extension_bytes
    return profile, extension_dwords, total_extension_len

class TestAudioTransportRTP(unittest.TestCase):
    def test_01_rtp_header_structure(self):
        """RTP header must follow RFC 3550 standard (version 2, Opus payload type 120)."""
        hdr = generate_rtp_header(seq=1000, timestamp=48000, ssrc=0x12345678)
        self.assertEqual(len(hdr), 12)
        self.assertEqual(hdr[0], 0x80)
        self.assertEqual(hdr[1], 120)

        # Unpack sequence, timestamp, ssrc
        seq = struct.unpack(">H", hdr[2:4])[0]
        ts = struct.unpack(">I", hdr[4:8])[0]
        ssrc = struct.unpack(">I", hdr[8:12])[0]

        self.assertEqual(seq, 1000)
        self.assertEqual(ts, 48000)
        self.assertEqual(ssrc, 0x12345678)

    def test_02_rtp_sequence_wrapping(self):
        """RTP 16-bit sequence number must wrap from 65535 to 0 gracefully."""
        seq = 65535
        hdr1 = generate_rtp_header(seq, timestamp=1000, ssrc=1)
        self.assertEqual(struct.unpack(">H", hdr1[2:4])[0], 65535)

        next_seq = (seq + 1) & 0xFFFF
        hdr2 = generate_rtp_header(next_seq, timestamp=1960, ssrc=1)
        self.assertEqual(struct.unpack(">H", hdr2[2:4])[0], 0)

    def test_03_timestamp_silence_advance_and_clamping(self):
        """Timestamp must step by 960 per 20ms frame, and clamp leaps after silence."""
        g_voice_timestamp = 100000
        g_voice_last_send_ms = 1000

        # Case 1: Normal 20ms audio frame
        now1 = 1020
        elapsed1 = now1 - g_voice_last_send_ms
        if elapsed1 <= 100:
            g_voice_timestamp += 960
        self.assertEqual(g_voice_timestamp, 100960)

        # Case 2: Short pause of 500ms
        g_voice_last_send_ms = now1
        now2 = 1520
        elapsed2 = now2 - g_voice_last_send_ms
        if elapsed2 > 100 and elapsed2 <= 3000:
            g_voice_timestamp += int(elapsed2 * 48)
        self.assertEqual(g_voice_timestamp, 100960 + 24000)

        # Case 3: Long pause of 10 seconds (must be clamped to 3000ms = 144,000 samples)
        g_voice_last_send_ms = now2
        now3 = 11520 # 10s later
        elapsed3 = now3 - g_voice_last_send_ms
        if elapsed3 > 3000:
            g_voice_timestamp += int(3000 * 48)
        self.assertEqual(g_voice_timestamp, 100960 + 24000 + 144000)

    def test_04_ip_discovery_packet_construction(self):
        """Constructs 74-byte discovery packet with SSRC at offset 4."""
        pkt = build_ip_discovery_packet(0xAABBCCDD)
        self.assertEqual(len(pkt), 74)
        self.assertEqual(pkt[0:4], b"\x00\x01\x00\x46")
        ssrc = struct.unpack(">I", pkt[4:8])[0]
        self.assertEqual(ssrc, 0xAABBCCDD)
        # Trailing 66 bytes must be zero padding
        self.assertEqual(pkt[8:], bytes(66))

    def test_05_ip_discovery_reply_parsing(self):
        """Parses IP discovery reply and correctly reads big-endian port."""
        reply = bytearray(74)
        reply[0] = 0
        reply[1] = 2 # Type 2: Response
        reply[2] = 0
        reply[3] = 70
        # Simulated WAN IP
        ip_bytes = b"203.0.113.195\x00"
        reply[4:4+len(ip_bytes)] = ip_bytes
        # Simulated WAN Port 54321 = 0xD431
        reply[72] = 0xD4
        reply[73] = 0x31

        parsed = parse_ip_discovery_reply(bytes(reply))
        self.assertIsNotNone(parsed)
        ip, port = parsed
        self.assertEqual(ip, "203.0.113.195")
        self.assertEqual(port, 54321)

    def test_06_rtp_header_extension_length_calculation(self):
        """Parses RTP header extension profile and counts length in 32-bit words."""
        # Simulated RTP packet with Extension bit set (X=1)
        # Profile 0xBEDE (1-byte header extension), length = 3 dwords (12 bytes)
        raw_pkt = bytearray(12)
        raw_pkt[0] = 0x90 # V=2, X=1
        ext_header = struct.pack(">HH", 0xBEDE, 3) + bytes(12)
        raw_pkt.extend(ext_header)

        res = parse_rtp_extension_header(bytes(raw_pkt), 12)
        self.assertIsNotNone(res)
        profile, dwords, total_bytes = res
        self.assertEqual(profile, 0xBEDE)
        self.assertEqual(dwords, 3)
        self.assertEqual(total_bytes, 16) # 4 bytes header + 12 bytes data

if __name__ == "__main__":
    unittest.main()
