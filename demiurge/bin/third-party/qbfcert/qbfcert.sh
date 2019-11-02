#! /bin/bash

readonly VERSION="1.0"

readonly BASEDIR="$(readlink -f "$(dirname $0)")"

readonly QBFSOLVER_SAT=10
readonly QBFSOLVER_UNSAT=20
readonly SATSOLVER_SAT=10
readonly SATSOLVER_UNSAT=20

readonly TMPDIR="$BASEDIR/tmp"


## tools

readonly TOOLBASEDIR="$BASEDIR"

readonly QBFSOLVER="$TOOLBASEDIR/depqbf/depqbf"
QBFSOLVER_PARAMS="--dep-man=simple"

readonly QRPCHECK="$TOOLBASEDIR/qrpcheck/qrpcheck"
QRPCHECK_PARAMS=" -p "

readonly QRPCERT="$TOOLBASEDIR/qrpcert/qrpcert"
QRPCERT_PARAMS=""

readonly CERTCHECK="$TOOLBASEDIR/certcheck/certcheck"
CERTCHECK_PARAMS=""

readonly SATSOLVER="$TOOLBASEDIR/lingeling/lingeling"
SATSOLVER_PARAMS=""


## files

readonly trace="$TMPDIR/qbfcert_trace$$.qrp"
readonly proof="$TMPDIR/qbfcert_proof$$.qrp"
readonly certificate="$TMPDIR/qbfcert_certificate$$.qrp"
readonly merged_cnf="$TMPDIR/qbfcert_merged_cnf$$.cnf"


## helpers

function log
{
  echo "*** qbfcert: $1" >> "$log_file"
}

function info
{
  echo "*** qbfcert: $1"
  log "$1"
}

function cleanup
{
  rm -f "$proof"
  rm -f "$certificate"
  rm -f "$trace"
  rm -f "$merged_cnf"
  if [[ x"$(readlink -f "$keep_cp_dir")" != x"$(readlink -f "$TMPDIR")" &&
        x"$(readlink -f "$keep_all_dir")" != x"$(readlink -f "$TMPDIR")" ]]; 
  then
    rm -f "$TMPDIR"/*.qrp
    rm -f "$TMPDIR"/*.aiger
    rm -f "$TMPDIR"/*.cnf
  fi
}

function clean_exit
{
  if [[ $1 != 0 ]]; then
    info "clean up and exit"
    info "FAILURE"
  fi
  cleanup
  exit $1
}

function die
{
  info "$1"
  clean_exit 1
}

function check_binary
{
  if [[ ! -f "$1" ]]; then
    die "no binary found at $1"
  fi
}


trap 'clean_exit 1' SIGHUP SIGINT SIGTERM


## options

check_icubes=true
qrp_format=bqrp
aiger_format=baiger
keep_cp_dir=undefined
keep_cp=false
keep_all_dir=undefined
keep_all=false
log_dir=undefined
log_file="/dev/null"
formula_name=undefined
error_msg=undefined

while [ $# -gt 0 ]
do
  case $1 in
    -h|--help) 
               echo "Usage: $0 [<OPTIONS>] <FORMULA>"
               echo
               echo "  Formula:"
               echo "    a Quantified Boolean Formula (QBF) in QDIMACS format"
               echo 
               echo "  Options:"
               echo "    -h, --help   print this message and exit"
               echo "    -i           disable initial cube checking"
               echo "    -f<format>   trace/proof format (default: bqrp)"
               echo "                   <format>: qrp (ascii QRP)"
               echo "                             bqrp (binary QRP)"
               echo "    -F<format>   certificate format (default: baiger)"
               echo "                   <format>: aiger (ascii AIGER)"
               echo "                             baiger (binary AIGER)"
               echo "    --keep=<dir> keep proof and certificate in <dir>"
               echo "    --Keep=<dir> keep all intermediate files in <dir>"
               echo "    --log=<dir>  log all tool output to <dir>"
               echo "    --version    print version number and exit"
               echo
               exit 0
               ;;
    -i)        check_icubes=false;;
    -f*)       qrp_format=`echo "$1"|sed -e 's,^-f,,'`
               if [[ ! x"$qrp_format" = xqrp &&
                     ! x"$qrp_format" = xbqrp ]]; then
                 #die "invalid format for option -f: '$qrp_format'"
                 if [[ x"$error_msg" == xundefined ]]; then
                   error_msg="invalid format for option -f: '$qrp_format'"
                 fi
               fi;;
    -F*)       aiger_format=`echo "$1"|sed -e 's,^-F,,'`
               if [[ ! x"$aiger_format" = xaiger &&
                     ! x"$aiger_format" = xbaiger ]]; then
                 #die "invalid format for option -F: '$aiger_format'"
                 if [[ x"$error_msg" == xundefined ]]; then
                   error_msg="invalid format for option -F: '$aiger_format'"
                 fi
               fi;;
    --keep=*)  keep_cp_dir=`echo "$1"|sed -e 's,^--keep=,,'`
               keep_cp=true
               if [[ x"$(readlink -f "$keep_cp_dir")" == \
                     x"$(readlink -f "$TMPDIR")" &&  
                     (x"$keep_all_dir" == xundefined ||
                      x"$(readlink -f "$keep_all_dir")" != \
                      x"$(readlink -f "$keep_cp_dir")") &&
                     x"$error_msg" == xundefined ]]; 
               then
                 info "<dir> in --keep=<dir> is tmp directory:"
                 info "  $(readlink -f $keep_cp_dir)"
                 info "  will not cleanup files from previous runs"
               elif [[ -f "$keep_cp_dir" ]]; then
                 die "'$keep_cp_dir' is not a directory"
               elif [[ ! -d "$keep_cp_dir" ]]; then
                 mkdir -p "$keep_cp_dir" 2> /dev/null
                 if [[ $? != 0 ]]; then
                   die "could not create directory: '$keep_cp_dir'"
                 fi
               fi
               if [[ x"$keep_all_dir" != xundefined &&
                     x"$(readlink -f "$keep_cp_dir")" != \
                     x"$(readlink -f "$keep_all_dir")" &&
                     x"$error_msg" == xundefined ]]; 
               then
                 info "mismatching directories for --(k|K)eep"
                 info "  will keep all files in:"
                 info "  $(readlink -f $keep_all_dir)"
                 keep_cp_dir=$keep_all_dir
               fi;;
    --Keep=*)  keep_all_dir=`echo "$1"|sed -e 's,^--Keep=,,'`
               keep_all=true
               if [[ x"$(readlink -f "$keep_all_dir")" == \
                     x"$(readlink -f "$TMPDIR")" && 
                     (x"$keep_cp_dir" == xundefined ||
                      x"$(readlink -f "$keep_all_dir")" != \
                      x"$(readlink -f "$keep_cp_dir")") &&
                     x"$error_msg" == xundefined ]];
               then
                 info "<dir> in --Keep=<dir> is tmp directory:"
                 info "  $(readlink -f $keep_all_dir)"
                 info "  will not cleanup files from previous runs"
               elif [[ -f "$keep_all_dir" ]]; then
                 die "'$keep_all_dir' is not a directory"
               elif [[ ! -d "$keep_all_dir" ]]; then
                 mkdir -p "$keep_all_dir" 2> /dev/null
                 if [[ $? != 0 ]]; then
                   die "could not create directory: '$keep_all_dir'"
                 fi
               fi
               if [[ x"$keep_cp_dir" == xundefined ]]; then
                 keep_cp_dir="$keep_all_dir"
               fi
               if [[ x"$(readlink -f "$keep_cp_dir")" != \
                     x"$(readlink -f "$keep_all_dir")" &&
                     x"$error_msg" == xundefined ]]; 
               then
                 info "mismatching directories for --(k|K)eep"
                 info "  will keep all files in:"
                 info "  $(readlink -f $keep_all_dir)"
                 keep_cp_dir=$keep_all_dir
               fi;;
    --log=*)   log_dir=`echo "$1"|sed -e 's,^--log=,,'`;;
    --version) echo "qbfcert-$VERSION"; clean_exit 0;;
    -*|--*)    die "invalid command line option: '$1'";;
    *)         break;;
  esac
  shift
done

if [[ x"$error_msg" != xundefined ]]; then
  die "$error_msg"
fi

## input formula

formula="$*"

if [[ x"$formula" == xundefined ]]; then
  die "input formula missing"
elif [[ ! -f "$formula" ]]; then
  die "invalid input formula: '$formula'"
fi

formula_name="qbfcert_run$$_$(basename "$formula")"
formula_name="${formula_name%.*}"

## logging

if [[ x"$log_dir" != xundefined ]]; then
  if [[ -d $log_dir ]]; then
    log_file="$log_dir/qbfcert_run$$.log"
  elif [[ -f $log_dir ]]; then
    die "invalid log dir: '$log_dir' (is a file)"
  else
    mkdir -p "$log_dir" 2> /dev/null
    if [[ $? != 0 ]]; then
      die "could not create log directory: '$log_dir'"
    fi
  fi
  QRPCHECK_PARAMS+=" -s "
  QRPCERT_PARAMS+=" -s "
fi

## check options

if [[ x"$qrp_format" == xundefined ]]; then
  qrp_format="bqrp"
fi
QBFSOLVER_PARAMS+=" --trace=$qrp_format "

if [[ x$aiger_format == xaiger ]]; then
  QRPCERT_PARAMS+=" --aiger-ascii "
fi


## check tool binaries

check_binary $QBFSOLVER
check_binary $QRPCHECK
check_binary $QRPCERT
check_binary $CERTCHECK
check_binary $SATSOLVER


## check tmp directory

if [[ ! -d "$TMPDIR" ]]; then
  if [[ -f "$TMPDIR" ]]; then
    die "invalid tmp dir: '$TMPDIR' (is a file)"
  fi
  mkdir -p "$TMPDIR" 2> /dev/null
  if [[ $? != 0 ]]; then
    die "could not create tmp directory: '$TMPDIR'"
  fi
fi

info "using tmp directory:"
info "  $(readlink -f "$TMPDIR")"

## run

log "cleanup"
cleanup
log "" 

log "run on: $(readlink -f "$formula")"
log "with options:"
if $check_icubes ]]; then
  log "  -i:     enabled"
else
  log "  -i:     disabled"
fi
log "  -f:     $qrp_format"
log "  -F:     $aiger_format"
if $keep_cp; then
  log "    --keep: $(readlink -f "$keep_cp_dir")"
fi
if $keep_all; then
  log "    --Keep: $(readlink -f "$keep_all_dir")"
fi
log "" 

# QBF solver
log "running QBF solver..."
log ""

$QBFSOLVER $QBFSOLVER_PARAMS < "$formula" > "$trace" 2>> "$log_file"
solver_result=$?

if [[ $check_icubes && $solver_result == $QBFSOLVER_SAT ]]; then
  QRPCHECK_PARAMS+=" -f $formula "
elif [[ $solver_result != $QBFSOLVER_SAT && 
        $solver_result != $QBFSOLVER_UNSAT ]]; then
  die "QBF solver ERROR"
fi
log ""
log "QBF solver OK"
log ""

if [[ x"$keep_all_dir" != xundefined ]]; then
  cp "$trace" "$keep_all_dir/$formula_name"_trace.qrp 2> /dev/null
  if [[ $? != 0 ]]; then
    die "failed to write trace to directory '$keep_all_dir'"
  fi
fi

# QRPcheck: extract and check resolution proof
log "running QRPcheck..."
log ""

if [[ $solver_result == $QBFSOLVER_UNSAT ]]; then
  $QRPCHECK $QRPCHECK_PARAMS "$trace" > "$proof" 2>> "$log_file"
  qrpcheck_result=$?
else
  $QRPCHECK $QRPCHECK_PARAMS "$trace" > "$proof" 2>> "$log_file"
  qrpcheck_result=$?
fi

if [[ $qrpcheck_result != 0 ]]; then
  die "QRPcheck ERROR"
fi 
log "" 
log "QRPcheck OK"
log ""

if [[ x"$keep_cp_dir" != xundefined ]]; then
  cp "$proof" "$keep_cp_dir/$formula_name"_proof.qrp 2> /dev/null
  if [[ $? != 0 ]]; then
    die "failed to write proof to directory '$keep_cp_dir'"
  fi
fi
 
# QRPcert: extract certificate
log "running QRPcert..."
log ""

$QRPCERT $QRPCERT_PARAMS "$proof" > "$certificate" 2>> "$log_file"
qrpcert_result=$?

if [[ $qrpcert_result != 0 ]]; then
  die "QRPcert ERROR"
fi
log ""
log "QRPcert OK"
log ""

if [[ x"$keep_cp_dir" != xundefined ]]; then
  cp "$certificate" "$keep_cp_dir/$formula_name"_certificate.aiger 2> /dev/null
  if [[ $? != 0 ]]; then
    die "failed to write certificate to directory '$keep_cp_dir'"
  fi
fi

# CertCheck: skolemize/herbrandize input formula
log "running Certcheck..."
log ""
$CERTCHECK $CERTCHECK_PARAMS "$formula" "$certificate" > "$merged_cnf"
certcheck_result=$?

if [[ $certcheck_result != 0 ]]; then
  die "certcheck ERROR"
fi
log "CertCheck OK"
log ""

if [[ x"$keep_all_dir" != xundefined ]]; then
  cp "$merged_cnf" "$keep_all_dir/$formula_name"_merged.cnf 2> /dev/null
  if [[ $? != 0 ]]; then
    die "failed to write merged cnf to directory '$keep_all_dir'"
  fi
fi

# SAT solver: validate certificate
log "running SAT solver..."
log ""
$SATSOLVER $SATSOLVER_PARAMS "$merged_cnf" >> "$log_file" 2>&1
sat_solver_result=$?

if [[ $sat_solver_result == $SATSOLVER_SAT ]]; then
  die "INVALID"
elif [[ $sat_solver_result != $SATSOLVER_UNSAT ]]; then
  die "SAT solver ERROR"
fi
log ""
log "SAT solver OK"

info ""
info "VALID"
clean_exit 0
