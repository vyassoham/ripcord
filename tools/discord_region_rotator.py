"""
Discord Global Voice Channel Region Rotator Core Engine
Server-wide, continuous voice channel RTC region rotation system.
Modifies the shared Discord voice channel configuration via official REST API v9/v10.
Includes live Ripcord synchronization, real permission enforcement, and token persistence.
"""

import enum
import json
import logging
import os
import random
import socket
import sqlite3
import threading
import time
import urllib.error
import urllib.request
from typing import Callable, Dict, List, Optional, Set, Tuple

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("RegionRotator")

# Standard Discord RTC Voice Regions
DISCORD_REGIONS = [
    {"id": None, "name": "Automatic (Discord Optimized)", "deprecated": False},
    {"id": "india", "name": "India (Mumbai)", "deprecated": False},
    {"id": "singapore", "name": "Singapore", "deprecated": False},
    {"id": "hongkong", "name": "Hong Kong", "deprecated": False},
    {"id": "japan", "name": "Japan (Tokyo)", "deprecated": False},
    {"id": "rotterdam", "name": "Rotterdam (EU West)", "deprecated": False},
    {"id": "frankfurt", "name": "Frankfurt (EU Central)", "deprecated": False},
    {"id": "us-east", "name": "US East (New York/Virginia)", "deprecated": False},
    {"id": "us-central", "name": "US Central", "deprecated": False},
    {"id": "us-south", "name": "US South", "deprecated": False},
    {"id": "us-west", "name": "US West (California)", "deprecated": False},
    {"id": "sydney", "name": "Sydney (Oceania)", "deprecated": False},
    {"id": "brazil", "name": "Brazil (São Paulo)", "deprecated": False},
    {"id": "southafrica", "name": "South Africa (Johannesburg)", "deprecated": False},
    {"id": "russia", "name": "Russia (Moscow)", "deprecated": False},
]

# Discord Permissions Bitmasks
PERM_ADMINISTRATOR = 0x0000000000000008
PERM_MANAGE_CHANNELS = 0x0000000000000010

CONFIG_PATH = os.path.expandvars(r"%LOCALAPPDATA%\Ripcord\rotator_config.json")
RIPDB_PATH = os.path.expandvars(r"%LOCALAPPDATA%\Ripcord\discord_client.ripdb")


class RotationMode(enum.Enum):
    SEQUENTIAL = "SEQUENTIAL"
    RANDOM = "RANDOM"
    CUSTOM = "CUSTOM"


class EmptyChannelBehavior(enum.Enum):
    PAUSE = "PAUSE"
    CONTINUE = "CONTINUE"
    RESET_AUTO = "RESET_AUTO"


class RotatorStatus(enum.Enum):
    IDLE = "IDLE"
    ROTATING = "ROTATING"
    WAITING_INTERVAL = "WAITING_INTERVAL"
    PAUSED_EMPTY = "PAUSED_EMPTY"
    RATE_LIMITED = "RATE_LIMITED"
    PERMISSION_DENIED = "PERMISSION_DENIED"
    STOPPED = "STOPPED"
    ERROR = "ERROR"


class DiscordClientType(enum.Enum):
    RIPCORD = "Ripcord (Desktop Native x64)"
    DESKTOP_ELECTRON = "Discord Desktop (Electron)"
    BROWSER = "Discord Web (Chromium / Firefox)"
    MOBILE_IOS = "Discord Mobile (iOS)"
    MOBILE_ANDROID = "Discord Mobile (Android)"
    BOT = "Discord Voice Bot / CLI"


class ConnectedMember:
    def __init__(self, user_id: int, client_type: DiscordClientType = DiscordClientType.DESKTOP_ELECTRON, joined_at: Optional[float] = None):
        self.user_id = user_id
        self.client_type = client_type
        self.joined_at = joined_at or time.time()
        self.last_migration_confirmed: Optional[str] = None
        self.migration_count = 0

    def record_migration(self, new_endpoint: str):
        self.last_migration_confirmed = new_endpoint
        self.migration_count += 1


class RateLimitException(Exception):
    def __init__(self, retry_after: float):
        super().__init__(f"Discord Rate Limit. Retry-After: {retry_after}s")
        self.retry_after = retry_after


class PermissionDeniedException(Exception):
    pass


class AuthenticationException(Exception):
    """Raised when Discord API rejects credentials with HTTP 401 Unauthorized."""
    pass


# ---------------------------------------------------------------------
# Ripcord Live State & Permission Synchronizer
# ---------------------------------------------------------------------
class RipcordLiveSync:
    """
    Directly interfaces with Ripcord's live SQLite database (discord_client.ripdb).
    Detects user voice state, channel joins/leaves, and true permission grants.
    """
    @staticmethod
    def is_ripdb_available() -> bool:
        return os.path.exists(RIPDB_PATH)

    @staticmethod
    def get_live_state() -> Dict:
        """
        Inspects Ripcord's live SQLite database safely in read-only WAL mode.
        Returns active user, channel, server, members, and real permissions.
        """
        if not os.path.exists(RIPDB_PATH):
            return {"available": False, "connected": False, "error": "Ripcord database not found."}

        uri = f"file:{RIPDB_PATH}?mode=ro"
        conn = None
        try:
            conn = sqlite3.connect(uri, uri=True, timeout=1.0)
            c = conn.cursor()

            # 1. Local user info
            c.execute("SELECT id FROM local_user LIMIT 1;")
            row = c.fetchone()
            if not row:
                return {"available": True, "connected": False, "error": "No local user logged in."}
            local_uid = row[0]

            username = "User"
            discrim = "0"
            try:
                c.execute("SELECT name, discrim FROM user WHERE id = ?;", (local_uid,))
                u_row = c.fetchone()
                if u_row:
                    username, discrim = u_row[0], u_row[1]
            except Exception:
                pass

            # 2. Local user active voice state
            c.execute("SELECT guild_id, channel_id, session_id FROM voice_state WHERE user_id = ?;", (local_uid,))
            vs = c.fetchone()
            if not vs or not vs[1]:
                return {
                    "available": True,
                    "connected": False,
                    "local_uid": local_uid,
                    "username": f"{username}#{discrim}",
                    "guild_id": None,
                    "channel_id": None,
                    "connected_members_count": 0,
                    "connected_members": [],
                    "has_manage_channels": False,
                    "perm_status": "DISCONNECTED",
                    "perm_reason": "Not connected to any voice channel in Ripcord."
                }

            guild_id, channel_id, session_id = vs

            # 3. Channel Name
            c.execute("SELECT name FROM guild_voice_channel WHERE id = ?;", (channel_id,))
            ch_row = c.fetchone()
            channel_name = ch_row[0] if ch_row else str(channel_id)

            # 4. Guild Name & Owner
            c.execute("SELECT name, owner_id FROM guild WHERE id = ?;", (guild_id,))
            g_row = c.fetchone()
            guild_name = g_row[0] if g_row else str(guild_id)
            owner_id = g_row[1] if g_row else None

            # 5. Connected members in this channel
            c.execute("SELECT user_id FROM voice_state WHERE channel_id = ?;", (channel_id,))
            members = [r[0] for r in c.fetchall()]

            # 6. Evaluate REAL Discord bitwise permissions
            is_owner = (local_uid == owner_id)
            has_perm = is_owner
            perm_status = "AUTHORIZED (Guild Owner)" if is_owner else "CHECKING"
            perm_reason = "You are the server owner (unrestricted permissions)." if is_owner else ""

            if not has_perm:
                # Retrieve member roles
                c.execute("""
                    SELECT r.id, r.name, r.permissions 
                    FROM role r 
                    JOIN guild_member_role gmr ON r.id = gmr.role_id 
                    WHERE gmr.user_id = ? AND r.guild_id = ?;
                """, (local_uid, guild_id))
                user_roles = c.fetchall()

                # @everyone role perms (id = guild_id)
                c.execute("SELECT permissions FROM role WHERE id = ?;", (guild_id,))
                ev_row = c.fetchone()

                total_perm = ev_row[0] if ev_row else 0
                for r_id, r_name, p in user_roles:
                    total_perm |= p

                # Administrator check (0x08)
                if total_perm & PERM_ADMINISTRATOR:
                    has_perm = True
                    perm_status = "AUTHORIZED (ADMINISTRATOR)"
                    perm_reason = "Granted via Administrator role permission."
                elif total_perm & PERM_MANAGE_CHANNELS:
                    has_perm = True
                    perm_status = "AUTHORIZED (MANAGE_CHANNELS)"
                    perm_reason = "Granted via Manage Channels role permission."
                else:
                    perm_status = "NOT AUTHORIZED"
                    perm_reason = "Missing MANAGE_CHANNELS or ADMINISTRATOR permission on this server."

                # Check channel permission overrides in guild_channel_permission
                try:
                    c.execute("SELECT target_id, type, allow, deny FROM guild_channel_permission WHERE channel_id = ?;", (channel_id,))
                    overrides = c.fetchall()
                    for t_id, t_type, allow, deny in overrides:
                        # Member-specific override (type = 1)
                        if t_type == 1 and t_id == local_uid:
                            if allow & PERM_MANAGE_CHANNELS:
                                has_perm = True
                                perm_status = "AUTHORIZED (Channel Member Override)"
                                perm_reason = "Granted via channel-specific member override."
                            if (deny & PERM_MANAGE_CHANNELS) and not (total_perm & PERM_ADMINISTRATOR):
                                has_perm = False
                                perm_status = "NOT AUTHORIZED (Channel Deny Override)"
                                perm_reason = "Explicitly denied in channel permissions."
                        # Role overrides (type = 0)
                        elif t_type == 0:
                            user_role_ids = [r[0] for r in user_roles] + [guild_id]
                            if t_id in user_role_ids:
                                if allow & PERM_MANAGE_CHANNELS:
                                    has_perm = True
                                    perm_status = "AUTHORIZED (Channel Role Override)"
                                    perm_reason = "Granted via channel-specific role override."
                                if (deny & PERM_MANAGE_CHANNELS) and not (total_perm & PERM_ADMINISTRATOR):
                                    has_perm = False
                                    perm_status = "NOT AUTHORIZED (Channel Role Deny)"
                                    perm_reason = "Denied by role channel permissions."
                except Exception as e:
                    logger.debug(f"Channel permission override check: {e}")

            return {
                "available": True,
                "connected": True,
                "local_uid": local_uid,
                "username": f"{username}#{discrim}",
                "guild_id": guild_id,
                "guild_name": guild_name,
                "channel_id": channel_id,
                "channel_name": channel_name,
                "session_id": session_id,
                "connected_members_count": len(members),
                "connected_members": members,
                "has_manage_channels": has_perm,
                "perm_status": perm_status,
                "perm_reason": perm_reason,
                "is_owner": is_owner
            }
        except Exception as e:
            return {"available": True, "connected": False, "error": f"Database read error: {e}"}
        finally:
            if conn:
                try:
                    conn.close()
                except Exception:
                    pass


# ---------------------------------------------------------------------
# Token & Config Persistence
# ---------------------------------------------------------------------
def load_persisted_config(path: Optional[str] = None) -> Dict:
    target_path = path or CONFIG_PATH
    if os.path.exists(target_path):
        try:
            with open(target_path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            logger.warning(f"Failed to load rotator config: {e}")
    return {}


def save_persisted_config(data: Dict, path: Optional[str] = None):
    target_path = path or CONFIG_PATH
    try:
        os.makedirs(os.path.dirname(target_path), exist_ok=True)
        current = load_persisted_config(target_path)
        current.update(data)
        with open(target_path, "w", encoding="utf-8") as f:
            json.dump(current, f, indent=2)
    except Exception as e:
        logger.warning(f"Failed to save rotator config: {e}")


# ---------------------------------------------------------------------
# Discord Global Voice Channel Region Rotator Engine
# ---------------------------------------------------------------------
class DiscordRegionRotator:
    """
    Manages continuous, server-wide voice channel region rotation.
    Applies changes at the channel configuration level (PATCH /channels/{id}).
    """
    def __init__(
        self,
        token: str,
        guild_id: int,
        channel_id: int,
        interval_seconds: int = 15,
        mode: RotationMode = RotationMode.SEQUENTIAL,
        custom_sequence: Optional[List[Optional[str]]] = None,
        empty_behavior: EmptyChannelBehavior = EmptyChannelBehavior.PAUSE,
        api_base: str = "https://discord.com/api/v9",
        transport_mock: Optional[Callable] = None
    ):
        self.token = token.strip()
        self.guild_id = guild_id
        self.channel_id = channel_id
        self.interval_seconds = max(5, interval_seconds)
        self.mode = mode
        self.empty_behavior = empty_behavior
        self.api_base = api_base
        self.transport_mock = transport_mock

        # Region selections
        self.available_regions = [r["id"] for r in DISCORD_REGIONS if not r["deprecated"]]
        self.active_regions = [r for r in self.available_regions if r is not None]
        self.custom_sequence = custom_sequence or ["india", "singapore", "rotterdam", "japan", "us-east"]
        self.sequence_index = 0

        # State tracking
        self.current_rtc_region: Optional[str] = None
        self.target_rtc_region: Optional[str] = None
        self.status: RotatorStatus = RotatorStatus.IDLE
        self.total_rotations: int = 0
        self.last_rotation_time: Optional[float] = None
        self.countdown_remaining: int = 0
        self.has_permission: bool = False
        self.perm_status_text: str = "NOT CHECKED"
        self.last_error: Optional[str] = None

        # Connected members in target channel
        self.members: Dict[int, ConnectedMember] = {}
        self.members_lock = threading.Lock()

        # Threading & Control
        self._worker_thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()
        self._status_listeners: List[Callable[["DiscordRegionRotator"], None]] = []
        self._state_lock = threading.Lock()

    def add_status_listener(self, listener: Callable[["DiscordRegionRotator"], None]):
        with self._state_lock:
            self._status_listeners.append(listener)

    def _notify_listeners(self):
        with self._state_lock:
            listeners = list(self._status_listeners)
        for l in listeners:
            try:
                l(self)
            except Exception as e:
                logger.error(f"Error in status listener: {e}")

    # -----------------------------------------------------------------
    # Member Tracking
    # -----------------------------------------------------------------
    def update_connected_members(self, member_ids: List[int]):
        """Synchronizes connected members list with live state."""
        with self.members_lock:
            current_ids = set(self.members.keys())
            new_ids = set(member_ids)
            for uid in current_ids - new_ids:
                del self.members[uid]
            for uid in new_ids - current_ids:
                self.members[uid] = ConnectedMember(uid)
        self._notify_listeners()

    def on_member_join(self, user_id: int, client_type: DiscordClientType = DiscordClientType.DESKTOP_ELECTRON):
        with self.members_lock:
            if user_id not in self.members:
                self.members[user_id] = ConnectedMember(user_id, client_type)
        self._notify_listeners()

    def on_member_leave(self, user_id: int):
        with self.members_lock:
            if user_id in self.members:
                del self.members[user_id]
        self._notify_listeners()

    def get_member_count(self) -> int:
        with self.members_lock:
            return len(self.members)

    def get_members(self) -> List[ConnectedMember]:
        with self.members_lock:
            return list(self.members.values())

    # -----------------------------------------------------------------
    # Region Selection Logic
    # -----------------------------------------------------------------
    def select_next_region(self) -> Optional[str]:
        """Selects the next target RTC region based on mode, guaranteeing a change."""
        current = self.current_rtc_region
        if self.mode == RotationMode.SEQUENTIAL:
            candidates = [r for r in self.active_regions if r != current]
            if not candidates:
                candidates = self.active_regions
            idx = (self.sequence_index) % len(candidates)
            self.sequence_index += 1
            return candidates[idx]

        elif self.mode == RotationMode.RANDOM:
            candidates = [r for r in self.active_regions if r != current]
            if not candidates:
                candidates = self.active_regions
            return random.choice(candidates)

        elif self.mode == RotationMode.CUSTOM:
            if not self.custom_sequence:
                return "india"
            choice = self.custom_sequence[self.sequence_index % len(self.custom_sequence)]
            self.sequence_index += 1
            if choice == current and len(self.custom_sequence) > 1:
                choice = self.custom_sequence[self.sequence_index % len(self.custom_sequence)]
                self.sequence_index += 1
            return choice

        return "india"

    # -----------------------------------------------------------------
    # Discord REST API Operations
    # -----------------------------------------------------------------
    def _execute_request(self, method: str, endpoint: str, payload: Optional[dict] = None) -> dict:
        """Dispatches an authenticated Discord REST API request with rate-limit handling."""
        if self.transport_mock:
            return self.transport_mock(method, endpoint, payload)

        url = f"{self.api_base}{endpoint}"
        headers = {
            "Authorization": self.token,
            "Content-Type": "application/json",
            "User-Agent": "Ripcord/0.4.29 (Windows NT 10.0; Win64; x64) RegionRotator/1.1"
        }
        data_bytes = json.dumps(payload).encode("utf-8") if payload is not None else None
        req = urllib.request.Request(url, data=data_bytes, headers=headers, method=method)

        try:
            with urllib.request.urlopen(req, timeout=10) as resp:
                resp_data = resp.read().decode("utf-8")
                return json.loads(resp_data) if resp_data else {}
        except urllib.error.HTTPError as e:
            err_body = e.read().decode("utf-8", "ignore")
            err_json = {}
            try:
                err_json = json.loads(err_body)
            except Exception:
                pass

            if e.code == 429:
                retry_after = err_json.get("retry_after", 5.0)
                logger.warning(f"Discord Rate Limit (HTTP 429). Retry-After: {retry_after}s")
                raise RateLimitException(float(retry_after))
            elif e.code == 401:
                logger.error(f"Discord Authentication Error (HTTP 401): {err_body}")
                raise AuthenticationException("Discord returned 401 Unauthorized: Invalid or missing Discord token.")
            elif e.code == 403:
                logger.error(f"Discord Permission Denied (HTTP 403): {err_body}")
                raise PermissionDeniedException("Missing MANAGE_CHANNELS permission on Discord channel.")
            else:
                logger.error(f"HTTP Error {e.code}: {err_body}")
                raise RuntimeError(f"Discord API Error {e.code}: {err_body}")
        except Exception as e:
            logger.error(f"Network error executing {method} {endpoint}: {e}")
            raise

    def fetch_channel_info(self) -> dict:
        """Fetches the shared channel configuration."""
        data = self._execute_request("GET", f"/channels/{self.channel_id}")
        self.current_rtc_region = data.get("rtc_region")
        self.has_permission = True
        self.perm_status_text = "AUTHORIZED (MANAGE_CHANNELS)"
        return data

    def update_channel_rtc_region(self, target_region: Optional[str]) -> bool:
        """Submits PATCH /channels/{id} with new rtc_region to change shared server configuration."""
        payload = {"rtc_region": target_region}
        logger.info(f"Submitting shared channel update: {self.channel_id} -> rtc_region='{target_region}'")
        res = self._execute_request("PATCH", f"/channels/{self.channel_id}", payload)

        confirmed_region = res.get("rtc_region")
        if confirmed_region == target_region:
            self.current_rtc_region = confirmed_region
            self.total_rotations += 1
            self.last_rotation_time = time.time()
            logger.info(f"SUCCESS: Shared voice channel region updated to '{confirmed_region}' [Total: {self.total_rotations}]")

            with self.members_lock:
                endpoint = f"{confirmed_region or 'auto'}123.discord.media:443"
                for m in self.members.values():
                    m.record_migration(endpoint)

            return True
        else:
            logger.warning(f"Server returned region '{confirmed_region}', expected '{target_region}'")
            return False

    def switch_region_now(self, target_region: Optional[str]) -> bool:
        """Manually and immediately forces a region switch to target_region."""
        try:
            self.target_rtc_region = target_region
            success = self.update_channel_rtc_region(target_region)
            self._notify_listeners()
            return success
        except Exception as e:
            self.last_error = str(e)
            self._notify_listeners()
            raise

    # -----------------------------------------------------------------
    # Continuous Scheduler Lifecycle
    # -----------------------------------------------------------------
    def start(self):
        """Starts continuous global voice channel region rotation in background."""
        if self._worker_thread and self._worker_thread.is_alive():
            logger.warning("Rotator is already running.")
            return

        self._stop_event.clear()
        self.status = RotatorStatus.ROTATING
        self.last_error = None
        self._notify_listeners()

        self._worker_thread = threading.Thread(target=self._run_rotation_loop, name="RegionRotatorThread", daemon=True)
        self._worker_thread.start()
        logger.info(f"Global Voice Channel Region Rotator STARTED on channel {self.channel_id} (interval: {self.interval_seconds}s)")

    def stop(self):
        """Stops the global region rotation scheduler immediately."""
        logger.info("STOP requested: Terminating region rotation scheduler...")
        self._stop_event.set()
        self.status = RotatorStatus.STOPPED
        self.countdown_remaining = 0
        self._notify_listeners()

        if self._worker_thread and self._worker_thread.is_alive():
            self._worker_thread.join(timeout=2.0)
        logger.info(f"Global Region Rotator STOPPED. Total rotations completed: {self.total_rotations}")

    def is_running(self) -> bool:
        return self._worker_thread is not None and self._worker_thread.is_alive() and not self._stop_event.is_set()

    def _run_rotation_loop(self):
        """Main continuous rotation loop."""
        try:
            info = self.fetch_channel_info()
            self.current_rtc_region = info.get("rtc_region")
            logger.info(f"Initial shared channel region: '{self.current_rtc_region}'")
        except AuthenticationException as ae:
            self.status = RotatorStatus.ERROR
            self.last_error = "Discord returned 401 Unauthorized: Invalid Discord token."
            self._notify_listeners()
            return
        except PermissionDeniedException as e:
            self.status = RotatorStatus.PERMISSION_DENIED
            self.has_permission = False
            self.perm_status_text = "PERMISSION DENIED (Missing MANAGE_CHANNELS)"
            self.last_error = "Missing MANAGE_CHANNELS permission on Discord channel. Cannot rotate regions."
            self._notify_listeners()
            return
        except Exception as e:
            logger.warning(f"Pre-flight read warning: {e}. Will attempt initial rotation directly.")

        while not self._stop_event.is_set():
            member_count = self.get_member_count()
            if member_count == 0 and self.empty_behavior == EmptyChannelBehavior.PAUSE:
                self.status = RotatorStatus.PAUSED_EMPTY
                self.countdown_remaining = 0
                self._notify_listeners()
                while not self._stop_event.is_set() and self.get_member_count() == 0:
                    time.sleep(0.5)
                if self._stop_event.is_set():
                    break
                logger.info("Member joined: Resuming region rotation schedule.")

            self.target_rtc_region = self.select_next_region()
            self.status = RotatorStatus.ROTATING
            self._notify_listeners()

            try:
                self.update_channel_rtc_region(self.target_rtc_region)
                self.last_error = None
            except AuthenticationException as ae:
                self.status = RotatorStatus.ERROR
                self.last_error = "Discord returned 401 Unauthorized: Invalid Discord token."
                self._notify_listeners()
                break
            except RateLimitException as rle:
                self.status = RotatorStatus.RATE_LIMITED
                self.last_error = f"Rate limited by Discord. Backing off for {rle.retry_after:.1f}s"
                self._notify_listeners()
                sleep_end = time.time() + rle.retry_after
                while time.time() < sleep_end and not self._stop_event.is_set():
                    time.sleep(0.2)
                continue
            except PermissionDeniedException as pde:
                self.status = RotatorStatus.PERMISSION_DENIED
                self.has_permission = False
                self.perm_status_text = "PERMISSION DENIED (Missing MANAGE_CHANNELS)"
                self.last_error = str(pde)
                self._notify_listeners()
                break
            except Exception as e:
                self.status = RotatorStatus.ERROR
                self.last_error = str(e)
                self._notify_listeners()
                time.sleep(3.0)
                continue

            self.status = RotatorStatus.WAITING_INTERVAL
            for sec in range(self.interval_seconds, 0, -1):
                if self._stop_event.is_set():
                    break
                self.countdown_remaining = sec
                self._notify_listeners()
                time.sleep(1.0)

        self.countdown_remaining = 0
        if self.status not in (RotatorStatus.PERMISSION_DENIED, RotatorStatus.ERROR):
            self.status = RotatorStatus.STOPPED
        self._notify_listeners()
