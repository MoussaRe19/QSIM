## Phase 1 — Future Event List (FEL)

**Objective:** Implement a core priority queue component to handle event scheduling.

### Components

- **`EventNotice`**: Structure to store event metadata, timestamps, tracking indices, and validity states.
    
- **`FEL` Operations**: Core min-heap mechanics to support extraction, insertion, peek, and lazy cancellation.
    

### Key Requirements

|**Operation**|**Description**|
|---|---|
|**Insertion & Extraction**|Standard min-heap behavior prioritizing the earliest timestamp.|
|**Secondary Ordering**|Tie-breaking logic for identical timestamps (e.g., using unique IDs).|
|**Cancellation**|$O(1)$ invalidation strategy using node tracking.|

### Verification Criteria

- **Correct Ordering**: Elements are extracted in strict chronological order.
    
- **State Management**: Invalidated events are handled properly without breaking heap integrity.
    
- **Memory Safety**: Code must compile with strict warning flags and run without memory leaks or undefined behavior.