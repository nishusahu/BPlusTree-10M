# Validation performed before packaging

The source package was built and tested before the ZIP was created.

## Checks performed

1. Release configuration and compilation with CMake/GNU C++.
2. `ctest` correctness suite:
   - 5,000 shuffled records,
   - memory-only tree,
   - disk-only tree,
   - hybrid tree,
   - every inserted key searched,
   - exact payload equality checked,
   - missing-key lookup checked,
   - splits/root growth forced.
3. AddressSanitizer + UndefinedBehaviorSanitizer build and the same C++ test suite.
4. End-to-end smoke benchmark on 20,000 generated shuffled records for all three modes.
5. Additional end-to-end benchmark on 100,000 generated shuffled records for all three modes.

All checks completed with zero correctness/verification failures at packaging time.

These tests validate the code itself; the final 10-million-row timings must still be generated on the student's machine using the actual assignment dataset.
