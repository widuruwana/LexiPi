\# Optimization Log: Project Alternate-Pi



\*\*Objective:\*\* Increase simulation throughput from ~500 games/sec (Single Core speed) to >2,500 games/sec (Multi-Core saturation) on a 6-core MinGW/Windows environment.



\*\*Current Status:\*\* ~464 games/sec (Serialized).

\*\*Primary Bottleneck:\*\* Heap Contention / Thread Serialization.



---



\## 1. Threading Architecture

\### Attempt

\* \*\*Initial Approach:\*\* `std::async` launching a new thread for every single game.

\* \*\*Issue:\*\* Thread Oversubscription. Context switching overhead exceeded computation time.

\* \*\*Fix:\*\* \*\*Batching\*\*. Created exactly `hardware\_concurrency` threads (6), each running a loop of ~1,600 games.

\* \*\*Result:\*\* Logic verified, but throughput did not increase, indicating threads were blocking each other.



\## 2. RNG Contention

\### Attempt

\* \*\*Diagnosis:\*\* `static std::mt19937` in `shuffleTileBag` caused a data race. All threads fought for the internal state lock of the random number generator.

\* \*\*Fix:\*\*

&nbsp;   1.  Switched to `thread\_local`.

&nbsp;   2.  Implemented `FastRNG` (Xorshift) to bypass `std` overhead entirely.

\* \*\*Result:\*\* Essential for correctness, but did not unlock performance. The bottleneck moved elsewhere.



\## 3. The "Malloc" Bottleneck (Heap Contention)

\### Attempt

\* \*\*Diagnosis:\*\* On Windows (MinGW), the system memory allocator (`malloc`) often has a global lock. Creating/Destroying `std::vector` objects 10,000 times/sec serializes execution.

\* \*\*Fixes Applied:\*\*

&nbsp;   1.  \*\*TileBag Reuse:\*\* Implemented `refillStandardTileBag` to clear/refill vectors instead of reallocating.

&nbsp;   2.  \*\*Object Reuse:\*\* Moved `AIPlayer`, `Board`, and `GameDirector` instantiation \*\*outside\*\* the hot loop.

&nbsp;   3.  \*\*Snapshot Optimization:\*\* Changed `snapshot = state.clone()` (allocates memory) to `snapshot = state` (copies data into existing capacity).

\* \*\*Result:\*\* Theoretical contention reduced, but profiler evidence (stuck at ~500 gps) suggests a hidden allocation is still occurring in the hot path.



\## 4. Referee \& Scoring Optimization

\### Attempt

\* \*\*Diagnosis:\*\* `Referee::validateMove` was creating temporary `std::vector<bool>` and `std::vector<NewTile>` for every move validation (~300k times/sim).

\* \*\*Fix:\*\* Replaced all heap vectors with \*\*Stack Arrays\*\* (e.g., `Tile\[7]`, `PlacedTile\[15]`). Stack memory is private to the thread and lock-free.

\* \*\*Result:\*\* Code is allocation-free, yet performance remained flat.



\## 5. Micro-Optimizations

\### Attempt

\* \*\*Diagnosis:\*\* `std::toupper` and `isalpha` can acquire locale locks on Windows.

\* \*\*Fix:\*\* Replaced with raw bitwise operations: `c ^ 0x20` and lookup tables.

\* \*\*Result:\*\* Removed potential locale locks.



\## 6. The "Hyper-Speed" Anomaly (5,000 Games/Sec)

\### Incident

\* \*\*Cause:\*\* An unsafe array lookup in `calculateTrueScoreFast` (`(letter \& 31) - 1`) accessed index `-1` or `30`, returning garbage data.

\* \*\*Effect:\*\* The bots calculated `0` points for all moves.

\* \*\*Outcome:\*\* Bots effectively stopped playing. They entered an endless loop of \*\*Exchanging Tiles\*\* until the bag emptied, then \*\*Passed\*\* out.

\* \*\*Why it was fast:\*\* The simulation skipped the expensive part (Board Geometry/Hook generation) because the board remained empty.

\* \*\*Correction:\*\* Fixed `mechanics.h` with a safe lookup table `MECHANICS\_POINTS`. Performance returned to the accurate (but slow) ~464 games/sec.



---



\## Conclusion \& Next Steps

We have successfully converted the engine to a \*\*Zero-Allocation\*\* architecture during the game loop. The logic is now thread-safe and lock-free on paper.



\*\*Why are we still stuck?\*\*

The persistence of the ~500 games/sec limit (exactly single-core speed) strongly suggests \*\*False Sharing\*\* or \*\*External Locking\*\*:

1\.  \*\*Console I/O:\*\* Verify `verbose` is strictly OFF. Even a single `cout` can lock threads.

2\.  \*\*MinGW Threading:\*\* The `std::thread` implementation in some MinGW versions is a wrapper around Win32 threads that may have hidden serialization.

3\.  \*\*Hidden Globals:\*\* `gDictionary` is shared. If `MoveGenerator` reads it without `const` guarantees (or if internal caching triggers writes), threads will lock.

