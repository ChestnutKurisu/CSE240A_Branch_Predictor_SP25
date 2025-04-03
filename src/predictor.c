//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors.             //
//========================================================//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "predictor.h"

const char *studentName = "Param Somane";
const char *studentID   = "A69033076";
const char *email       = "psomane@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

// ---- Gshare data structures ----
static uint32_t ghistory_g;
static uint8_t *bht_gshare = NULL; // 2-bit counters

static inline void shift_prediction(uint8_t* c, uint8_t outcome) {
    // 2-bit states: SN=0, WN=1, WT=2, ST=3
    if (outcome == TAKEN) {
        if (*c < ST) { (*c)++; }
    } else {
        if (*c > SN) { (*c)--; }
    }
}

// ---- Tournament data structures ----
static uint8_t  *localBHT   = NULL;
static uint16_t *localPHT   = NULL;
static uint8_t  *globalBHT  = NULL;
static uint8_t  *choicePT   = NULL;
static uint32_t globalhistory_t;
static uint8_t  localOutcome;
static uint8_t  globalOutcome;

// ---- Custom (TAGE-like) data structures ----
#define BIMODAL_SIZE 4099
#define LEN_BIMODAL  2

#define NUM_BANKS 7
#define LEN_GLOBAL 9
#define LEN_TAG    10
#define LEN_COUNTS 3
#define MAX_HISTORY_LEN 131

static const uint8_t GEOMETRICS[NUM_BANKS] = {130, 76, 44, 26, 15, 9, 5};

static inline void updateSaturate(int8_t *saturate, int taken, int nbits) {
    if (taken) {
        if ((*saturate) < ((1 << (nbits - 1)) - 1)) {
            (*saturate)++;
        }
    } else {
        if ((*saturate) > -(1 << (nbits - 1))) {
            (*saturate)--;
        }
    }
}

static inline void updateSaturateMinMax(int8_t *saturate, int taken, int min, int max) {
    if (taken) {
        if ((*saturate) < max) {
            (*saturate)++;
        }
    } else {
        if ((*saturate) > min) {
            (*saturate)--;
        }
    }
}

static int8_t t_bimodalPredictor[BIMODAL_SIZE];

typedef struct {
    int8_t   saturateCounter; // 3-bit
    uint16_t tag;             // up to 10 bits
    int8_t   usefulness;      // typically small
} BankEntry;

typedef struct {
    int8_t   geometryLength;
    int8_t   targetLength;
    uint32_t compressed;
} CompressedHistory;

typedef struct {
    int geometry;
    BankEntry entry[1 << LEN_GLOBAL];
    CompressedHistory indexCompressed;
    CompressedHistory tagCompressed[2];
} Bank;

static Bank    tageBank[NUM_BANKS];
static uint8_t t_globalHistory[MAX_HISTORY_LEN];
static uint32_t t_pathHistory;
static uint8_t  primaryBank, alternateBank;
static uint8_t  primaryPrediction, alternatePrediction;
static uint8_t  lastPrediction;
static int8_t   useAlternate;
static uint32_t bankGlobalIndex[NUM_BANKS];
static int      tagResult[NUM_BANKS];

//------------------------------------//
//    Predictor Function Declarations //
//------------------------------------//

// Gshare
static inline uint8_t gshare_predict(uint32_t pc);
static inline void     train_gshare(uint32_t pc, uint8_t outcome);

// Tournament
static void     tournament_init();
static inline uint8_t get_local_prediction(uint32_t pc);
static inline uint8_t get_global_prediction(uint32_t pc);
static inline uint8_t get_tournament_prediction(uint32_t pc);
static inline void     tournament_update(uint32_t pc, uint8_t outcome);

// Custom TAGE
static void     tage_init();
static inline uint8_t tage_predict(uint32_t pc);
static inline void     tage_train(uint32_t pc, uint8_t outcome);

//------------------------------------//
//        Memory Usage (Optional)     //
//------------------------------------//

static inline void print_memory_usage(void)
{
    unsigned int bits_used = 0;
    switch (bpType) {
        case STATIC: {
            bits_used = 0;
            break;
        }
        case GSHARE: {
            bits_used = (1 << ghistoryBits) * 2; // 2 bits each
            break;
        }
        case TOURNAMENT: {
            // globalBHT => 2 bits each => (1<<ghistoryBits)*2
            // choicePT  => same => (1<<ghistoryBits)*2
            // localBHT  => (1<<lhistoryBits)*2
            // localPHT  => (1<<pcIndexBits)*lhistoryBits bits
            bits_used += (1 << ghistoryBits) * 2;
            bits_used += (1 << ghistoryBits) * 2;
            bits_used += (1 << lhistoryBits) * 2;
            bits_used += (1 << pcIndexBits) * lhistoryBits;
            break;
        }
        case CUSTOM: {
            // Bimodal
            bits_used += BIMODAL_SIZE * LEN_BIMODAL;
            // TAGE banks
            for (int i = 0; i < NUM_BANKS; i++) {
                bits_used += (1 << LEN_GLOBAL) * 15; // saturateCounter+tag+usefulness ~15 bits
                bits_used += 144; // overhead for compression struct
            }
            // Histories
            bits_used += MAX_HISTORY_LEN;
            bits_used += 16; // pathHistory
            bits_used += 4;  // useAlternate
            bits_used += NUM_BANKS * LEN_GLOBAL;
            bits_used += NUM_BANKS * LEN_TAG;
            break;
        }
        default: {
            bits_used = 0;
            break;
        }
    }
    fprintf(stdout, "Approx memory usage: %u bits (%.2f KB)\n",
            bits_used, (double)bits_used / 8192.0);
}

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

//--------------------------------------------------------
// init_predictor
//--------------------------------------------------------
void init_predictor() {
    switch (bpType) {
    case STATIC:
        // Nothing to do
        break;

    case GSHARE: {
        ghistory_g = 0;
        bht_gshare = (uint8_t *)malloc((1 << ghistoryBits) * sizeof(uint8_t));
        memset(bht_gshare, WN, (1 << ghistoryBits) * sizeof(uint8_t));
    } break;

    case TOURNAMENT:
        tournament_init();
        break;

    case CUSTOM:
        tage_init();
        break;

    default:
        // unknown type => do nothing
        break;
    }

    // Print usage info if desired
    print_memory_usage();
}

//--------------------------------------------------------
// make_prediction
//--------------------------------------------------------
uint8_t make_prediction(uint32_t pc) {
    switch (bpType) {
    case STATIC:
        return TAKEN;  // always predict taken

    case GSHARE:
        return gshare_predict(pc);

    case TOURNAMENT:
        return get_tournament_prediction(pc);

    case CUSTOM:
        return tage_predict(pc);

    default:
        // If there is not a compatable bpType then return NOTTAKEN
        return NOTTAKEN;
    }
}

//--------------------------------------------------------
// train_predictor
//--------------------------------------------------------
void train_predictor(uint32_t pc, uint8_t outcome) {
    switch (bpType) {
    case STATIC:
        // No training needed
        break;

    case GSHARE:
        train_gshare(pc, outcome);
        break;

    case TOURNAMENT:
        tournament_update(pc, outcome);
        break;

    case CUSTOM:
        tage_train(pc, outcome);
        break;

    default:
        // unknown => do nothing
        break;
    }
}

//--------------------------------------------------------
// GSHARE Implementation
//--------------------------------------------------------
static inline uint8_t gshare_predict(uint32_t pc) {
    uint32_t mask  = (1 << ghistoryBits) - 1;
    uint32_t index = ((pc & mask) ^ (ghistory_g & mask));
    uint8_t  state = bht_gshare[index];
    return (state == SN || state == WN) ? NOTTAKEN : TAKEN;
}

static inline void train_gshare(uint32_t pc, uint8_t outcome) {
    uint32_t mask  = (1 << ghistoryBits) - 1;
    uint32_t index = ((pc & mask) ^ (ghistory_g & mask));
    shift_prediction(&bht_gshare[index], outcome);
    ghistory_g = ((ghistory_g << 1) | outcome) & mask;
}

//--------------------------------------------------------
// TOURNAMENT Implementation
//--------------------------------------------------------
static void tournament_init() {
    localBHT = (uint8_t *)malloc((1 << lhistoryBits) * sizeof(uint8_t));
    memset(localBHT, WN, (1 << lhistoryBits) * sizeof(uint8_t));

    localPHT = (uint16_t *)malloc((1 << pcIndexBits) * sizeof(uint16_t));
    memset(localPHT, 0, (1 << pcIndexBits) * sizeof(uint16_t));

    choicePT = (uint8_t *)malloc((1 << ghistoryBits) * sizeof(uint8_t));
    memset(choicePT, WN, (1 << ghistoryBits) * sizeof(uint8_t)); // weakly select global

    globalBHT = (uint8_t *)malloc((1 << ghistoryBits) * sizeof(uint8_t));
    memset(globalBHT, WN, (1 << ghistoryBits) * sizeof(uint8_t));

    globalhistory_t = 0;
}

static inline uint8_t get_local_prediction(uint32_t pc) {
    uint32_t phtIndex = pc & ((1 << pcIndexBits) - 1);
    uint16_t lhist    = localPHT[phtIndex] & ((1 << lhistoryBits) - 1);
    uint8_t  lPred    = localBHT[lhist];
    localOutcome      = (lPred == SN || lPred == WN) ? NOTTAKEN : TAKEN;
    return localOutcome;
}

static inline uint8_t get_global_prediction(uint32_t pc) {
    (void)pc;
    uint32_t mask = (1 << ghistoryBits) - 1;
    uint32_t gIdx = globalhistory_t & mask;
    uint8_t  gPred = globalBHT[gIdx];
    globalOutcome  = (gPred == SN || gPred == WN) ? NOTTAKEN : TAKEN;
    return globalOutcome;
}

static inline uint8_t get_tournament_prediction(uint32_t pc) {
    uint32_t mask   = (1 << ghistoryBits) - 1;
    uint32_t cIndex = globalhistory_t & mask;
    uint8_t  choice = choicePT[cIndex];

    get_global_prediction(pc);
    get_local_prediction(pc);

    if (choice == SN || choice == WN) {
        return globalOutcome;
    } else {
        return localOutcome;
    }
}

static inline void tournament_update(uint32_t pc, uint8_t outcome) {
    uint32_t mask   = (1 << ghistoryBits) - 1;
    uint32_t cIndex = globalhistory_t & mask;

    // Update choice if localOutcome != globalOutcome
    if (localOutcome != globalOutcome) {
        if (localOutcome == outcome) {
            shift_prediction(&choicePT[cIndex], TAKEN);
        } else if (globalOutcome == outcome) {
            shift_prediction(&choicePT[cIndex], NOTTAKEN);
        }
    }

    // Update local predictor
    uint32_t phtIndex = pc & ((1 << pcIndexBits) - 1);
    uint16_t lhist    = localPHT[phtIndex] & ((1 << lhistoryBits) - 1);
    shift_prediction(&localBHT[lhist], outcome);

    lhist <<= 1;
    lhist  &= (1 << lhistoryBits) - 1;
    lhist  |= outcome;
    localPHT[phtIndex] = lhist;

    // Update global predictor
    uint32_t gIdx = globalhistory_t & mask;
    shift_prediction(&globalBHT[gIdx], outcome);

    // Update global history
    globalhistory_t = ((globalhistory_t << 1) | outcome) & mask;
}

//--------------------------------------------------------
// CUSTOM (TAGE) Implementation
//--------------------------------------------------------
static inline uint8_t t_getBimodalPrediction(uint32_t pc) {
    int idx = pc % BIMODAL_SIZE;
    int8_t val = t_bimodalPredictor[idx];
    // LEN_BIMODAL=2 => range is 0..3 => >=2 => TAKEN
    return (val >= (1 << (LEN_BIMODAL - 1))) ? TAKEN : NOTTAKEN;
}

static inline void t_updateCompressed(CompressedHistory* h, uint8_t* global) {
    uint32_t newC = (h->compressed << 1) + global[0];
    newC ^= global[h->geometryLength] << (h->geometryLength % h->targetLength);
    newC ^= (newC >> h->targetLength);
    newC &= (1 << h->targetLength) - 1;
    h->compressed = newC;
}

static inline uint16_t generateGlobalEntryTag(uint32_t pc, int bankIndex) {
    int bitsToUse = LEN_TAG - ((bankIndex + (NUM_BANKS & 1)) / 2);
    int mix = (int)(
        pc
        ^ (tageBank[bankIndex].tagCompressed[0].compressed)
        ^ ((tageBank[bankIndex].tagCompressed[1].compressed) << 1)
    );
    return (uint16_t)(mix & ((1 << bitsToUse) - 1));
}

static inline int F(int A, int size, int bank) {
    A = A & ((1 << size) - 1);
    int A1 = (A & ((1 << LEN_GLOBAL) - 1));
    int A2 = (A >> LEN_GLOBAL);
    A2     = ((A2 << bank) & ((1 << LEN_GLOBAL) - 1)) + (A2 >> (LEN_GLOBAL - bank));
    A      = A1 ^ A2;
    A      = ((A << bank) & ((1 << LEN_GLOBAL) - 1)) + (A >> (LEN_GLOBAL - bank));
    return A;
}

static inline uint32_t getGlobalIndex(uint32_t pc, int bankIdx) {
    int geometry = tageBank[bankIdx].geometry;
    int index;
    if (geometry >= 16) {
        index = pc
             ^ (pc >> (LEN_GLOBAL - (NUM_BANKS - bankIdx - 1)))
             ^ (tageBank[bankIdx].indexCompressed.compressed)
             ^ F(t_pathHistory, 16, bankIdx);
    } else {
        index = pc
             ^ (pc >> (LEN_GLOBAL - NUM_BANKS + bankIdx + 1))
             ^ (tageBank[bankIdx].indexCompressed.compressed)
             ^ F(t_pathHistory, geometry, bankIdx);
    }
    return (uint32_t)(index & ((1 << LEN_GLOBAL) - 1));
}

static void tage_init() {
    // Bimodal
    for (int i = 0; i < BIMODAL_SIZE; i++) {
        // let’s default to "weakly taken" => 1
        t_bimodalPredictor[i] = (1 << (LEN_BIMODAL - 1)) - 1;
    }
    // TAGE banks
    for (int i = 0; i < NUM_BANKS; i++) {
        tageBank[i].geometry = GEOMETRICS[i];

        tageBank[i].indexCompressed.compressed     = 0;
        tageBank[i].indexCompressed.geometryLength = GEOMETRICS[i];
        tageBank[i].indexCompressed.targetLength   = LEN_GLOBAL;

        tageBank[i].tagCompressed[0].compressed     = 0;
        tageBank[i].tagCompressed[0].geometryLength = GEOMETRICS[i];
        tageBank[i].tagCompressed[0].targetLength   =
            (int8_t)(LEN_TAG - ((i + (NUM_BANKS & 1)) / 2));

        tageBank[i].tagCompressed[1].compressed     = 0;
        tageBank[i].tagCompressed[1].geometryLength = GEOMETRICS[i];
        tageBank[i].tagCompressed[1].targetLength   =
            (int8_t)(LEN_TAG - ((i + (NUM_BANKS & 1)) / 2) - 1);

        for (int j = 0; j < (1 << LEN_GLOBAL); j++) {
            tageBank[i].entry[j].saturateCounter = 0; // borderline
            tageBank[i].entry[j].tag            = 0;
            tageBank[i].entry[j].usefulness     = 0;
        }
    }
    memset(t_globalHistory, 0, sizeof(t_globalHistory));
    t_pathHistory = 0;
    useAlternate  = 8;
    primaryBank = alternateBank = NUM_BANKS;
    primaryPrediction = alternatePrediction = lastPrediction = NOTTAKEN;

    srand((unsigned int)time(NULL));
}

static inline uint8_t tage_predict(uint32_t pc) {
    for (int i = 0; i < NUM_BANKS; i++) {
        tagResult[i]       = generateGlobalEntryTag(pc, i);
        bankGlobalIndex[i] = getGlobalIndex(pc, i);
    }

    primaryBank   = NUM_BANKS;
    alternateBank = NUM_BANKS;

    for (int i = 0; i < NUM_BANKS; i++) {
        if (tageBank[i].entry[ bankGlobalIndex[i] ].tag == tagResult[i]) {
            primaryBank = i;
            break;
        }
    }
    for (int i = primaryBank + 1; i < NUM_BANKS; i++) {
        if (tageBank[i].entry[ bankGlobalIndex[i] ].tag == tagResult[i]) {
            alternateBank = i;
            break;
        }
    }

    if (primaryBank < NUM_BANKS) {
        if (alternateBank < NUM_BANKS) {
            int8_t altCtr = tageBank[alternateBank].entry[ bankGlobalIndex[alternateBank] ].saturateCounter;
            alternatePrediction = (altCtr >= 0) ? TAKEN : NOTTAKEN;
        } else {
            alternatePrediction = t_getBimodalPrediction(pc);
        }
        int8_t pCtr = tageBank[primaryBank].entry[ bankGlobalIndex[primaryBank] ].saturateCounter;
        int8_t pU   = tageBank[primaryBank].entry[ bankGlobalIndex[primaryBank] ].usefulness;
        if ((pCtr != 0 && pCtr != -1) || (pU != 0) || (useAlternate < 8)) {
            lastPrediction = (pCtr >= 0) ? TAKEN : NOTTAKEN;
        } else {
            lastPrediction = alternatePrediction;
        }
    } else {
        // fallback to bimodal
        alternatePrediction = t_getBimodalPrediction(pc);
        lastPrediction      = alternatePrediction;
    }

    return lastPrediction;
}

static inline void tage_train(uint32_t pc, uint8_t outcome) {
    // 1. Determine whether we need to allocate a new entry in the TAGE tables (i.e., whether
    //    the prediction was mispredicted and we have reason to believe a new entry could
    //    improve future prediction accuracy).

    int needAllocate = 0;

    // primaryBank is the TAGE bank (or set of banks) that provided the final prediction.
    // If primaryBank is valid (less than NUM_BANKS), we have a prediction from TAGE.
    // Otherwise, the prediction came from the bimodal predictor.
    if (primaryBank < NUM_BANKS) {
        // pCtr is the saturating counter from the TAGE table entry in the primary bank.
        int8_t pCtr = tageBank[primaryBank].entry[ bankGlobalIndex[primaryBank] ].saturateCounter;

        // If the final prediction was wrong AND the sign of pCtr is opposite from the actual outcome,
        // we may want to allocate a new entry from the other TAGE banks (which didn't match).
        // This is a typical "allocate on misprediction" strategy for TAGE.
        if ((lastPrediction != outcome) && ((pCtr >= 0) != outcome)) {
            needAllocate = 1;
        }
    } else {
        // If the prediction was from the bimodal predictor, we consider allocating
        // if the prediction was simply incorrect.
        needAllocate = (lastPrediction != outcome);
    }

    // 2. If we need to allocate, determine whether it is possible or desirable to do so.
    //    TAGE banks use a "usefulness" counter to decide if an entry is worth replacing.

    if (needAllocate) {
        int8_t minUse = 127;

        // Search for the minimum "usefulness" across all banks that are more selective (i.e. < primaryBank).
        // The lower this value, the more likely it is that an entry can be replaced.
        for (int i = 0; i < primaryBank; i++) {
            int8_t u = tageBank[i].entry[ bankGlobalIndex[i] ].usefulness;
            if (u < minUse) {
                minUse = u;
            }
        }

        // If all banks have high usefulness, we decrement the usefulness counters (to give a chance
        // for entries to become less useful and thus replaceable in the future).
        if (minUse > 0) {
            for (int i = primaryBank - 1; i >= 0; i--) {
                tageBank[i].entry[ bankGlobalIndex[i] ].usefulness--;
            }
        } else {
            // Otherwise, at least one bank has minimal usefulness, and we can replace an entry in one of
            // those banks with a new tag/saturating counter.

            // Y is used here to randomly select which bank (among the possible ones) gets the new entry.
            // The expression "(1 << (primaryBank - 1)) - 1" essentially creates a bitmask.
            // For example, if primaryBank = 3, we get (1 << 2) - 1 = 3 (binary 11).
            int Y = rand() & ((1 << (primaryBank - 1)) - 1);
            int X = primaryBank - 1;

            // This loop steps backward through the banks (less selective to more selective) until it
            // finds one that is randomly selected to allocate.
            while ((Y & 1) != 0) {
                X--;
                Y >>= 1;
                if (X < 0) break;
            }

            // Once we choose bank X, look for an entry in that bank with usefulness == minUse.
            // We then re-initialize it with the new tag, a saturating counter biased by the outcome,
            // and reset usefulness.
            for (int i = X; i >= 0; i--) {
                if (tageBank[i].entry[ bankGlobalIndex[i] ].usefulness == minUse) {
                    // Re-initialize the entry with the new tag and starting counter value.
                    tageBank[i].entry[ bankGlobalIndex[i] ].tag = generateGlobalEntryTag(pc, i);
                    tageBank[i].entry[ bankGlobalIndex[i] ].saturateCounter =
                        (outcome == TAKEN) ? 0 : -1;
                    tageBank[i].entry[ bankGlobalIndex[i] ].usefulness = 0;
                    break;
                }
            }
        }
    }

    // 3. Update the saturating counter in the primary bank (if used), or the bimodal predictor (otherwise).
    //    Saturating counters move up/down based on whether the outcome was TAKEN or NOTTAKEN.
    if (primaryBank < NUM_BANKS) {
        updateSaturate(
            &tageBank[primaryBank].entry[ bankGlobalIndex[primaryBank] ].saturateCounter,
            outcome,
            LEN_COUNTS
        );
    } else {
        // If the prediction came from the bimodal predictor, update it instead.
        int idx = pc % BIMODAL_SIZE;
        updateSaturateMinMax(&t_bimodalPredictor[idx], outcome, 0, (1 << LEN_BIMODAL) - 1);
    }

    // 4. If the primary prediction was different from the alternate prediction,
    //    update the usefulness counter for the primary table’s entry. We reward or penalize
    //    the primary TAGE entry if it got the actual outcome right or wrong, respectively.
    if (lastPrediction != alternatePrediction) {
        if (primaryBank < NUM_BANKS) {
            updateSaturateMinMax(
                &tageBank[primaryBank].entry[ bankGlobalIndex[primaryBank] ].usefulness,
                (lastPrediction == outcome),
                0, 3
            );
        }
    }

    // 5. Update the global history with the new outcome. This shifts the array t_globalHistory
    //    and inserts the latest taken/not-taken result at the front (index 0).
    for (int i = MAX_HISTORY_LEN - 1; i > 0; i--) {
        t_globalHistory[i] = t_globalHistory[i - 1];
    }
    t_globalHistory[0] = outcome ? TAKEN : NOTTAKEN;

    // Update the path history (which often tracks the program-counter's least significant bits).
    t_pathHistory <<= 1;
    t_pathHistory += (pc & 1);
    t_pathHistory &= ((1 << 16) - 1); // keep only the lower 16 bits

    // 6. Update the compressed histories for each TAGE bank. Compressed history is used to index
    //    and tag TAGE tables with partial, hashed versions of the global history.
    for (int i = 0; i < NUM_BANKS; i++) {
        t_updateCompressed(&tageBank[i].indexCompressed, t_globalHistory);
        t_updateCompressed(&tageBank[i].tagCompressed[0], t_globalHistory);
        t_updateCompressed(&tageBank[i].tagCompressed[1], t_globalHistory);
    }
}
