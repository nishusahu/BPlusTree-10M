# Dataset location

Do **not** commit the 10-million-record CSV to GitHub.

Either place it here as:

```text
data/dataset.csv
```

or keep it anywhere on your machine and pass its absolute path to the benchmark scripts.

Expected columns:

```csv
key,payload
K026A13E,someRandomPayload...
```

The implementation expects unique keys. Disk/hybrid mode supports keys up to 32 bytes.
