"""
Master Test Runner: Executes all unit, regression, protocol, and security test suites.
"""

import sys
import unittest
import time

def run_tests():
    print("=" * 70)
    print("RIPCORD AUTOMATED VERIFICATION & TEST SUITE")
    print("=" * 70)

    loader = unittest.TestLoader()
    suite = loader.discover(start_dir=r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\tests", pattern="test_*.py")

    start_time = time.time()
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    elapsed = time.time() - start_time

    print("\n" + "=" * 70)
    print("TEST SUITE EXECUTION SUMMARY")
    print("=" * 70)
    print(f"Total Tests Run : {result.testsRun}")
    print(f"Successes       : {result.testsRun - len(result.failures) - len(result.errors)}")
    print(f"Failures        : {len(result.failures)}")
    print(f"Errors          : {len(result.errors)}")
    print(f"Skipped         : {len(result.skipped)}")
    print(f"Total Time      : {elapsed:.3f} seconds")
    print("=" * 70)

    if result.wasSuccessful():
        print("RESULT: ALL AUTOMATED TESTS PASSED.")
        return 0
    else:
        print("RESULT: TESTS FAILED.")
        return 1

if __name__ == "__main__":
    sys.exit(run_tests())
