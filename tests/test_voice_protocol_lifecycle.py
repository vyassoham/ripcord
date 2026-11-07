"""
Unit Test Suite: Discord Voice Gateway v8/v9 Protocol & State Machine Lifecycle
Tests Voice Identify (Op 0) with DAVE, Ready (Op 2), Session Description (Op 4),
Heartbeat (Op 3) seq_ack, binary opcode framing, and session cleanup.
"""

import json
import struct
import unittest

# Discord Voice Opcodes
VOICE_OP_IDENTIFY = 0
VOICE_OP_SELECT_PROTOCOL = 1
VOICE_OP_READY = 2
VOICE_OP_HEARTBEAT = 3
VOICE_OP_SESSION_DESCRIPTION = 4
VOICE_OP_SPEAKING = 5
VOICE_OP_HEARTBEAT_ACK = 6
VOICE_OP_RESUME = 7
VOICE_OP_HELLO = 8
VOICE_OP_RESUMED = 9
VOICE_OP_CLIENT_DISCONNECT = 13
VOICE_OP_DAVE_PREPARE_TRANSITION = 21
VOICE_OP_DAVE_EXECUTE_TRANSITION = 22
VOICE_OP_DAVE_TRANSITION_READY = 23
VOICE_OP_DAVE_PREPARE_EPOCH = 24

# DAVE Binary Opcodes
DAVE_MLS_EXTERNAL_SENDER = 25
DAVE_MLS_KEY_PACKAGE = 26
DAVE_MLS_PROPOSALS = 27
DAVE_MLS_COMMIT_WELCOME = 28
DAVE_MLS_ANNOUNCE_COMMIT = 29
DAVE_MLS_WELCOME = 30
DAVE_MLS_INVALID_COMMIT = 31

class MockVoiceClient:
    """Simulates client-side voice connection state."""
    def __init__(self, server_id=1001, user_id=2002, session_id="test_sess", token="secret_tok"):
        self.server_id = server_id
        self.user_id = user_id
        self.session_id = session_id
        self.token = token
        self.state = "DISCONNECTED"
        self.last_seq = -1
        self.ssrc = 0
        self.encryption_mode = None
        self.dave_enabled = False
        self.dave_protocol_version = 0
        self.pending_transition_id = -1
        self.sent_messages = []

    def connect(self):
        self.state = "CONNECTING"
        # Generate Op 0 Identify
        identify_payload = {
            "op": VOICE_OP_IDENTIFY,
            "d": {
                "server_id": str(self.server_id),
                "user_id": str(self.user_id),
                "session_id": self.session_id,
                "token": self.token,
                "video": True,
                "max_dave_protocol_version": 1,
            }
        }
        self.sent_messages.append(identify_payload)

    def handle_text_message(self, raw_json: str):
        msg = json.loads(raw_json)
        op = msg.get("op")
        seq = msg.get("seq")
        d = msg.get("d", {})

        if seq is not None and seq >= 0:
            self.last_seq = seq

        if op == VOICE_OP_READY:
            self.state = "READY"
            self.ssrc = d.get("ssrc")
        elif op == VOICE_OP_SESSION_DESCRIPTION:
            self.state = "CONNECTED"
            self.encryption_mode = d.get("mode")
            dave_ver = d.get("dave_protocol_version", 0)
            if dave_ver > 0:
                self.dave_enabled = True
                self.dave_protocol_version = dave_ver
        elif op == VOICE_OP_DAVE_PREPARE_TRANSITION:
            self.pending_transition_id = d.get("transition_id", 0)
            # Send Ready for Transition (Op 23)
            self.sent_messages.append({
                "op": VOICE_OP_DAVE_TRANSITION_READY,
                "d": {"transition_id": self.pending_transition_id}
            })
        elif op == VOICE_OP_DAVE_EXECUTE_TRANSITION:
            if d.get("transition_id") == self.pending_transition_id:
                self.state = "DAVE_ACTIVE"

    def handle_binary_message(self, raw_bytes: bytes):
        """Processes binary framing from Voice Gateway: 2 bytes seq + 1 byte opcode + payload."""
        if len(raw_bytes) < 3:
            return False
        seq = (raw_bytes[0] << 8) | raw_bytes[1]
        self.last_seq = seq
        opcode = raw_bytes[2]
        payload = raw_bytes[3:]
        return opcode, payload

    def create_heartbeat(self, now_ms: int):
        """Generates Op 3 Heartbeat including required seq_ack."""
        return {
            "op": VOICE_OP_HEARTBEAT,
            "d": {
                "t": now_ms,
                "seq_ack": self.last_seq
            }
        }

    def disconnect(self):
        self.state = "DISCONNECTED"
        self.last_seq = -1
        self.ssrc = 0
        self.dave_enabled = False
        self.pending_transition_id = -1

class TestVoiceProtocolLifecycle(unittest.TestCase):
    def test_01_identify_advertises_dave_protocol(self):
        """Voice Identify Op 0 must include max_dave_protocol_version: 1 and video: true."""
        client = MockVoiceClient()
        client.connect()
        self.assertEqual(len(client.sent_messages), 1)
        ident = client.sent_messages[0]
        self.assertEqual(ident["op"], 0)
        self.assertEqual(ident["d"]["max_dave_protocol_version"], 1)
        self.assertTrue(ident["d"]["video"])

    def test_02_ready_and_session_description_handshake(self):
        """Validates transition from Ready to Connected with AEAD xchacha20 encryption mode."""
        client = MockVoiceClient()
        client.connect()

        # Gateway sends Op 2 (Ready)
        ready_payload = json.dumps({
            "op": VOICE_OP_READY,
            "seq": 1,
            "d": {
                "ssrc": 123456,
                "ip": "1.2.3.4",
                "port": 50000,
                "modes": ["aead_xchacha20_poly1305_rtpsize"]
            }
        })
        client.handle_text_message(ready_payload)
        self.assertEqual(client.state, "READY")
        self.assertEqual(client.ssrc, 123456)
        self.assertEqual(client.last_seq, 1)

        # Gateway sends Op 4 (Session Description)
        sd_payload = json.dumps({
            "op": VOICE_OP_SESSION_DESCRIPTION,
            "seq": 2,
            "d": {
                "mode": "aead_xchacha20_poly1305_rtpsize",
                "dave_protocol_version": 1,
                "secret_key": [0] * 32
            }
        })
        client.handle_text_message(sd_payload)
        self.assertEqual(client.state, "CONNECTED")
        self.assertEqual(client.encryption_mode, "aead_xchacha20_poly1305_rtpsize")
        self.assertTrue(client.dave_enabled)
        self.assertEqual(client.last_seq, 2)

    def test_03_heartbeat_reports_acknowledged_sequence(self):
        """Heartbeat (Op 3) must include seq_ack matching the latest gateway sequence."""
        client = MockVoiceClient()
        client.last_seq = 42

        hb = client.create_heartbeat(1700000000000)
        self.assertEqual(hb["op"], 3)
        self.assertEqual(hb["d"]["seq_ack"], 42)
        self.assertEqual(hb["d"]["t"], 1700000000000)

    def test_04_binary_message_framing_and_sequence_tracking(self):
        """Binary gateway frames must extract big-endian 16-bit sequence and 8-bit opcode."""
        client = MockVoiceClient()
        # Binary frame: seq=0x0105 (261), opcode=29 (ANNOUNCE_COMMIT), payload=b"\x00\x01data"
        raw_frame = struct.pack(">HB", 261, DAVE_MLS_ANNOUNCE_COMMIT) + b"\x00\x01commit_bytes"
        opcode, payload = client.handle_binary_message(raw_frame)

        self.assertEqual(client.last_seq, 261)
        self.assertEqual(opcode, DAVE_MLS_ANNOUNCE_COMMIT)
        self.assertEqual(payload, b"\x00\x01commit_bytes")

        # Heartbeat immediately following binary frame must report 261
        hb = client.create_heartbeat(1700000001000)
        self.assertEqual(hb["d"]["seq_ack"], 261)

    def test_05_dave_transition_ready_ack(self):
        """When Gateway issues Prepare Transition (Op 21), client must respond with Op 23."""
        client = MockVoiceClient()
        client.handle_text_message(json.dumps({
            "op": VOICE_OP_DAVE_PREPARE_TRANSITION,
            "seq": 10,
            "d": {"protocol_version": 1, "transition_id": 99}
        }))

        self.assertEqual(client.pending_transition_id, 99)
        self.assertEqual(len(client.sent_messages), 1)
        resp = client.sent_messages[0]
        self.assertEqual(resp["op"], VOICE_OP_DAVE_TRANSITION_READY)
        self.assertEqual(resp["d"]["transition_id"], 99)

    def test_06_disconnect_resets_state_cleanly(self):
        """Disconnect must reset last_seq to -1 and clear SSRC and transition IDs."""
        client = MockVoiceClient()
        client.last_seq = 500
        client.ssrc = 9999
        client.dave_enabled = True

        client.disconnect()
        self.assertEqual(client.state, "DISCONNECTED")
        self.assertEqual(client.last_seq, -1)
        self.assertEqual(client.ssrc, 0)
        self.assertFalse(client.dave_enabled)

if __name__ == "__main__":
    unittest.main()
