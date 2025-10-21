#!/bin/bash

# ==========================================
# ChampSim Branch Predictor Automation Script
# (Prebuilt binaries version)
# ==========================================

set +e  # Don't exit on error

# ---- Color Codes ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ---- Configuration ----
WARMUP_INSTRUCTIONS=25000000
SIMULATION_INSTRUCTIONS=250000000
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BASE_RESULTS_DIR="results_${TIMESTAMP}"

# Predictors list
PREDICTORS=("btip" "fdip" "no")

# Trace selection mode: "auto", "specific", or "all"
TRACE_SELECTION_MODE="specific"

# Specific traces to use
SPECIFIC_TRACES=(
    "spec_x264_001.champsimtrace.xz"
    "spec_perlbench_001.champsimtrace.xz"
    
)

# ---- Display Header ----
echo -e "${BLUE}=========================================="
echo "ChampSim Branch Predictor Comparison"
echo "==========================================${NC}"
echo "Timestamp: $TIMESTAMP"
echo "Results Directory: $BASE_RESULTS_DIR"
echo ""

# ---- Validate Environment ----
if [ ! -f "config.sh" ]; then
    echo -e "${RED}Error: Must be run from ChampSim root directory${NC}"
    exit 1
fi

# ---- Create Results Directory ----
mkdir -p "$BASE_RESULTS_DIR"
echo -e "${GREEN}✓ Created results directory${NC}"

# ---- Locate Traces ----
POSSIBLE_DIRS=("../traces" "./traces" "../../traces")
TRACES_DIR=""
for dir in "${POSSIBLE_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        count=$(find "$dir" -name "*.champsimtrace.xz" -type f 2>/dev/null | wc -l)
        if [ $count -gt 0 ]; then
            TRACES_DIR="$dir"
            echo -e "${GREEN}Found traces directory: $TRACES_DIR${NC}"
            break
        fi
    fi
done

if [ -z "$TRACES_DIR" ]; then
    echo -e "${RED}Error: No traces directory found${NC}"
    exit 1
fi

ALL_TRACES=($(find "$TRACES_DIR" -name "*.champsimtrace.xz" -type f | sort))
if [ ${#ALL_TRACES[@]} -eq 0 ]; then
    echo -e "${RED}Error: No trace files found in $TRACES_DIR${NC}"
    exit 1
fi
echo -e "${GREEN}Found ${#ALL_TRACES[@]} total trace files${NC}"

# ---- Select Traces ----
TRACES=()
case "$TRACE_SELECTION_MODE" in
    "specific")
        echo -e "${CYAN}Using specific trace selection${NC}"
        for t in "${SPECIFIC_TRACES[@]}"; do
            path=$(find "$TRACES_DIR" -name "$t" -type f | head -1)
            if [ -n "$path" ]; then
                TRACES+=("$path")
                echo -e "  ${GREEN}✓ Found:${NC} $t"
            else
                echo -e "  ${YELLOW}⚠ Not found:${NC} $t"
            fi
        done
        if [ ${#TRACES[@]} -eq 0 ]; then
            echo -e "${YELLOW}No specific traces found, falling back to auto mode${NC}"
            TRACES=("${ALL_TRACES[@]:0:5}")
        fi
        ;;
    "auto")
        echo -e "${CYAN}Auto-selecting first 5 traces${NC}"
        TRACES=("${ALL_TRACES[@]:0:5}")
        ;;
    "all")
        echo -e "${CYAN}Using all available traces${NC}"
        TRACES=("${ALL_TRACES[@]}")
        ;;
    *)
        echo -e "${RED}Invalid TRACE_SELECTION_MODE: $TRACE_SELECTION_MODE${NC}"
        exit 1
        ;;
esac

echo -e "\n${GREEN}Selected ${#TRACES[@]} trace(s):${NC}"
for ((i=0; i<${#TRACES[@]}; i++)); do
    echo "  $((i+1)). $(basename ${TRACES[$i]})"
done

if [ ${#TRACES[@]} -eq 0 ]; then
    echo -e "${RED}Error: No valid traces selected${NC}"
    exit 1
fi

# ---- Confirm Execution ----
echo -e "\n${YELLOW}About to run ${#PREDICTORS[@]} predictors on ${#TRACES[@]} traces${NC}"
echo -e "${YELLOW}Estimated time: $((${#PREDICTORS[@]} * ${#TRACES[@]} * 20 / 60)) - $((${#PREDICTORS[@]} * ${#TRACES[@]} * 40 / 60)) hours${NC}"
read -p "Continue? (y/n) " -n 1 -r
echo
[[ ! $REPLY =~ ^[Yy]$ ]] && echo "Aborted" && exit 0

# ---- Map predictor to prebuilt binary ----
get_binary_path() {
    local predictor=$1
    case "$predictor" in
        btip) echo "bin/champsim_btip" ;;
        fdip) echo "bin/champsim_fdip" ;;
        no)   echo "bin/champsim_no" ;;
        *)    echo "" ;;
    esac
}

# ---- Simulation function ----
run_simulation() {
    local predictor=$1
    local trace=$2
    local trace_name=$(basename "$trace" .champsimtrace.xz)
    local predictor_dir="$BASE_RESULTS_DIR/$predictor"
    local output_file="$predictor_dir/${trace_name}.txt"

    mkdir -p "$predictor_dir"

    echo -e "\n${CYAN}Running $predictor on $trace_name...${NC}"

    local binary=$(get_binary_path "$predictor")
    if [ ! -f "$binary" ]; then
        echo -e "${RED}✗ Binary not found: $binary${NC}"
        return 1
    fi

    local start_time=$(date +%s)
    timeout 3600 "$binary" \
        --warmup-instructions $WARMUP_INSTRUCTIONS \
        --simulation-instructions $SIMULATION_INSTRUCTIONS \
        "$trace" > "$output_file" 2>&1
    local exit_code=$?
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    if [ $exit_code -ne 0 ]; then
        echo -e "${RED}✗ Simulation failed or timeout (exit code: $exit_code)${NC}"
        return 1
    fi

    # --- Metrics Extraction ---
    local ipc=$(grep -m1 "CPU 0 cumulative IPC:" "$output_file" | awk '{print $5}')
    local instructions=$(grep -m1 "CPU 0 cumulative IPC:" "$output_file" | awk '{print $4}')
    ipc=${ipc:-N/A}
    instructions=${instructions:-0}

    local branch_mpki=$(grep -m1 "Branch Prediction Accuracy:" "$output_file" | awk -F'MPKI:' '{print $2}' | awk '{print $1}')
    branch_mpki=${branch_mpki:-N/A}

    extract_mpki() {
        local cache_name=$1
        local file=$2
        local total_access=$(grep -m1 "cpu0->$cache_name TOTAL" "$file" | awk '{for(i=1;i<=NF;i++){if($i=="ACCESS:"){print $(i+1); break}}}')
        local total_miss=$(grep -m1 "cpu0->$cache_name TOTAL" "$file" | awk '{for(i=1;i<=NF;i++){if($i=="MISS:"){print $(i+1); break}}}')
        total_access=${total_access:-0}
        total_miss=${total_miss:-0}
        local mpki="N/A"
        if [[ "$instructions" =~ ^[0-9]+$ ]] && [ "$instructions" -gt 0 ]; then
            mpki=$(echo "scale=4; ($total_miss * 1000)/$instructions" | bc)
        fi
        echo "$mpki,$total_access,$total_miss"
    }

    read l1i_mpki l1i_access l1i_miss <<< $(extract_mpki "cpu0_L1I" "$output_file" | tr ',' ' ')
    read l1d_mpki l1d_access l1d_miss <<< $(extract_mpki "cpu0_L1D" "$output_file" | tr ',' ' ')
    read l2c_mpki l2c_access l2c_miss <<< $(extract_mpki "cpu0_L2C" "$output_file" | tr ',' ' ')
    read llc_mpki llc_access llc_miss <<< $(extract_mpki "LLC" "$output_file" | tr ',' ' ')

    local rob_occ=$(grep -m1 "Average ROB Occupancy at Mispredict:" "$output_file" | awk '{print $6}')
    rob_occ=${rob_occ:-N/A}

    # Print summary
    echo "  IPC: ${ipc}"
    echo "  Branch MPKI: ${branch_mpki}"
    echo "  ROB Avg Occupancy: ${rob_occ}"
    echo "  L1I MPKI: ${l1i_mpki} (Accesses: ${l1i_access}, Misses: ${l1i_miss})"
    echo "  L1D MPKI: ${l1d_mpki} (Accesses: ${l1d_access}, Misses: ${l1d_miss})"
    echo "  L2C MPKI: ${l2c_mpki} (Accesses: ${l2c_access}, Misses: ${l2c_miss})"
    echo "  LLC MPKI: ${llc_mpki} (Accesses: ${llc_access}, Misses: ${llc_miss})"

    # Save to CSV
    echo "$predictor,$trace_name,$ipc,$branch_mpki,$rob_occ,$l1i_mpki,$l1d_mpki,$l2c_mpki,$llc_mpki,$duration" >> "$BASE_RESULTS_DIR/summary.csv"

    return 0
}

# ---- Summary File Header ----
echo "Predictor,Trace,IPC,Branch_MPKI,ROB_Occupancy,L1I_MPKI,L1D_MPKI,L2C_MPKI,LLC_MPKI,Duration_sec" > "$BASE_RESULTS_DIR/summary.csv"

OVERALL_START=$(date +%s)

# ---- Main Loop ----
for predictor in "${PREDICTORS[@]}"; do
    echo -e "\n${BLUE}========================================"
    echo "Processing Predictor: $predictor"
    echo -e "========================================${NC}"

    success=0
    fail=0
    for trace in "${TRACES[@]}"; do
        if run_simulation "$predictor" "$trace"; then
            ((success++))
        else
            ((fail++))
        fi
    done
    echo -e "${GREEN}✔ $success completed, ${RED}✗ $fail failed${NC} for $predictor"
done

OVERALL_END=$(date +%s)
TOTAL_DURATION=$((OVERALL_END - OVERALL_START))

echo -e "\n${CYAN}All simulations completed in $TOTAL_DURATION seconds${NC}"
echo -e "${GREEN}Summary CSV: $BASE_RESULTS_DIR/summary.csv${NC}"
