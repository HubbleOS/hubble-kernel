import tomllib

with open("config.toml", "rb") as f:
    config = tomllib.load(f)

print(config["ARCH"])
print(config["MEM"])
print(config["QEMU"]["ISO_PATH"])
