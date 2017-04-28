# mongodb-store

A store implementation using MongoDB in C

## Dependency

- mongo-c-driver-1.4.2

## Install

```
> ./install.sh
> make
```

## Run Cache Daemon

```
> ./stored
```

## Run Test - Cache Daemon API (Single)

```
> ./stored-test
```

## Run Test - Cache Daemon API (Batch)

```
Usage: ./stored-test-batch [OPTIONS]
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

## Issue

- CID length is 20 bytes while MongoDB key length is 24 bytes [see this](https://mongodb.github.io/node-mongodb-native/api-bson-generated/objectid.html)
