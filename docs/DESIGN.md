# Design Notes

## Memory-only B+ Tree

Each node is a C++ object. Internal nodes store separator keys and child pointers. Leaf nodes store sorted keys and payload strings and are linked through a `next` pointer.

Insertion descends using `upper_bound`, inserts into a leaf, and splits on overflow. A separator is recursively propagated toward the root. Search descends to one leaf using the same separator rule and then performs `lower_bound` inside the leaf.

## Disk-only B+ Tree

The index is a page-oriented binary file with a fixed 4,096-byte page size.

```text
index.dat
+----------------------+  page 0
| metadata             |
+----------------------+  page 1
| B+ tree node         |
+----------------------+  page 2
| B+ tree node         |
+----------------------+
| ...                  |
```

Payload bytes are kept separately:

```text
payload.dat
[length][payload bytes][length][payload bytes]...
```

A leaf therefore stores a key and 64-bit payload-file offset instead of copying a large variable-length payload into the fixed-size index page.

This is close to a database index in which a leaf stores a key plus record identifier/pointer.

### Serialized leaf page

```text
32-byte header
(key[32], payload_offset[8]) * N
```

### Serialized internal page

```text
32-byte header
first_child[8]
(key[32], right_child[8]) * N
```

Page capacity is derived from the page layout rather than hard-coded as a toy B+ Tree order.

## Hybrid B+ Tree

Hybrid mode uses exactly the same disk representation but adds an application-level LRU cache of deserialized index pages.

```text
search/insert
    |
    v
LRU page cache ---- hit ----> node
    |
   miss
    v
index.dat
```

The cache uses write-through updates. This makes correctness simple and means the main effect of the cache is to reduce page reads, especially for root and internal pages that are repeatedly traversed.

## Fair benchmarking

The dataset is streamed from CSV; it is not first loaded completely into a vector. Search records are chosen using reservoir sampling, allowing a uniform fixed-size sample from a 10-million-row stream.

Each implementation should be launched as a separate process. Linux `getrusage()` is used for peak resident-set memory, so process isolation prevents one implementation's peak from contaminating another's result.

## I/O interpretation

Page-I/O counters are logical operations performed by the B+ Tree implementation. Linux's filesystem page cache can make physical device I/O smaller than logical I/O, particularly on repeated reads. Disk-only means there is no application-level B+ Tree cache; it does not attempt to disable the operating system's normal filesystem cache.
