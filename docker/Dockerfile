# LJ4EVR CI image (design 9.3, ME-20): the self-hosted Linux runners have no cmake,
# so every Linux job runs inside this. Built and pushed by .github/workflows/docker.yml
# to ghcr.io/unbannablegt/lj4evr-ci:<short-sha> and :latest.
FROM debian:trixie

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      gcc clang cmake ninja-build python3 git ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# Root inside the container: the runner writes its file commands (save_state, outputs)
# into _temp as the host runner user, and a container user with another uid cannot
# open them (EACCES, seen on atlas-ci2). git refuses a workspace owned by another uid
# without safe.directory.
RUN git config --system --add safe.directory '*'
