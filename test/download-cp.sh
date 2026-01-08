#!/usr/bin/env bash
set -euo pipefail

BASE_URL="https://cache-datasets.s3.amazonaws.com/cache_dataset_oracleGeneral/2015_cloudphysics"
OUT_DIR="data/cp"

mkdir -p "${OUT_DIR}"
cd "${OUT_DIR}"

FILES=(
  w90.oracleGeneral.bin.zst
  w91.oracleGeneral.bin.zst
  w92.oracleGeneral.bin.zst
  w93.oracleGeneral.bin.zst
  w94.oracleGeneral.bin.zst
  w95.oracleGeneral.bin.zst
)

for f in "${FILES[@]}"; do
  echo "Downloading $f ..."
  wget -c "${BASE_URL}/${f}"
done

echo "CP traces downloaded to ${OUT_DIR}"
