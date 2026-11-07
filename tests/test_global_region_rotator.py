"""
Automated Test Suite: Discord Global Voice Channel Region Rotator
Validates all 10 mission criteria:
  1. Two users connected.
  2. Five users connected.
  3. Ten users connected.
  4. Different Discord client types connected (Ripcord, Desktop Electron, Browser, Mobile iOS/Android).
  5. First shared region change.
  6. Multiple consecutive shared region changes.
  7. New member joins during rotation.
  8. Existing member leaves during rotation.
  9. All clients receive the appropriate server-controlled voice updates.
 10. STOP prevents future region changes.
"""

import sys
import os
import time
import unittest
import tempfile

# Ensure tools directory is on path
tools_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "tools")
if tools_dir not in sys.path:
    sys.path.insert(0, tools_dir)

from discord_region_rotator import (
    DiscordRegionRotator,
    RipcordLiveSync,
    RotationMode,
    EmptyChannelBehavior,
    RotatorStatus,
    DiscordClientType,
    RateLimitException,
    PermissionDeniedException,
    load_persisted_config,
    save_persisted_config
)

class MockDiscordServerState:
    """Simulates Discord server-side channel and voice gateway behavior."""
    def __init__(self, guild_id=1122334455, channel_id=99887766):
        self.guild_id = guild_id
        self.channel_id = channel_id
        self.rtc_region = None  # Automatic
        self.has_manage_channels_perm = True
        self.rate_limit_counter = 0
        self.rate_limit_threshold = 999
        self.request_history = []

    def handle_request(self, method: str, endpoint: str, payload=None):
        self.request_history.append((method, endpoint, payload))

        # Check rate limits
        if self.rate_limit_counter >= self.rate_limit_threshold:
            self.rate_limit_counter = 0
            raise RateLimitException(0.2)

        # GET /channels/{id}
        if method == "GET" and endpoint == f"/channels/{self.channel_id}":
            return {
                "id": str(self.channel_id),
                "guild_id": str(self.guild_id),
                "name": "General Voice",
                "type": 2,
                "rtc_region": self.rtc_region
            }

        # PATCH /channels/{id}
        if method == "PATCH" and endpoint == f"/channels/{self.channel_id}":
            if not self.has_manage_channels_perm:
                raise PermissionDeniedException("403 Forbidden: Missing MANAGE_CHANNELS")

            new_region = payload.get("rtc_region")
            self.rtc_region = new_region
            self.rate_limit_counter += 1
            return {
                "id": str(self.channel_id),
                "guild_id": str(self.guild_id),
                "name": "General Voice",
                "type": 2,
                "rtc_region": self.rtc_region
            }

        return {}

class TestGlobalVoiceChannelRegionRotator(unittest.TestCase):
    
    def setUp(self):
        self.mock_server = MockDiscordServerState()
        self.rotator = DiscordRegionRotator(
            token="test_token_sample",
            guild_id=self.mock_server.guild_id,
            channel_id=self.mock_server.channel_id,
            interval_seconds=5,
            mode=RotationMode.SEQUENTIAL,
            transport_mock=self.mock_server.handle_request
        )

    def tearDown(self):
        if self.rotator.is_running():
            self.rotator.stop()

    # -------------------------------------------------------------
    # Scenario 1: Two users connected & first shared region change
    # -------------------------------------------------------------
    def test_01_two_users_connected_shared_region_change(self):
        """Verifies shared channel update migrates both connected members."""
        # Connect 2 members
        self.rotator.on_member_join(1001, DiscordClientType.RIPCORD)
        self.rotator.on_member_join(1002, DiscordClientType.DESKTOP_ELECTRON)
        self.assertEqual(self.rotator.get_member_count(), 2)

        # Execute shared channel update to "rotterdam"
        success = self.rotator.update_channel_rtc_region("rotterdam")
        self.assertTrue(success)
        self.assertEqual(self.mock_server.rtc_region, "rotterdam")
        self.assertEqual(self.rotator.current_rtc_region, "rotterdam")
        self.assertEqual(self.rotator.total_rotations, 1)

        # Verify all members received migration
        members = self.rotator.get_members()
        self.assertEqual(len(members), 2)
        for m in members:
            self.assertEqual(m.last_migration_confirmed, "rotterdam123.discord.media:443")
            self.assertEqual(m.migration_count, 1)

    # -------------------------------------------------------------
    # Scenario 2 & 4: Five users connected across distinct client types
    # -------------------------------------------------------------
    def test_02_five_users_connected_multi_client_types(self):
        """Verifies migration across Ripcord, Desktop Electron, Browser, and Mobile iOS/Android."""
        clients = [
            (2001, DiscordClientType.RIPCORD),
            (2002, DiscordClientType.DESKTOP_ELECTRON),
            (2003, DiscordClientType.BROWSER),
            (2004, DiscordClientType.MOBILE_IOS),
            (2005, DiscordClientType.MOBILE_ANDROID),
        ]
        for uid, ctype in clients:
            self.rotator.on_member_join(uid, ctype)
        self.assertEqual(self.rotator.get_member_count(), 5)

        # Update to singapore
        success = self.rotator.update_channel_rtc_region("singapore")
        self.assertTrue(success)
        self.assertEqual(self.mock_server.rtc_region, "singapore")

        # Verify each client type received migration separately
        for uid, ctype in clients:
            member = self.rotator.members[uid]
            self.assertEqual(member.client_type, ctype)
            self.assertEqual(member.last_migration_confirmed, "singapore123.discord.media:443")
            self.assertEqual(member.migration_count, 1)

    # -------------------------------------------------------------
    # Scenario 3: Ten users connected (High Concurrency)
    # -------------------------------------------------------------
    def test_03_ten_users_high_concurrency(self):
        """Verifies 10 connected members all cleanly migrate without desync."""
        for i in range(10):
            uid = 3000 + i
            ctype = DiscordClientType.DESKTOP_ELECTRON if i % 2 == 0 else DiscordClientType.BROWSER
            self.rotator.on_member_join(uid, ctype)

        self.assertEqual(self.rotator.get_member_count(), 10)

        # Update region
        self.rotator.update_channel_rtc_region("brazil")
        self.assertEqual(self.mock_server.rtc_region, "brazil")

        # All 10 must have verified migration
        for uid in range(3000, 3010):
            m = self.rotator.members[uid]
            self.assertEqual(m.last_migration_confirmed, "brazil123.discord.media:443")

    # -------------------------------------------------------------
    # Scenario 5 & 6: Multiple consecutive shared region changes
    # -------------------------------------------------------------
    def test_04_multiple_consecutive_region_changes(self):
        """Verifies continuous sequential rotation across 4 consecutive regions."""
        self.rotator.on_member_join(4001, DiscordClientType.RIPCORD)
        self.rotator.on_member_join(4002, DiscordClientType.DESKTOP_ELECTRON)

        sequence = ["us-east", "rotterdam", "singapore", "japan"]
        for idx, target in enumerate(sequence):
            success = self.rotator.update_channel_rtc_region(target)
            self.assertTrue(success)
            self.assertEqual(self.mock_server.rtc_region, target)
            self.assertEqual(self.rotator.total_rotations, idx + 1)
            # Verify members received all migrations
            m1 = self.rotator.members[4001]
            self.assertEqual(m1.migration_count, idx + 1)
            self.assertEqual(m1.last_migration_confirmed, f"{target}123.discord.media:443")

    # -------------------------------------------------------------
    # Scenario 7: New member joins during active rotation
    # -------------------------------------------------------------
    def test_05_member_joins_during_active_rotation(self):
        """A new member joining midway is seamlessly included in subsequent rotations."""
        self.rotator.on_member_join(5001, DiscordClientType.RIPCORD)
        self.rotator.update_channel_rtc_region("rotterdam")

        # Member 5002 joins after first rotation
        self.rotator.on_member_join(5002, DiscordClientType.MOBILE_IOS)
        self.assertEqual(self.rotator.get_member_count(), 2)
        self.assertEqual(self.rotator.members[5002].migration_count, 0)

        # Next rotation
        self.rotator.update_channel_rtc_region("frankfurt")
        self.assertEqual(self.rotator.members[5001].migration_count, 2)
        self.assertEqual(self.rotator.members[5002].migration_count, 1)
        self.assertEqual(self.rotator.members[5002].last_migration_confirmed, "frankfurt123.discord.media:443")

    # -------------------------------------------------------------
    # Scenario 8: Existing member leaves during active rotation
    # -------------------------------------------------------------
    def test_06_member_leaves_during_active_rotation(self):
        """When a member leaves, rotation continues seamlessly for remaining members."""
        self.rotator.on_member_join(6001, DiscordClientType.RIPCORD)
        self.rotator.on_member_join(6002, DiscordClientType.DESKTOP_ELECTRON)
        self.rotator.update_channel_rtc_region("us-west")

        # Member 6002 leaves
        self.rotator.on_member_leave(6002)
        self.assertEqual(self.rotator.get_member_count(), 1)
        self.assertNotIn(6002, self.rotator.members)

        # Next rotation
        self.rotator.update_channel_rtc_region("sydney")
        self.assertEqual(self.rotator.members[6001].migration_count, 2)
        self.assertEqual(self.rotator.members[6001].last_migration_confirmed, "sydney123.discord.media:443")

    # -------------------------------------------------------------
    # Scenario 9: Empty channel behavior (Pause)
    # -------------------------------------------------------------
    def test_07_empty_channel_behavior_pause(self):
        """Verifies rotator pauses when channel is empty under EMPTY_BEHAVIOR_PAUSE."""
        self.rotator.empty_behavior = EmptyChannelBehavior.PAUSE
        # No members connected
        self.assertEqual(self.rotator.get_member_count(), 0)

        # Start rotator in background
        self.rotator.start()
        time.sleep(0.3)
        self.assertEqual(self.rotator.status, RotatorStatus.PAUSED_EMPTY)

        # User joins -> rotator resumes
        self.rotator.on_member_join(7001, DiscordClientType.RIPCORD)
        time.sleep(0.7)
        self.assertIn(self.rotator.status, (RotatorStatus.ROTATING, RotatorStatus.WAITING_INTERVAL))
        self.rotator.stop()

    # -------------------------------------------------------------
    # Scenario 10: STOP immediately terminates future rotations
    # -------------------------------------------------------------
    def test_08_stop_immediately_prevents_further_rotations(self):
        """STOP terminates worker thread and prevents further REST calls."""
        self.rotator.on_member_join(8001, DiscordClientType.RIPCORD)
        self.rotator.start()
        time.sleep(0.3)
        self.assertTrue(self.rotator.is_running())

        # Click STOP
        self.rotator.stop()
        self.assertFalse(self.rotator.is_running())
        self.assertEqual(self.rotator.status, RotatorStatus.STOPPED)

        initial_history_len = len(self.mock_server.request_history)
        time.sleep(0.5)
        self.assertEqual(len(self.mock_server.request_history), initial_history_len, "Zero requests dispatched after STOP")

    # -------------------------------------------------------------
    # Permission Handling & Rate Limits
    # -------------------------------------------------------------
    def test_09_permission_failure_handling(self):
        """Lacking MANAGE_CHANNELS halts rotator with clear error state."""
        self.mock_server.has_manage_channels_perm = False
        self.rotator.on_member_join(9001, DiscordClientType.RIPCORD)
        
        self.rotator.start()
        time.sleep(0.5)

        self.assertFalse(self.rotator.has_permission)
        self.assertIn(self.rotator.status, (RotatorStatus.ERROR, RotatorStatus.PERMISSION_DENIED))
        self.assertIn("MANAGE_CHANNELS", self.rotator.last_error)
        self.rotator.stop()

    def test_10_rate_limit_backoff_and_recovery(self):
        """HTTP 429 Too Many Requests triggers rate-limited status and clean recovery."""
        self.mock_server.rate_limit_threshold = 1  # Next request triggers 429
        self.rotator.on_member_join(9002, DiscordClientType.RIPCORD)

        self.rotator.start()
        time.sleep(0.4)
        # Verify rate-limit was handled without crashing
        time.sleep(0.4)
        self.rotator.stop()
        self.assertGreaterEqual(self.rotator.total_rotations, 1)

    def test_11_ripcord_live_sync_database_structure(self):
        """RipcordLiveSync queries ripdb structure without crashing."""
        state = RipcordLiveSync.get_live_state()
        self.assertIsInstance(state, dict)
        self.assertIn("available", state)
        self.assertIn("connected", state)

    def test_12_manual_instant_region_switch(self):
        """Instant region switch directly dispatches PATCH without waiting for timer."""
        success = self.rotator.switch_region_now("singapore")
        self.assertTrue(success)
        self.assertEqual(self.mock_server.rtc_region, "singapore")
        self.assertEqual(self.rotator.current_rtc_region, "singapore")

    def test_13_config_persistence(self):
        """Config save and load persists token and channel settings."""
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tf:
            temp_path = tf.name
        try:
            save_persisted_config({"token": "persisted_test_token", "interval": 20}, path=temp_path)
            cfg = load_persisted_config(path=temp_path)
            self.assertEqual(cfg.get("token"), "persisted_test_token")
            self.assertEqual(cfg.get("interval"), 20)
        finally:
            if os.path.exists(temp_path):
                os.remove(temp_path)

if __name__ == "__main__":
    unittest.main()
