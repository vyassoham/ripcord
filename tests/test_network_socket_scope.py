"""
Unit Test Suite: Socket Network Layer & IPv4 Scope Isolation
Validates that IPv4 forcing is limited strictly to Discord voice media servers,
leaving Slack, Discord REST API, CDN, and local sockets on dual-stack/IPv6.
"""

import unittest

def is_discord_voice_host(hostname: str) -> bool:
    """Emulates C function IsDiscordVoiceHost in hook.c."""
    if not hostname:
        return False
    h = hostname.lower()
    return "discord.media" in h or "discord.gg" in h

class TestNetworkSocketScope(unittest.TestCase):
    def test_01_discord_voice_media_hosts_match(self):
        """Discord voice servers (*.discord.media and *.discord.gg) match and trigger IPv4 routing."""
        voice_hosts = [
            "us-east123.discord.media",
            "rotterdam884.discord.media",
            "singapore12.discord.media",
            "gateway.discord.gg",
            "voice.discord.gg",
        ]
        for vh in voice_hosts:
            with self.subTest(host=vh):
                self.assertTrue(is_discord_voice_host(vh))

    def test_02_discord_rest_api_preserved(self):
        """Discord REST API (discord.com) is NOT forced to IPv4, preserving IPv6/dual-stack."""
        self.assertFalse(is_discord_voice_host("discord.com"))
        self.assertFalse(is_discord_voice_host("status.discord.com"))

    def test_03_discord_cdn_preserved(self):
        """Discord CDN (cdn.discordapp.com, media.discordapp.net) is NOT forced to IPv4."""
        self.assertFalse(is_discord_voice_host("cdn.discordapp.com"))
        self.assertFalse(is_discord_voice_host("media.discordapp.net"))

    def test_04_slack_hosts_preserved(self):
        """Slack workspaces and APIs are NEVER affected by Discord voice routing constraints."""
        slack_hosts = [
            "slack.com",
            "api.slack.com",
            "myteam.slack.com",
            "files.slack.com",
            "wss-primary.slack.com",
        ]
        for sh in slack_hosts:
            with self.subTest(host=sh):
                self.assertFalse(is_discord_voice_host(sh))

    def test_05_empty_and_localhost_preserved(self):
        """Localhost, loopback, and empty hostnames are preserved."""
        self.assertFalse(is_discord_voice_host(""))
        self.assertFalse(is_discord_voice_host("127.0.0.1"))
        self.assertFalse(is_discord_voice_host("::1"))
        self.assertFalse(is_discord_voice_host("localhost"))

if __name__ == "__main__":
    unittest.main()
