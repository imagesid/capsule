#!/usr/bin/env bash
set -euo pipefail

BASE_URL="https://cache-datasets.s3.amazonaws.com/cache_dataset_oracleGeneral/2007_msr"
OUT_DIR="data/msr"

mkdir -p "${OUT_DIR}"
cd "${OUT_DIR}"

FILES=(
  msr_hm_0.oracleGeneral.zst
  msr_prn_0.oracleGeneral.zst
  msr_prn_1.oracleGeneral.zst
  msr_proj_0.oracleGeneral.zst
  msr_proj_1.oracleGeneral.zst
  msr_proj_2.oracleGeneral.zst
  msr_proj_4.oracleGeneral.zst
  msr_prxy_0.oracleGeneral.zst
  msr_prxy_1.oracleGeneral.zst
  msr_src1_0.oracleGeneral.zst
  msr_src1_1.oracleGeneral.zst
  msr_usr_1.oracleGeneral.zst
  msr_usr_2.oracleGeneral.zst
  msr_web_2.oracleGeneral.zst
)

for f in "${FILES[@]}"; do
  echo "Downloading $f ..."
  wget -c "${BASE_URL}/${f}"
done

echo "All MSR traces downloaded to ${OUT_DIR}"
