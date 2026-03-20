\# Alternate-Pi: Optimization Retrospective \& Technical Documentation



\*\*Date:\*\* January 12, 2026

\*\*Project Goal:\*\* Optimize a C++ Scrabble/GADDAG engine from single-core performance to maximum multi-core saturation (>2,500 games/sec).

\*\*Current Status:\*\* Stable at ~460-510 games/sec (CPU Compute Bound).



---



\## 1. Architectural Overhaul: Threading Model

\*\*Initial Problem:\*\* The engine was single-threaded or using inefficient `std::async` calls per game, leading to thread oversubscription and context-switching overhead.



\### 1.1 Batch Processing Implementation

\* \*\*Change:\*\* Replaced per-game thread spawning with a \*\*Worker Pool model\*\*.

\* \*\*Implementation:\*\* Created `N` threads (where `N = hardware\_concurrency`), each running a continuous loop of `TotalGames / N`.

\* \*\*Outcome:\*\* Eliminated thread creation overhead. CPU usage stabilized at 100%, but throughput remained low due to locking.



\### 1.2 Random Number Generator (RNG) Fixes

\* \*\*Issue:\*\* `std::shuffle` and `std::mt19937` using `static` variables caused massive contention. All threads were fighting for the internal state lock of the RNG.

\* \*\*Fix:\*\*

&nbsp;   \* Removed `static` RNG instances.

&nbsp;   \* Implemented `thread\_local` RNG seeding in `main.cpp` / `aiai.cpp`.

&nbsp;   \* Ensured independent deterministic replay capability per thread.



---



\## 2. Memory Optimization: The "Malloc" War

\*\*Critical Bottleneck:\*\* On Windows/MinGW, the system allocator (`malloc`/`free`) employs a global lock. High-frequency allocations serialized the 6 worker threads into a single line.



\### 2.1 The `Move` Struct (The "Trojan Horse")

\* \*\*Problem:\*\* `struct Move` contained `std::vector<Tile> tiles`. Even when unused, the vector constructor/destructor ran ~300,000 times/sec per thread.

\* \*\*Fix:\*\* Converted `Move` to a \*\*POD (Plain Old Data)\*\* type.

&nbsp;   \* Removed `std::vector`.

&nbsp;   \* Implemented Small Buffer Optimization (SBO) using `char word\[16]` and `exchangeLetters\[8]`.

\* \*\*Impact:\*\* Massive reduction in heap contention.



\### 2.2 The Referee \& Scoring

\* \*\*Problem:\*\* `Referee::validateMove` created temporary `std::vector<bool>` and `std::vector<NewTile>` for every validation check.

\* \*\*Fix:\*\* Replaced all dynamic containers with \*\*Stack Arrays\*\* (e.g., `bool used\[7]`, `PlacedTile placed\[15]`).

\* \*\*Result:\*\* Validation became strictly stack-bound (zero allocation).



\### 2.3 String Allocations in Hot Paths

\* \*\*Problem:\*\* `AIPlayer::calculateDifferential` and logging functions were building `std::string` objects via concatenation (`+=`) inside the move generation loop.

\* \*\*Fix:\*\*

&nbsp;   \* Rewrote `calculateDifferential` to use `char` buffers.

&nbsp;   \* Changed `MoveResult::message` from `std::string` to `const char\*` (static literals).

&nbsp;   \* Removed `MoveResult::wordsFormed` (expensive vector) from the hot path; moved reconstruction to the UI layer (`GameDirector`).



\### 2.4 State Management

\* \*\*Problem:\*\* `snapshot = state.clone()` was allocating new memory for every turn.

\* \*\*Fix:\*\* Changed to `snapshot = state` (Copy Assignment), which reuses existing vector capacity.



---



\## 3. Logic \& Algorithmic Fixes



\### 3.1 The "Hyper-Speed" Anomaly (5,000 Games/Sec)

\* \*\*Incident:\*\* Simulation speed briefly spiked to 5,000 games/sec with weird results (scores ~0).

\* \*\*Root Cause:\*\* A memory safety bug in `heuristics.h` (`(letter \& 31) - 1`) accessed index `-1` or `30`, returning garbage scores.

\* \*\*Mechanism:\*\* Bots saw 0 points for every move, entered an infinite \*\*Exchange Loop\*\* (instant moves), and passed out once the bag empty. The board remained empty, bypassing 99% of the GADDAG calculation.

\* \*\*Fix:\*\* Implemented safe, localized lookup tables for point values.



\### 3.2 Scoring Desynchronization

\* \*\*Issue:\*\* `calculateTrueScoreFast` was desynchronized when scanning prefixes, causing valid hooks to be scored incorrectly.

\* \*\*Fix:\*\* Rewrote the scanning logic to strictly separate Prefix (Backwards scan), Main Word (Candidate scan), and Suffix (Forward scan).



\### 3.3 Mechanics Inlining

\* \*\*Optimization:\*\* Moved the "True Score" calculation from `mechanics.cpp` to a \*\*Templated Header Implementation\*\* (`mechanics.h`).

\* \*\*Benefit:\*\* Allowed the compiler to inline the scoring logic directly into the `MoveGenerator` loop, removing function call overhead.



---



\## 4. The Computational Ceiling (Current State)



\### 4.1 The Limit

\* \*\*Throughput:\*\* ~460-510 games/sec.

\* \*\*Bottleneck:\*\* Raw CPU Compute.

\* \*\*Analysis:\*\* The engine performs ~900,000 full-board constraint generations per second (15 rows + 15 cols × 60 turns × 500 games). This saturates the ALU/Branch Prediction capacity of the hardware.



\### 4.2 Failed/Rejected Optimizations

1\.  \*\*Incremental Caching:\*\* Attempted to cache Row constraints.

&nbsp;   \* \*Outcome:\* Overhead of state management (Dirty Flags) likely exceeded the cost of brute-force generation on small boards (15x15).

2\.  \*\*Lookahead Pruning (Branch \& Bound):\*\* Attempted to add `maxScore` to GADDAG nodes to prune low-scoring branches.

&nbsp;   \* \*Outcome:\* User reported "It didn't work." Likely due to the cost of the extra conditional checks inside the tight recursion loop outweighting the savings, or the "Loose Upper Bound" being too generous to prune effectively.



---



\## 5. File Manifest (Stable Check)



\* \*\*`types.h`\*\*: POD `Move` and `MoveResult` structs (Zero Allocation).

\* \*\*`mechanics.h`\*\*: Inlined, templated, safe scoring logic.

\* \*\*`referee.cpp`\*\*: Stack-based move validation.

\* \*\*`game\_director.cpp`\*\*: Lazy word reconstruction (UI only).

\* \*\*`ai\_player.cpp`\*\*: Stack-based differential calculations; no strings in hot loop.

\* \*\*`dictionary.cpp`\*\*: Standard GADDAG build (Fast Indexing).

