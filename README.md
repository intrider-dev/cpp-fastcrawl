# HighPerfCrawler

HighPerfCrawler is a high-performance, multithreaded C++ crawler designed to scan massive lists of websites and detect specified keywords. It is built for speed, efficiency, and low memory usage, using libcurl's multi interface and Linux epoll.

## Features

- **Multithreaded architecture** for parallel processing of thousands of URLs.
- **Asynchronous HTTP(S) requests** using curl multi API + epoll.
- **Thread-safe streaming output** to a result file (no in-memory accumulation).
- **Keyword filtering** (e.g. `--contains=bitrix,aspro`).
- **Thread-safe logging** (to stderr or to a file).
- **Clean CLI interface** — all parameters are configurable via command line.
- **Highly scalable** — suitable for millions of domains.

## Build

Dependencies:
- `g++` with C++17 support
- `cmake`
- `libcurl` development headers
- Linux (or WSL / Unix-like system with epoll)

### Build instructions

```sh
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage

```sh
./HighPerfCrawler --input=domains.txt --output=out.csv --threads=8 [debug] [--contains=bitrix,aspro] [--logfile=log.txt]
```

**Arguments:**
- `--input=<file>` — input file with one domain per line.
- `--output=<file>` — path to write the matched domains.
- `--threads=<N>` — number of threads (typically equal to CPU cores).
- `debug` — optional, enables debug-level logging.
- `--contains=<words>` — optional comma-separated keywords to match in HTML.
- `--logfile=<file>` — optional log output file (default: stderr).

### Example

```sh
./HighPerfCrawler --input=domains.txt --output=matched.csv --threads=8 --contains=bitrix,aspro --logfile=log.txt debug
```

The output will contain only the domains whose HTML contains at least one of the specified keywords.

## License

This project is licensed under the MIT License.

---

**Pull requests, bug reports, and contributions are welcome!**
