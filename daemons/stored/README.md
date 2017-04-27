# mongodb-store

A store implementation using MongoDB in C

## Dependency

- mongo-c-driver-1.4.2

## Install

```
> ./setup.sh
> make
```

## Run Cache Daemon

```
> ./store
```

## Run Example

```
> ./mstore
```

## Run Test - Cache Daemon API

```
Usage: ./mstore-test [OPTIONS]
  -p            Amount of put operations in total.
  -t            Number of threads.
  -s            Chunk size (byte).
  -h            Displays this help.
```

## Run Test - Cache API

```
> make store-test
> ./store-test
```

## Result

## Todo

- implement garbage collector thread
- memory leak testing
