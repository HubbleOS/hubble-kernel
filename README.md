# kernel

```bash
make
make run
make build
make flash
```

```bash
docker-compose -f docker-compose.yml build

docker-compose build x86-builder
docker-compose build arm64-builder

docker-compose run x86-builder

docker-compose run --rm x86-builder bash
```