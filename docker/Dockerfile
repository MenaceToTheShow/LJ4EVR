# LJ4EVR CI image (design 9.3, ME-20): the self-hosted Linux runners have no cmake,
# so every Linux job runs inside this. Built and pushed by .github/workflows/docker.yml
# to ghcr.io/unbannablegt/lj4evr-ci:<short-sha> and :latest.
FROM debian:trixie

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      gcc clang cmake ninja-build python3 git ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# uid 1000 = the runner user on atlas-ci2, so the mounted workspace is owned by
# the user that builds in it; safe.directory covers the case where it is not.
RUN useradd --uid 1000 --create-home ci \
 && git config --system --add safe.directory '*'
USER ci
WORKDIR /home/ci
