#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Copyright (c) 2019 FXTC developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Check include guards.

export LC_ALL=C
HEADER_ID_PREFIX="BITCOIN_"
## FXTC BEGIN
HEADER_ID_PREFIX_LITECOIN="LITECOIN_"
HEADER_ID_PREFIX_TALKCOIN="TALKCOIN_"
HEADER_ID_PREFIX_DASH="DASH_"
HEADER_ID_PREFIX_LYRA="LYRA_"
HEADER_ID_PREFIX_PIVX="PIVX_"
HEADER_ID_PREFIX_RNRT="RNRT_"
HEADER_ID_PREFIX_ZCOIN="ZCOIN_"
HEADER_ID_PREFIX_FXTC="FXTC_"
HEADER_ID_SUFFIX="_H"
## FXTC END

REGEXP_EXCLUDE_FILES_WITH_PREFIX="src/(crypto/ctaes/|leveldb/|secp256k1/|tinyformat.h|univalue/)"

EXIT_CODE=0
for HEADER_FILE in $(git ls-files -- "*.h" | grep -vE "^${REGEXP_EXCLUDE_FILES_WITH_PREFIX}")
do
    ## FXTC BEGIN
    ##HEADER_ID_BASE=$(cut -f2- -d/ <<< "${HEADER_FILE}" | sed "s/\.h$//g" | tr / _ | tr "[:lower:]" "[:upper:]")
    HEADER_ID_BASE=$(cut -f2- -d/ <<< "${HEADER_FILE}" | sed "s/\.h$//g" | tr / _| tr - _ | tr "[:lower:]" "[:upper:]")
    ## FXTC END
    HEADER_ID="${HEADER_ID_PREFIX}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    ## FXTC BEGIN
    ##if [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID}" "${HEADER_FILE}") != 2 ]]; then
    HEADER_ID_LITECOIN="${HEADER_ID_PREFIX_LITECOIN}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    HEADER_ID_TALKCOIN="${HEADER_ID_PREFIX_TALKCOIN}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    HEADER_ID_DASH="${HEADER_ID_PREFIX_DASH}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    HEADER_ID_LYRA="${HEADER_ID_PREFIX_LYRA}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    HEADER_ID_PIVX="${HEADER_ID_PREFIX_PIVX}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    HEADER_ID_RNRT="${HEADER_ID_PREFIX_RNRT}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    HEADER_ID_ZCOIN="${HEADER_ID_PREFIX_ZCOIN}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    HEADER_ID_FXTC="${HEADER_ID_PREFIX_FXTC}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    if [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID}" "${HEADER_FILE}") != 2 ]] &&
       [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID_LITECOIN}" "${HEADER_FILE}") != 2 ]] &&
       [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID_TALKCOIN}" "${HEADER_FILE}") != 2 ]] &&
       [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID_DASH}" "${HEADER_FILE}") != 2 ]] &&
       [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID_LYRA}" "${HEADER_FILE}") != 2 ]] &&
       [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID_PIVX}" "${HEADER_FILE}") != 2 ]] &&
       [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID_RNRT}" "${HEADER_FILE}") != 2 ]] &&
       [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID_ZCOIN}" "${HEADER_FILE}") != 2 ]] &&
       [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID_FXTC}" "${HEADER_FILE}") != 2 ]]; then
    ## FXTC END
        echo "${HEADER_FILE} seems to be missing the expected include guard:"
        echo "  #ifndef ${HEADER_ID}"
        echo "  #define ${HEADER_ID}"
        echo "  ..."
        echo "  #endif // ${HEADER_ID}"
        echo
        EXIT_CODE=1
    fi
done
exit ${EXIT_CODE}
