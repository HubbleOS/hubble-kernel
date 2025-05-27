# docker.mk

.PHONY: docker-build docker-run docker-%

DOCKER_CMDS := build run

docker-build:
	@${DOCKER_RUN} make

docker-run: docker-build
	@make host-run

docker-%:
ifneq (,$(filter $*,$(DOCKER_CMDS)))
	@echo "🛠️  Running '$*' inside Docker..."
	@${DOCKER_RUN} make $*
else
	@make $*
endif
