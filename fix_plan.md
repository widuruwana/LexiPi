# Fix Plan: Revamp Game Logic and Ensure Test Player Accuracy

## Objective
Fix the critical "unplayable" state caused by pass-by-value errors in the game engine and verify the accuracy of the test player.

## Steps

### 1. Fix `executePlayMove` Signature
*   **File**: `include/choices.h`
*   **Action**: Change the function signature to accept `Player` by reference.
    *   `bool executePlayMove(..., Player &currentPlayer, ...);`
*   **File**: `src/choices.cpp`
*   **Action**: Update the implementation to match the header.
    *   `bool executePlayMove(..., Player &currentPlayer, ...) { ... }`

### 2. Verify and Fix Other State Modifiers
*   **File**: `include/choices.h` & `src/choices.cpp`
*   **Action**: Review `executeExchangeMove` and other helper functions to ensure they also pass `Player` or `TileRack` by reference where modification is intended.
    *   `executeExchangeMove` already takes `Player &player`, so it should be fine, but double-check.

### 3. Update `runGame` Call Site
*   **File**: `src/game_engine.cpp`
*   **Action**: Ensure the call to `executePlayMove` in `runGame` passes the player correctly.
    *   Current: `executePlayMove(..., players[currentPlayer], ...)`
    *   Since `players[currentPlayer]` returns a reference to the element in the array, this call site likely doesn't need changing *if* the function signature is fixed to take a reference.

### 4. Verify Test Player Logic
*   **File**: `src/test_player.cpp`
*   **Action**: No code changes required for `TestPlayer` itself, as it correctly implements the interface.
*   **File**: `src/modes/Test/test_mode.cpp`
*   **Action**: Verify the test scenario.
    *   P1 plays "HELLO".
    *   P2 plays "WRLD" (forming "WORLD").
    *   Ensure the `Move::Play` calls use the correct rack letters (excluding existing board letters). The current code seems correct ("WRLD" instead of "WORLD").

### 5. Compile and Run Test Mode
*   **Action**: Compile the project.
*   **Action**: Run the test mode.
*   **Verification**:
    *   Check if P1's score updates after the first move.
    *   Check if P1's rack is replenished (or at least tiles removed).
    *   Check if P2's move is valid and score updates.
    *   Ensure the game ends correctly (after 6 passes or empty rack).

## Verification Plan
1.  **Build**: `cmake --build build`
2.  **Run**: Execute the binary and select Test Mode.
3.  **Observe**: Watch the output for score updates and rack changes.