"""
Unit Test Suite: Erlang ETF (External Term Format) Parser and Small Tuple Decoding
Validates ETF term decoding, SMALL_TUPLE_EXT (tag 104) handling, map key skipping,
and bounds check safety on malformed/truncated payloads.
"""

import unittest
import struct

ETF_VERSION = 131
ETF_SMALL_ATOM = 115
ETF_SMALL_TUPLE = 104

def encode_etf_atom(name: str) -> bytes:
    name_bytes = name.encode("latin1")
    return bytes([ETF_SMALL_ATOM, len(name_bytes)]) + name_bytes

def encode_etf_small_tuple(elements: list) -> bytes:
    arity = len(elements)
    assert arity <= 255
    res = bytes([ETF_SMALL_TUPLE, arity])
    for elem in elements:
        res += elem
    return res

DEDFRAME_TUPLE_FLAG = 0x8000000

class MockDedState:
    """Simulates Ripcord's DedState ETF decoder state machine with DedStepHook applied."""
    def __init__(self, data: bytes):
        self.data = bytearray(data)
        self.length = len(data)
        self.index = 255
        self.frames = [0] * 256

    def step(self):
        if self.index != 255:
            frame = self.frames[self.index]
            if frame == DEDFRAME_TUPLE_FLAG:
                self.index += 1
                return {"type": "event", "event": "ListEnd"}
            if (frame & DEDFRAME_TUPLE_FLAG) == 0 and frame < 2:
                self.index += 1
                return {"type": "event", "event": "ListEnd"}

        term_size = self.length
        if term_size == 0:
            return {"type": "error", "error": "EndedUnexpectedly"}

        term_tag = self.data[0]

        # SMALL_TUPLE_EXT handling
        if term_tag == ETF_SMALL_TUPLE:
            if term_size < 2:
                return {"type": "error", "error": "EndedUnexpectedly"}

            if self.index == 0:
                return {"type": "error", "error": "ExceededSizeLimit"}

            # Update parent frame count if inside an outer list/tuple
            if self.index != 255:
                frame = self.frames[self.index]
                self.frames[self.index] = (frame & 1) | (2 * (frame >> 1) - 2)

            arity = self.data[1]
            self.index -= 1
            self.frames[self.index] = DEDFRAME_TUPLE_FLAG | (2 * arity)

            self.data = self.data[2:]
            self.length -= 2

            return {"type": "event", "event": "ListStart", "arity": arity}

        # Atom handling
        if term_tag == ETF_SMALL_ATOM:
            if term_size < 2:
                return {"type": "error", "error": "EndedUnexpectedly"}
            atom_len = self.data[1]
            if term_size < 2 + atom_len:
                return {"type": "error", "error": "EndedUnexpectedly"}

            if self.index != 255:
                self.frames[self.index] -= 2

            val = self.data[2:2+atom_len].decode("latin1", errors="replace")
            self.data = self.data[2+atom_len:]
            self.length -= (2 + atom_len)
            return {"type": "event", "event": "Atom", "value": val}

        # Scalar fallback
        if self.index != 255:
            self.frames[self.index] -= 2
        self.data = self.data[1:]
        self.length -= 1
        return {"type": "event", "event": "Scalar"}

class TestETFParser(unittest.TestCase):
    def test_01_small_tuple_decoding(self):
        """Valid SMALL_TUPLE_EXT emits ListStart with correct arity and ListEnd."""
        payload = encode_etf_small_tuple([
            encode_etf_atom("ok"),
            encode_etf_atom("connected")
        ])
        state = MockDedState(payload)

        r1 = state.step()
        self.assertEqual(r1["type"], "event")
        self.assertEqual(r1["event"], "ListStart")
        self.assertEqual(r1["arity"], 2)

        r2 = state.step()
        self.assertEqual(r2["event"], "Atom")
        self.assertEqual(r2["value"], "ok")

        r3 = state.step()
        self.assertEqual(r3["event"], "Atom")
        self.assertEqual(r3["value"], "connected")

        r4 = state.step()
        self.assertEqual(r4["event"], "ListEnd")

    def test_02_truncated_small_tuple_handled_safely(self):
        """Truncated SMALL_TUPLE_EXT (tag without arity byte) must return EndedUnexpectedly error."""
        truncated = bytes([ETF_SMALL_TUPLE])
        state = MockDedState(truncated)
        res = state.step()
        self.assertEqual(res["type"], "error")
        self.assertEqual(res["error"], "EndedUnexpectedly")

    def test_03_zero_arity_tuple(self):
        """0-element empty tuple {} decodes to ListStart(0) immediately followed by ListEnd."""
        empty_tuple = encode_etf_small_tuple([])
        state = MockDedState(empty_tuple)
        r1 = state.step()
        self.assertEqual(r1["event"], "ListStart")
        self.assertEqual(r1["arity"], 0)
        r2 = state.step()
        self.assertEqual(r2["event"], "ListEnd")

    def test_04_stack_depth_limit_protection(self):
        """Decoder must refuse to underflow frame index when reaching maximum recursion depth (index 0)."""
        state = MockDedState(bytes([ETF_SMALL_TUPLE, 1]))
        state.index = 0
        state.frames[0] = 0x8000002 # Active tuple with 1 element remaining
        res = state.step()
        self.assertEqual(res["type"], "error")
        self.assertEqual(res["error"], "ExceededSizeLimit")

    def test_05_unknown_map_key_discard_simulation(self):
        """Simulate discarding unknown map keys without aborting gateway sync."""
        known_keys = {"guild_id", "name", "channels"}
        test_map = {
            "guild_id": 123456789,
            "new_discord_experimental_feature_flag": True,
            "name": "General Guild",
        }

        parsed_keys = {}
        for k, v in test_map.items():
            if k in known_keys:
                parsed_keys[k] = v

        self.assertIn("guild_id", parsed_keys)
        self.assertIn("name", parsed_keys)
        self.assertNotIn("new_discord_experimental_feature_flag", parsed_keys)

if __name__ == "__main__":
    unittest.main()
