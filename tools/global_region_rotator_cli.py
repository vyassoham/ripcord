"""
Ripcord / Discord Global Voice Channel Region Rotator — Command-Line Tool
Server-wide, continuous voice channel region rotation runner with live terminal status.
"""

import argparse
import signal
import sys
import time

from discord_region_rotator import (
    DiscordRegionRotator,
    RotationMode,
    EmptyChannelBehavior,
    RotatorStatus,
    DiscordClientType
)

def main():
    parser = argparse.ArgumentParser(description="Discord Global Voice Channel Region Rotator")
    parser.add_argument("--token", required=True, help="Discord user or bot token")
    parser.add_argument("--guild", required=True, type=int, help="Target Guild (Server) ID")
    parser.add_argument("--channel", required=True, type=int, help="Target Voice Channel ID")
    parser.add_argument("--interval", type=int, default=15, help="Rotation interval in seconds (min 5s)")
    parser.add_argument("--mode", choices=["SEQUENTIAL", "RANDOM", "CUSTOM"], default="SEQUENTIAL", help="Rotation mode")
    parser.add_argument("--custom", type=str, default="", help="Comma-separated custom region list")
    parser.add_argument("--empty", choices=["PAUSE", "CONTINUE", "RESET_AUTO"], default="PAUSE", help="Empty channel behavior")
    parser.add_argument("--max-rotations", type=int, default=0, help="Max rotations to perform (0 for continuous until stopped)")

    args = parser.parse_args()

    mode = RotationMode[args.mode]
    empty_behavior = EmptyChannelBehavior[args.empty]
    custom_sequence = [r.strip() for r in args.custom.split(",") if r.strip()] if args.custom else None

    print("=" * 70)
    print("DISCORD GLOBAL VOICE CHANNEL REGION ROTATOR (SERVER-WIDE)")
    print("=" * 70)
    print(f"Target Guild ID       : {args.guild}")
    print(f"Target Channel ID     : {args.channel}")
    print(f"Rotation Interval     : {args.interval}s")
    print(f"Rotation Mode         : {mode.value}")
    print(f"Empty Channel Policy  : {empty_behavior.value}")
    print("=" * 70)

    rotator = DiscordRegionRotator(
        token=args.token,
        guild_id=args.guild,
        channel_id=args.channel,
        interval_seconds=args.interval,
        mode=mode,
        custom_sequence=custom_sequence,
        empty_behavior=empty_behavior
    )

    def sig_handler(sig, frame):
        print("\n[STOP] Signal received. Halting global region rotation gracefully...")
        rotator.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    rotator.start()
    print("Rotator active. Press Ctrl+C to STOP.\n")

    try:
        last_rot_count = -1
        while rotator.is_running():
            time.sleep(1.0)
            if rotator.total_rotations != last_rot_count:
                last_rot_count = rotator.total_rotations
                print(f"[{time.strftime('%H:%M:%S')}] STATUS: {rotator.status.value} | Current: {rotator.current_rtc_region} | Target: {rotator.target_rtc_region} | Members: {rotator.get_member_count()} | Rotations: {rotator.total_rotations}")
            
            if args.max_rotations > 0 and rotator.total_rotations >= args.max_rotations:
                print(f"Reached max rotations ({args.max_rotations}). Stopping.")
                rotator.stop()
                break

    except KeyboardInterrupt:
        print("\nStopping...")
        rotator.stop()

    print(f"\nFinal Summary: Completed {rotator.total_rotations} server-wide region rotations.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
