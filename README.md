# kernel

```bash
make
```


```bash
docker-compose build x86-builder
docker-compose run --rm x86-builder bash            
docker-compose run --rm x86-builder make            
docker-compose run --rm x86-builder make clean            
```

```bash
qemu-system-x86_64 \
        -drive if=pflash,format=raw,readonly=on,file=ovmf/OVMF_CODE.fd \
        -drive if=pflash,format=raw,file=ovmf/OVMF_VARS.fd \
        -hda fat:rw:iso \
        -m 512 \
        -vga std \
        -serial stdio
```


```bash
        # qemu-system-x86_64 \
#               -M pc \
#               -drive if=pflash,format=raw,readonly=on,file=ovmf/OVMF_CODE.fd \
#               -drive if=pflash,format=raw,file=ovmf/OVMF_VARS.fd \
#               -hda fat:rw:iso \
#               -m 512 \
#               -serial stdio \
qemu-system-x86_64 \

  -M pc \
  -cpu Haswell \
  -smp 2 \
  -m 1024 \
  -drive if=pflash,format=raw,readonly=on,file=ovmf/OVMF_CODE.fd \
    -drive if=pflash,format=raw,file=ovmf/OVMF_VARS.fd \
          -hda fat:rw:iso \
               -serial stdio \
```