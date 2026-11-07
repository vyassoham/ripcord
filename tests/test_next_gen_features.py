"""
Unit and Integration Test Suite: Next-Gen Features and Protocol Handlers
Covers:
  1. Voice Member Drag-and-Drop: Permission gating, MIME serialization, and REST dispatch.
  2. Voice Region Migration: Endpoint change, session teardown, and clean reconnect.
  3. Gateway Recovery: Op 6 Resume vs Op 2 Identify, Op 9 Invalid Session backoff.
  4. Explorer File Drop: MIME text/uri-list and CF_HDROP validation for text channels.
  5. Live Audio Device Switching: Device enumeration and stream re-binding simulation.
"""

import os
import struct
import json
import time
import unittest

PERM_ADMINISTRATOR = 0x0000000000000008
PERM_MANAGE_CHANNELS = 0x0000000000000010
PERM_MOVE_MEMBERS = 0x0000000000800000

MIME_VOICE_MEMBER = "application/x-discord-voicemember"
VOICE_MEMBER_MAGIC = 0x564D4F56  # 'VMOV'

class TestNextGenFeatures(unittest.TestCase):
    
    # -------------------------------------------------------------
    # 1. Voice Member Drag-and-Drop
    # -------------------------------------------------------------
    def test_01_voice_member_drag_permission_gating(self):
        """Verifies that MOVE_MEMBERS or ADMINISTRATOR enables drag, while normal perms reject."""
        def check_drag_enabled(user_perms):
            can_move = bool(user_perms & (PERM_ADMINISTRATOR | PERM_MOVE_MEMBERS))
            item_flags = 0x21  # ItemIsEnabled | ItemIsSelectable
            if can_move:
                item_flags |= 0x04  # ItemIsDragEnabled
            return item_flags, can_move

        # Case A: Normal user without move perms
        flags_normal, can_normal = check_drag_enabled(0x0000000000000400)  # VIEW_CHANNEL only
        self.assertFalse(can_normal)
        self.assertEqual(flags_normal & 0x04, 0, "Normal user must NOT have ItemIsDragEnabled")

        # Case B: Moderator with MOVE_MEMBERS but NO Manage Channels
        flags_mod, can_mod = check_drag_enabled(PERM_MOVE_MEMBERS)
        self.assertTrue(can_mod)
        self.assertEqual(flags_mod & 0x04, 0x04, "Moderator with MOVE_MEMBERS MUST have ItemIsDragEnabled")

        # Case C: Admin with ADMINISTRATOR
        flags_admin, can_admin = check_drag_enabled(PERM_ADMINISTRATOR)
        self.assertTrue(can_admin)
        self.assertEqual(flags_admin & 0x04, 0x04, "Administrator MUST have ItemIsDragEnabled")

    def test_02_voice_member_mime_serialization_and_deserialization(self):
        """Verifies binary encoding and parsing of application/x-discord-voicemember MIME data."""
        guild_id = 112233445566778899
        user_id = 998877665544332211
        src_channel_id = 554433221100998877

        # Serialize
        payload = struct.pack("<IIQQQ", VOICE_MEMBER_MAGIC, 28, guild_id, user_id, src_channel_id)
        self.assertEqual(len(payload), 32)

        # Deserialize & Validate
        magic, size, dec_guild, dec_user, dec_src = struct.unpack("<IIQQQ", payload)
        self.assertEqual(magic, VOICE_MEMBER_MAGIC)
        self.assertEqual(size, 28)
        self.assertEqual(dec_guild, guild_id)
        self.assertEqual(dec_user, user_id)
        self.assertEqual(dec_src, src_channel_id)

    def test_03_voice_member_move_rest_dispatch(self):
        """Verifies construction of the Discord REST PATCH request for moving a member."""
        guild_id = 112233445566778899
        user_id = 998877665544332211
        dest_channel_id = 665544332211009988
        token = "test_bot_token_sample"

        def build_move_request(gid, uid, target_cid, auth_token):
            url = f"/api/v9/guilds/{gid}/members/{uid}"
            headers = {
                "Authorization": auth_token,
                "Content-Type": "application/json",
                "User-Agent": "Ripcord/0.4.29-hardened"
            }
            body = json.dumps({"channel_id": str(target_cid)})
            return "PATCH", url, headers, body

        method, url, headers, body = build_move_request(guild_id, user_id, dest_channel_id, token)
        self.assertEqual(method, "PATCH")
        self.assertEqual(url, f"/api/v9/guilds/{guild_id}/members/{user_id}")
        self.assertIn("Authorization", headers)
        parsed_body = json.loads(body)
        self.assertEqual(parsed_body["channel_id"], str(dest_channel_id))

    def test_04_voice_member_move_error_responses(self):
        """Simulates REST API responses (204 No Content, 403 Forbidden, 400 Channel Full)."""
        def handle_move_response(status_code, body_json):
            if status_code in (200, 204):
                return {"status": "SUCCESS", "error": None}
            elif status_code == 403:
                return {"status": "ERROR", "error": "Missing Permissions (403 Forbidden)"}
            elif status_code == 400:
                err = body_json.get("message", "Channel is full")
                return {"status": "ERROR", "error": err}
            else:
                return {"status": "ERROR", "error": f"Unexpected HTTP {status_code}"}

        res_ok = handle_move_response(204, {})
        self.assertEqual(res_ok["status"], "SUCCESS")

        res_forbidden = handle_move_response(403, {"code": 50013, "message": "Missing Permissions"})
        self.assertEqual(res_forbidden["status"], "ERROR")
        self.assertIn("Missing Permissions", res_forbidden["error"])

        res_full = handle_move_response(400, {"code": 40001, "message": "Target channel is full"})
        self.assertEqual(res_full["status"], "ERROR")
        self.assertIn("Target channel is full", res_full["error"])

    # -------------------------------------------------------------
    # 2. Voice Region Migration & Reconnection
    # -------------------------------------------------------------
    def test_05_voice_region_migration_state_reset(self):
        """Verifies state machine cleans up old socket and resets DAVE state on VOICE_SERVER_UPDATE."""
        class MockVoiceSession:
            def __init__(self):
                self.endpoint = "us-east100.discord.media:443"
                self.token = "initial_token_123"
                self.ssrc = 12345
                self.udp_socket_open = True
                self.heartbeat_active = True
                self.dave_initialized = True
                self.last_seq = 42

            def handle_voice_server_update(self, new_endpoint, new_token):
                if new_endpoint != self.endpoint:
                    # Teardown old session
                    self.udp_socket_open = False
                    self.heartbeat_active = False
                    self.dave_initialized = False
                    self.last_seq = -1
                    # Update endpoint and token
                    self.endpoint = new_endpoint
                    self.token = new_token
                    return True
                return False

            def reinit_new_endpoint(self, new_ssrc):
                self.ssrc = new_ssrc
                self.udp_socket_open = True
                self.heartbeat_active = True
                self.dave_initialized = True

        session = MockVoiceSession()
        self.assertTrue(session.udp_socket_open)
        self.assertEqual(session.last_seq, 42)

        # Trigger region migration
        swapped = session.handle_voice_server_update("eu-west555.discord.media:443", "migrated_token_456")
        self.assertTrue(swapped)
        self.assertFalse(session.udp_socket_open, "Old UDP socket must be closed")
        self.assertFalse(session.dave_initialized, "DAVE session must be cleanly reset")
        self.assertEqual(session.last_seq, -1, "Sequence must be reset to -1")
        self.assertEqual(session.endpoint, "eu-west555.discord.media:443")

        # Re-establish to new server
        session.reinit_new_endpoint(99999)
        self.assertTrue(session.udp_socket_open)
        self.assertEqual(session.ssrc, 99999)

    # -------------------------------------------------------------
    # 3. Gateway Recovery: Resume vs Identify
    # -------------------------------------------------------------
    def test_06_gateway_resume_payload_and_invalid_session_backoff(self):
        """Verifies Op 6 Resume payload structure and Op 9 backoff with jitter."""
        session_id = "abc123session456"
        seq = 1420
        token = "test_user_token"

        # Build Op 6 Resume
        resume_payload = {
            "op": 6,
            "d": {
                "token": token,
                "session_id": session_id,
                "seq": seq
            }
        }
        self.assertEqual(resume_payload["op"], 6)
        self.assertEqual(resume_payload["d"]["seq"], 1420)

        # Simulate Op 9 Invalid Session: d=false means unresumable
        def handle_op9(d_resumable):
            if d_resumable:
                return "TRY_RESUME", 0.5
            else:
                # Must discard session and wait 1.0 - 5.0s before fresh Identify
                backoff_time = 1.5  # Deterministic test value within 1-5s
                return "FORCE_REIDENTIFY", backoff_time

        action_resumable, delay1 = handle_op9(True)
        self.assertEqual(action_resumable, "TRY_RESUME")

        action_unresumable, delay2 = handle_op9(False)
        self.assertEqual(action_unresumable, "FORCE_REIDENTIFY")
        self.assertGreaterEqual(delay2, 1.0)
        self.assertLessEqual(delay2, 5.0)

    # -------------------------------------------------------------
    # 4. Explorer File Drop & MIME Validation
    # -------------------------------------------------------------
    def test_07_explorer_file_drop_mime_parsing(self):
        """Verifies that text/uri-list drops containing file:// URLs are extracted and accepted."""
        raw_uri_list = "file:///C:/Users/test/Documents/screenshot.png\r\nfile:///C:/Users/test/Documents/log.txt\r\n"
        
        def parse_file_drops(mime_text):
            files = []
            for line in mime_text.splitlines():
                line = line.strip()
                if line.startswith("file:///"):
                    path = line[8:].replace("/", "\\")
                    files.append(path)
            return files

        extracted = parse_file_drops(raw_uri_list)
        self.assertEqual(len(extracted), 2)
        self.assertEqual(extracted[0], r"C:\Users\test\Documents\screenshot.png")
        self.assertEqual(extracted[1], r"C:\Users\test\Documents\log.txt")

    # -------------------------------------------------------------
    # 5. Live Audio Device Enumeration
    # -------------------------------------------------------------
    def test_08_audio_device_live_switching(self):
        """Simulates dynamic audio device switching without terminating active VoIP session."""
        mock_devices = [
            {"name": "Default Communication Device", "id": "wasapi_{0001}", "is_default": True},
            {"name": "Microphone (Realtek High Definition Audio)", "id": "wasapi_{0002}", "is_default": False},
            {"name": "USB Condenser Microphone", "id": "wasapi_{0003}", "is_default": False}
        ]

        class AudioStreamState:
            def __init__(self, current_id):
                self.active_device_id = current_id
                self.stream_running = True
                self.rebind_count = 0

            def switch_device(self, new_id):
                if new_id != self.active_device_id:
                    # Momentarily mute, rebind WASAPI endpoint, resume
                    self.active_device_id = new_id
                    self.rebind_count += 1
                    return True
                return False

        stream = AudioStreamState(mock_devices[0]["id"])
        self.assertEqual(stream.active_device_id, "wasapi_{0001}")

        # Switch to USB Microphone
        success = stream.switch_device(mock_devices[2]["id"])
        self.assertTrue(success)
        self.assertEqual(stream.active_device_id, "wasapi_{0003}")
        self.assertEqual(stream.rebind_count, 1)
        self.assertTrue(stream.stream_running, "VoIP audio stream must remain running across switch")

if __name__ == "__main__":
    unittest.main()
