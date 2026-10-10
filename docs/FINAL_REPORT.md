# B+ Tree Implementation and Performance Analysis

## 1. Objective

The objective of this assignment is to implement and compare three variants of a B+ Tree for a large dataset containing 10,000,000 randomly shuffled records:

1. Memory-only B+ Tree
2. Disk-only B+ Tree
3. Memory + Disk (Hybrid) B+ Tree

Each implementation is evaluated using insertion time, search time, memory usage, disk usage, tree height, and logical disk I/O wherever applicable.

---

## 2. Dataset

The provided CSV dataset contains:

- Total data records: 10,000,000
- Header rows: 1
- Columns: `key`, `payload`
- Key type: unique alphanumeric string
- Key length: exactly 8 characters
- Payload length: exactly 64 characters
- Input order: randomly shuffled

Example:

```text
key,payload
K026A13E,vwWk6rHLNHGb78UpfkPxeoLWTpUk72dIfP6I7Tv9Ej3yxqanwNUzsXUlqYfBhB9p
K00B8035,0PMSfJTEab9ZV43Psgw8Kg2KKHLrYLbIgA1wHE2q8RNydoHzFgkhgZ277RW4E4s7
```

The randomly shuffled input is useful because it exercises normal B+ Tree node splitting behavior instead of simply inserting already sorted keys.

---

## 3. Implementation Environment

The experiment was executed on the following system:

- Operating System: Ubuntu 26.04 LTS
- Kernel: 7.0.0-31-generic
- Processor: 13th Gen Intel Core i7-13620H
- Logical CPUs: 16
- Physical cores: 10
- Threads per core: 2
- RAM: 30 GiB
- Storage: PHISON EM280512GMN48-PE21T1AS NVMe SSD, 476.9 GB
- Compiler: g++ 15.2.0
- CMake: 4.2.3
- Language standard: C++17

The three modes were run as separate processes so that peak process memory measurements were not contaminated by the previous implementation.

---

## 4. B+ Tree Design

### 4.1 Memory-only B+ Tree

The complete B+ Tree is maintained in main memory. Internal nodes hold separator keys and child pointers, while leaves hold keys and payloads. Leaf nodes are linked to support sequential traversal.

The configured maximum number of keys per node is 100.

Advantages:

- Very low search latency
- Very high insertion throughput
- No index page I/O after the input data is read

Disadvantage:

- Memory usage grows with the number of indexed records

### 4.2 Disk-only B+ Tree

The disk-based implementation stores the B+ Tree in fixed-size pages.

Assumptions:

- Page size: 4096 bytes
- Maximum stored key size: 32 bytes
- Page identifier: fixed-width integer
- Derived internal-node capacity: 101 keys/page
- Derived leaf capacity: 101 entries/page
- Payloads are stored separately from the B+ Tree index

The index file stores the tree pages, while a separate payload file stores variable record payloads. Leaf entries map a key to the payload's location instead of storing the complete payload inside each index page.

This design avoids loading the complete index into main memory.

### 4.3 Hybrid Memory + Disk B+ Tree

The hybrid implementation uses the same disk-based B+ Tree format but adds an in-memory LRU page cache.

Configuration:

- Cache size: 4096 pages
- Page size: 4096 bytes
- Nominal cache capacity: approximately 16 MiB

Frequently reused pages, such as the root and upper internal levels, can be served directly from memory instead of being read from the B+ Tree file repeatedly.

The current implementation uses write-through behavior: modified pages are still written to disk, which is why the hybrid mode mainly reduces page reads rather than page writes.

---

## 5. Experimental Procedure

Correctness and scalability were checked incrementally before executing the full benchmark.

The implementation was first tested with a generated smoke-test dataset. The actual assignment dataset was then tested at:

- 100,000 records
- 1,000,000 records
- 10,000,000 records

All three B+ Tree variants returned zero payload-verification failures at every tested scale.

The final 10-million-record experiment used:

- 10,000 sampled searches per implementation
- Random seed: 42
- Original shuffled insertion order

---

## 6. Final Results: 10,000,000 Records

| Metric | Memory-only | Disk-only | Hybrid |
|---|---:|---:|---:|
| Records inserted | 10,000,000 | 10,000,000 | 10,000,000 |
| Insertion time | **18.254 s** | 154.679 s | 143.018 s |
| Insertion throughput | **547,831 records/s** | 64,650 records/s | 69,922 records/s |
| Average search time | **1.690 us** | 13.088 us | 11.004 us |
| Peak RSS | 1685.76 MB | **5.61 MB** | 20.29 MB |
| Disk usage | 0 | 1212.17 MB | 1212.17 MB |
| Tree height | 4 | 4 | 4 |
| Nodes/pages | 145,648 | 144,298 | 144,298 |
| Insert page reads | 0 | 39,670,624 | 9,670,360 |
| Insert page writes | 0 | 10,288,589 | 10,288,589 |
| Search page reads | 0 | 40,000 | 12,559 |
| Search page writes | 0 | 0 | 0 |
| Cache hit rate | N/A | N/A | 75.62% |
| Verification failures | **0** | **0** | **0** |

---

## 7. Result Analysis

### 7.1 Insertion Performance

The memory-only implementation completed insertion of all 10 million records in 18.254 seconds, while the disk-only implementation required 154.679 seconds.

Therefore, for this experiment, memory-only construction was approximately:

```text
154.679 / 18.254 = 8.47x
```

faster than disk-only construction.

The reason is that the memory implementation performs normal pointer and memory accesses, while the disk implementation repeatedly accesses fixed-size index pages and writes updated pages back to the index file.

The hybrid implementation reduced insertion time from 154.679 seconds to 143.018 seconds, an improvement of approximately 7.54% over disk-only.

### 7.2 Search Performance

The average measured search times were:

- Memory-only: 1.690 us
- Disk-only: 13.088 us
- Hybrid: 11.004 us

Memory-only search was approximately 7.74x faster than disk-only search.

The hybrid cache improved average search time by approximately 15.92% compared with disk-only.

### 7.3 Memory Usage

The memory-only B+ Tree required 1685.76 MB of peak resident memory, approximately 1.65 GiB.

The disk-only tree required only 5.61 MB peak RSS because the complete tree was not kept as ordinary in-memory node objects.

The hybrid tree used 20.29 MB peak RSS. This is higher than the disk-only implementation because it intentionally reserves memory for cached pages, but it is still far smaller than the memory-only B+ Tree.

This demonstrates the main storage trade-off:

- Memory-only: highest RAM usage, lowest latency
- Disk-only: lowest RAM usage, highest I/O overhead
- Hybrid: modest extra RAM for lower disk-page activity

### 7.4 Disk I/O

The disk-only implementation performed 39,670,624 logical index page reads during insertion.

The hybrid implementation performed 9,670,360 insertion page reads.

This is a reduction of approximately 75.62%.

For the 10,000 searches:

- Disk-only search page reads: 40,000
- Hybrid search page reads: 12,559

The hybrid cache therefore reduced search page reads by approximately 68.60%.

The hybrid cache hit rate across the measured run was 75.62%.

### 7.5 Search-I/O Sanity Check

The disk-only tree height was 4 and 10,000 searches were performed.

The measured number of disk-only search page reads was:

```text
10,000 searches x 4 levels = 40,000 page reads
```

which exactly matches the reported 40,000 page reads.

This confirms that, without the application-level cache, the search algorithm traversed approximately one page at each B+ Tree level.

### 7.6 Page Writes

Both disk-only and hybrid modes reported 10,288,589 insertion page writes.

This is expected because the hybrid implementation uses a write-through policy. Pages may be served from the memory cache for reads, but modified pages are still written to disk to keep persistent storage synchronized.

Hence, the cache significantly reduces reads but does not reduce writes in the current implementation.

---

## 8. Scalability

The implementation was also measured at smaller scales before the final experiment.

### 8.1 Tree Height

| Records | Memory | Disk | Hybrid |
|---:|---:|---:|---:|
| 100,000 | 3 | 3 | 3 |
| 1,000,000 | 4 | 4 | 4 |
| 10,000,000 | 4 | 4 | 4 |

The tree height grows very slowly because each node has a large fanout. Even for 10 million records, only four levels were required.

### 8.2 Memory-only RAM Growth

| Records | Peak RSS |
|---:|---:|
| 100,000 | 21.21 MB |
| 1,000,000 | 172.67 MB |
| 10,000,000 | 1685.76 MB |

Memory usage grows approximately linearly with the number of records.

### 8.3 Disk Storage Growth

| Records | Disk usage |
|---:|---:|
| 100,000 | 12.12 MB |
| 1,000,000 | 121.50 MB |
| 10,000,000 | 1212.17 MB |

On-disk storage also grows approximately linearly.

---

## 9. Important Measurement Note

The page read/write counters in this project represent **logical B+ Tree page I/O operations issued by the implementation**.

They should not be interpreted as a direct measurement of raw physical NVMe device transactions. The operating system may satisfy file accesses through its filesystem/page cache.

For this reason, the I/O counters are used to compare the algorithms consistently at the application level rather than to claim an exact count of physical SSD operations.

---

## 10. Conclusions

All three implementations successfully indexed and searched the complete 10-million-record shuffled dataset with zero verification failures.

The memory-only B+ Tree achieved the highest performance, completing insertion in 18.254 seconds and averaging 1.690 us per sampled search, but it consumed approximately 1.65 GiB of RAM.

The disk-only B+ Tree reduced application memory usage to only 5.61 MB, but insertion time increased to 154.679 seconds because of page-based file access and persistent writes.

The hybrid B+ Tree provided a useful compromise. With only 20.29 MB peak RSS, it reduced insertion page reads by approximately 75.62% and search page reads by approximately 68.60% compared with disk-only. It also improved insertion and search times while preserving the same persistent index structure.

The experiment demonstrates why B+ Trees are well suited to database indexing: their high fanout keeps tree height small even for millions of records, while page-oriented storage and caching allow a system to balance memory consumption against I/O performance.
