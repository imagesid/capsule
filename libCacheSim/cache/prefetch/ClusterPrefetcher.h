//  Capsule.c
//  libCacheSim
//
//  Created by imagesid on 25/02/03.
//  Copyright © 2025 imagesid. All rights reserved.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include <glib.h>
#include "glibconfig.h"
#include "g64.h"

#include "../../include/libCacheSim/prefetchAlgo.h"
#include "../../include/libCacheSim/prefetchAlgo/Mithril.h"
#include "../../include/libCacheSim/prefetchAlgo/PG.h"
#include "../../include/libCacheSim/prefetchAlgo/OBL.h"
#include "../../include/libCacheSim/prefetchAlgo/CAPSULE.h"

#include "../../../ioblazer/ioblazer.h" 
#include "dbscan/dbscan.h"
#include "kmeans/kmeans.h"
#include "meanshift/meanshift.h"
#include "kmedoids/kmedoids.h"
#include "../../../libCacheSim/bin/cachesim/debug.h"

// Define the enum
enum {
    Mithril,
    PG,
    OBL,
    CAPSULE
};


/**
    stats
*/
typedef struct {
    uint8_t used;   // 1 = prefetched and later accessed
    uint8_t tries;  // how many times it was prefetched but not used
} PrefStat;


static size_t PREFSTAT_TABLE_SIZE = 0;

static PrefStat *global_prefstats = NULL;

static PrefStat *get_prefstat(uint64_t lba) {
    return &global_prefstats[lba % PREFSTAT_TABLE_SIZE];
}

typedef struct {
    uint64_t total_requests;
    uint64_t total_prefetches;
    uint32_t window_size;    // e.g., 100 or 1000
    uint32_t prefetch_limit; // e.g., 30 prefetches per window
} PrefetchLimiter;

static PrefetchLimiter limiter = {0, 0, 100, 100}; 

/**end addition*/

// ----- Configuration Constants -----
#define INITIAL_CAPACITY 4
#define NUM_EPSILONS 11
#define HASH_SIZE 4096 
#define DIM_INDEX_SIZE 8192     
#define NUM_KEYS 10
#define MAX_CLUSTER_CAP 100000 

static bool enable_prefetching = false;
static uint32_t last_bucket =0;
static uint32_t total_yang_di_prefetch=0;
static uint32_t prefetch_cumul=0;
static bool sekali = true;
static bool sparse = true;
static bool has_spatial = true;
static bool LFU_EVICTION = true;
static bool billion_type= false;
static bool check_type= false;

// ----- Global Epsilon Array -----
static uint32_t epsilons[NUM_EPSILONS] = {512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288};
static bool useful_epsilon[NUM_EPSILONS] = { false };

/*
  ----- Composite Key Structure -----
  This structure represents a cluster identifier
  under a single epsilon value.
  Total size = 8 bytes (aligned).
*/


typedef struct {
    uint8_t eps_idx;         // Index into epsilons array
    uint32_t bucket_id;      // Bucket ID under that epsilon
} CompositeClusterId;

/*
  ===== Metadata Overhead Simulation =====

  Let's simulate the dynamic metadata overhead (i.e., memory used per cluster,
  including both static structure and dynamic allocations).

  For a new cluster:
    - The Cluster structure itself uses 104 bytes after reordering and alignment optimization.
    

  Additionally, each cluster now tracks its own current metadata usage (`md_usage`).
  This field ensures that when we evict a cluster, we always know its exact memory footprint
  without relying on global audits or recomputation.
*/

typedef struct Cluster {
    CompositeClusterId id;         // 8 bytes (alligned)
    uint64_t base_lba;             // 8 bytes

    uint32_t *neg_deltas;          // 8 bytes
    uint32_t *pos_deltas;          // 8 bytes
    bool *neg_freq;                // 8 bytes
    bool *pos_freq;                // 8 bytes

    struct Cluster *next;          // 8 bytes (hash chain linkage)
    struct Cluster *f_prev;        // 8 bytes (LFU doubly linked list)
    struct Cluster *f_next;        // 8 bytes

    uint32_t last_prefetched;      // 4 bytes
    uint32_t last_prefetched_ts;   // 4 bytes

    uint16_t neg_count;            // 2 bytes
    uint16_t pos_count;            // 2 bytes
    uint16_t neg_capacity;         // 2 bytes
    uint16_t pos_capacity;         // 2 bytes
    uint16_t freq;                 // 2 bytes

    bool is_valid;                 // 1 byte
    uint8_t _pad1;                 // 1 byte padding (for 8-byte alignment)

    size_t md_usage;               // 8 bytes — dynamically tracked metadata usage (Cluster + arrays)

   // Total fixed-size Cluster metadata = 104 bytes (aligned, 64-bit)

} Cluster;

// ----- Primary Hash Table -----
static Cluster *clusterHashTable[HASH_SIZE] = { 0 };

// Knuth's Constant
static unsigned int hashCompositeClusterId(const CompositeClusterId *id) {
    return (id->eps_idx * 2654435761U ^ id->bucket_id) % HASH_SIZE;
}

static bool compareCompositeClusterId(const CompositeClusterId *a, const CompositeClusterId *b) {
    return a->eps_idx == b->eps_idx && a->bucket_id == b->bucket_id;
}

static void computeCompositeIds(uint64_t data, CompositeClusterId *out, int num_eps) {
    for (int i = 0; i < num_eps; i++) {
        out[i].eps_idx = i;
        out[i].bucket_id = data / epsilons[i];
    }
}

static inline size_t cluster_metadata_size(const Cluster *c) {
    if (!c) return 0;
    size_t size = sizeof(Cluster);
    if (c->neg_deltas && c->neg_capacity > 0)
        size += c->neg_capacity * (sizeof(uint32_t) + sizeof(bool));
    if (c->pos_deltas && c->pos_capacity > 0)
        size += c->pos_capacity * (sizeof(uint32_t) + sizeof(bool));
    return size;
}

// === Debug Metadata Usage Tracking ===
static inline void debug_md_usage(const char *action,
                                  const Cluster *cluster,
                                  size_t delta_bytes,
                                  void *params,
                                  int scheme) {
    // size_t total_meta = 0;

    
}

#define MAX_FREQ 65536  
static Cluster *lfu_buckets[MAX_FREQ] = {NULL};  


static Cluster *getOrCreateClusterSingleCall(const CompositeClusterId *id,
                                             void *params,
                                             int scheme,
                                             size_t *md_to_allocate) {
    unsigned int index = hashCompositeClusterId(id);
    Cluster *curr = clusterHashTable[index];

    // --- Scan for an existing valid cluster ---
    while (curr) {
        if (curr->is_valid &&
            curr->id.eps_idx == id->eps_idx &&
            curr->id.bucket_id == id->bucket_id) {
            *md_to_allocate = 0;  // No new metadata allocation needed
            return curr;
        }
        curr = curr->next;
    }

    // --- If not found, report how much metadata a new one needs ---
    *md_to_allocate = sizeof(Cluster)
                    + (4 * sizeof(uint32_t))  // neg_deltas
                    + (4 * sizeof(uint32_t))  // pos_deltas
                    + (4 * sizeof(bool))      // neg_freq
                    + (4 * sizeof(bool));     // pos_freq

    return NULL;  // Let caller handle creation
}

static inline void debug_metadata_change(const char *action,
                                         const Cluster *c,
                                         size_t change,
                                         void *params,
                                         int scheme)
{
    size_t total = 0;
    switch (scheme) {
        case Mithril:
            total = ((Mithril_params_t *)params)->cur_metadata_size;
            break;
        case PG:
            total = ((PG_params_t *)params)->cur_metadata_size;
            break;
        case OBL:
            total = ((OBL_params_t *)params)->cur_metadata_size;
            break;
        case CAPSULE:
            total = ((CAPSULE_params_t *)params)->cur_metadata_size;
            break;
    }

    printf("[META %s] cluster=%p base=%lu change=%zuB total=%zuB\n",
           action,
           (void*)c,
           c ? c->base_lba : 0UL,
           change,
           total);
}

static Cluster *find_cluster_by_base_and_eps(uint64_t base_lba, uint32_t eps_idx) {
    for (int i = 0; i < HASH_SIZE; i++) {
        Cluster *curr = clusterHashTable[i];
        while (curr) {
            if (curr->base_lba == base_lba && curr->id.eps_idx == eps_idx)
                return curr;
            curr = curr->next;
        }
    }
    return NULL;
}

static Cluster *createNewCluster(const CompositeClusterId *id, uint64_t base_lba, void *params, int scheme) {

    
    unsigned int index = hashCompositeClusterId(id);

    Cluster *newCluster = malloc(sizeof(Cluster));
    if (!newCluster) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    newCluster->id = *id;
    newCluster->base_lba = base_lba;

    newCluster->is_valid = true;
    newCluster->freq = 0;         
    

    // Initialize frequency list pointers
    newCluster->f_prev = NULL;
    newCluster->f_next = NULL;

    newCluster->neg_count = 0;
    newCluster->pos_count = 0;
    newCluster->neg_capacity = 4;
    newCluster->pos_capacity = 4;

    newCluster->neg_deltas = malloc(newCluster->neg_capacity * sizeof(uint32_t));
    newCluster->pos_deltas = malloc(newCluster->pos_capacity * sizeof(uint32_t));

    if (!newCluster->neg_deltas || !newCluster->pos_deltas) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // add freq
    newCluster->neg_freq = malloc(newCluster->neg_capacity * sizeof(bool));
    newCluster->pos_freq = malloc(newCluster->pos_capacity * sizeof(bool));

    if (!newCluster->neg_freq || !newCluster->pos_freq) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    newCluster->last_prefetched = 0;
    newCluster->last_prefetched_ts = 0;
    newCluster->next = clusterHashTable[index];

    // to make faster for finding it
    clusterHashTable[index] = newCluster;


    size_t md_init_size = cluster_metadata_size(newCluster);

    newCluster->md_usage = md_init_size;

    switch (scheme) {
        case Mithril:
            ((Mithril_params_t *)params)->cur_metadata_size += md_init_size;
            break;
        case PG:
            ((PG_params_t *)params)->cur_metadata_size += md_init_size;
            break;
        case OBL:
            ((OBL_params_t *)params)->cur_metadata_size += md_init_size;
            break;
        case CAPSULE:
            ((CAPSULE_params_t *)params)->cur_metadata_size += md_init_size;
            break;
    }

    debug_md_usage("INSERT", newCluster, md_init_size, params, scheme);


    return newCluster;
}

static void insert_into_lfu_list(Cluster *cluster) {
    uint16_t freq = cluster->freq;
    Cluster *head = lfu_buckets[freq];

    cluster->f_next = head;
    cluster->f_prev = NULL;

    if (head != NULL) {
        head->f_prev = cluster;
    }

    lfu_buckets[freq] = cluster;
    // printf("freq %d\n", freq);
}

static void update_lfu_position(Cluster *cluster) {
    uint16_t old_freq = cluster->freq;

    if (old_freq >= MAX_FREQ - 1) {
        
        return;
    }


    // -------- REMOVE FROM OLD LIST --------
    if (cluster->f_prev) {
        cluster->f_prev->f_next = cluster->f_next;
        // printf("  - Unlinked from previous cluster @%p\n", (void*)cluster->f_prev);
    } else {
        // It was head of its bucket
        lfu_buckets[old_freq] = cluster->f_next;
        // printf("  - Removed from head of freq bucket %u\n", old_freq);
    }

    if (cluster->f_next) {
        cluster->f_next->f_prev = cluster->f_prev;
        // printf("  - Unlinked from next cluster @%p\n", (void*)cluster->f_next);
    }

    // Clear old pointers
    cluster->f_prev = NULL;
    cluster->f_next = NULL;

    // -------- INCREASE FREQUENCY --------
    cluster->freq = old_freq + 1;

    // -------- INSERT INTO NEW BUCKET --------
    Cluster *new_head = lfu_buckets[cluster->freq];

    cluster->f_next = new_head;
    cluster->f_prev = NULL;

    if (new_head) {
        new_head->f_prev = cluster;
        // printf("  - Inserted before new head cluster @%p in freq %u\n", 
            //    (void*)new_head, cluster->freq);
    } else {
        // printf("  - Inserted as head in empty freq bucket %u\n", cluster->freq);
    }

    lfu_buckets[cluster->freq] = cluster;

    // -------- DEBUG DUMP --------
    // printf("[LFU] Bucket[%u] now starts at @%p\n", cluster->freq, (void*)lfu_buckets[cluster->freq]);
}

static Cluster *evict_LFU_cluster() {
    for (int f = 0; f < MAX_FREQ; f++) {
        Cluster *victim = lfu_buckets[f];

        while (victim != NULL) {
            // Only access f_next after confirming victim is valid
            Cluster *next = victim->f_next;

            // Safety check (optional debug)
            // assert((uintptr_t)victim > 4096);

            if (victim->is_valid) {
                // Detach from LFU list
                if (lfu_buckets[f] == victim)
                    lfu_buckets[f] = next;
                if (victim->f_next)
                    victim->f_next->f_prev = NULL;

                victim->f_prev = NULL;
                victim->f_next = NULL;

                return victim; 
            }

            victim = next;
        }
    }
    return NULL;
}

static void remove_from_cluster_hash_table(const CompositeClusterId *id) {
    unsigned int index = hashCompositeClusterId(id);
    Cluster *curr = clusterHashTable[index];
    Cluster *prev = NULL;

    while (curr) {
        if (curr->id.eps_idx == id->eps_idx && curr->id.bucket_id == id->bucket_id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                clusterHashTable[index] = curr->next;
            }
            // Prevent future access
            curr->next = NULL;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

static void print_lfu_buckets() {
    printf("=== LFU Buckets ===\n");
    for (int f = 0; f < MAX_FREQ; f++) {
        Cluster *curr = lfu_buckets[f];
        if (!curr) continue;

        printf("Freq %d:\n", f);
        while (curr) {
            printf("  -> ID: (eps_idx=%d, bucket_id=%u), base_lba=%lu, neg=%u, pos=%u\n",
                   curr->id.eps_idx, curr->id.bucket_id, curr->base_lba,
                   curr->neg_count, curr->pos_count);
            curr = curr->f_next;
        }
    }
    printf("===================\n");
}

static void remove_from_lfu_list(Cluster *cluster) {
    if (!cluster) return;
    // print_lfu_buckets();
    uint16_t freq = cluster->freq;

    if (lfu_buckets[freq] == cluster) {
        // Removing the head of the list
        lfu_buckets[freq] = cluster->f_next;
    }

    if (cluster->f_prev) {
        cluster->f_prev->f_next = cluster->f_next;
    }

    if (cluster->f_next) {
        cluster->f_next->f_prev = cluster->f_prev;
    }

    cluster->f_prev = NULL;
    cluster->f_next = NULL;
}



static void debug_lfu_lists(void) {
    printf("\n==== LFU LIST DUMP ====\n");
    int nonempty = 0;
    for (int f = 0; f < MAX_FREQ; f++) {
        Cluster *head = lfu_buckets[f];
        if (!head) continue;  // skip empty

        nonempty++;
        printf("[FREQ %d] ", f);

        int count = 0;
        for (Cluster *cur = head; cur != NULL; cur = cur->f_next) {
            printf("-> %p(base=%lu, valid=%d) ",
                   (void*)cur, cur->base_lba, cur->is_valid);
            if (++count > 50) {  // prevent infinite loop if corrupted
                printf("... (loop or too long)\n");
                break;
            }
        }
        printf("\n");
        break;
    }
    if (!nonempty)
        printf("[LFU] All buckets empty.\n");
    printf("=========================\n");
}

static void debug_all_clusters(void) {
    printf("\n===== DEBUG: ALL EXISTING CLUSTERS =====\n");
    bool any_found = false;
    size_t total_clusters = 0;
    size_t total_deltas   = 0;
    size_t total_meta     = 0;

    // Automatically detect hash table size
    size_t table_size = sizeof(clusterHashTable) / sizeof(clusterHashTable[0]);

    for (size_t index = 0; index < table_size; index++) {
        Cluster *curr = clusterHashTable[index];
        if (!curr) continue;  // skip empty bucket

        printf("[HASH %zu]\n", index);
        int bucket_count = 0;

        while (curr) {
            if (!curr->is_valid) {
                curr = curr->next;
                continue;
            }

            any_found = true;
            total_clusters++;
            bucket_count++;

            printf("  Cluster %p | base=%lu | eps_idx=%d | bucket_id=%u | freq=%u\n",
                   (void*)curr,
                   curr->base_lba,
                   curr->id.eps_idx,
                   curr->id.bucket_id,
                   curr->freq);

            printf("    neg_count=%u pos_count=%u neg_cap=%u pos_cap=%u\n",
                   curr->neg_count, curr->pos_count,
                   curr->neg_capacity, curr->pos_capacity);

            if (curr->neg_count > 0) {
                printf("    NEG deltas: ");
                for (int i = 0; i < curr->neg_count; i++) {
                    printf("%u ", curr->neg_deltas[i]);
                    total_deltas++;
                }
                printf("\n");
            }

            if (curr->pos_count > 0) {
                printf("    POS deltas: ");
                for (int i = 0; i < curr->pos_count; i++) {
                    printf("%u ", curr->pos_deltas[i]);
                    total_deltas++;
                }
                printf("\n");
            }

            size_t meta = cluster_metadata_size(curr);
            total_meta += meta;
            printf("    est_metadata=%zu bytes\n\n", meta);

            curr = curr->next;
        }

        if (bucket_count > 0)
            printf("[hash=%zu] total_clusters=%d in this bucket\n\n", index, bucket_count);
    }

    if (!any_found)
        printf("[INFO] No clusters currently exist.\n");

    printf("===== SUMMARY =====\n");
    printf("Total clusters: %zu\n", total_clusters);
    printf("Total deltas:   %zu\n", total_deltas);
    printf("Total metadata: %zu bytes\n", total_meta);
    printf("===================\n\n");
}


static void assignDataToCluster(uint64_t data, uint8_t freq, void *params, int scheme) {
    
    for (int eps_idx = 0; eps_idx < NUM_EPSILONS; eps_idx++) {
        // just save only into useful epsilon
        // assign only to useful epsilon
        // printf("Useful %d => %d\n",epsilons[eps_idx], useful_epsilon[eps_idx] );
        if (!useful_epsilon[eps_idx]) continue;
        
        CompositeClusterId id = {
            .eps_idx = eps_idx,
            .bucket_id = data / epsilons[eps_idx]
        };

        while (1) {
            size_t md_needed = 0;
            Cluster *cluster = getOrCreateClusterSingleCall(&id, params, scheme, &md_needed);
            // printf("cluster: %p\n", (void*)cluster);
            bool allow_insert = false;
            switch (scheme) {
                case Mithril:
                    allow_insert = (((Mithril_params_t *)params)->cur_metadata_size + md_needed <
                                     ((Mithril_params_t *)params)->max_metadata_size);
                    break;
                case PG:
                    allow_insert = (((PG_params_t *)params)->cur_metadata_size + md_needed <
                                     ((PG_params_t *)params)->max_metadata_size);
                    break;
                case OBL:
                    allow_insert = (((OBL_params_t *)params)->cur_metadata_size + md_needed <
                                     ((OBL_params_t *)params)->max_metadata_size);
                    break;
                case CAPSULE:
                    allow_insert = (((CAPSULE_params_t *)params)->cur_metadata_size + md_needed <
                                     ((CAPSULE_params_t *)params)->max_metadata_size);
                    break;
            }
            // printf("cur metadata %d , max metadata %d\n",((CAPSULE_params_t *)params)->cur_metadata_size + md_needed ,
                                    //  ((CAPSULE_params_t *)params)->max_metadata_size);
            if (!allow_insert) {
                Cluster *victim = NULL;

                // === Choose eviction policy === always lfu
                if (LFU_EVICTION) {
                    victim = evict_LFU_cluster();
                } else {
                    // victim = evict_LRU_cluster();
                }

                if (!victim) {
                    
                    debug_all_clusters(); // Helpful to verify hash table state
                    exit(-1);
                    // break;
                }

                
                size_t freed_size = victim->md_usage;

                
                switch (scheme) {
                    case Mithril:
                        ((Mithril_params_t *)params)->cur_metadata_size -= freed_size;
                        break;
                    case PG:
                        ((PG_params_t *)params)->cur_metadata_size -= freed_size;
                        break;
                    case OBL:
                        ((OBL_params_t *)params)->cur_metadata_size -= freed_size;
                        break;
                    case CAPSULE:
                        ((CAPSULE_params_t *)params)->cur_metadata_size -= freed_size;
                        break;
                }


                debug_md_usage("EVICT", victim, freed_size, params, scheme);

                // Phase 5: free safely
                if (LFU_EVICTION)
                    remove_from_lfu_list(victim);
                remove_from_cluster_hash_table(&victim->id);

                victim->is_valid = false;
                free(victim->neg_deltas);
                free(victim->pos_deltas);
                free(victim->neg_freq);
                free(victim->pos_freq);
                free(victim);
                victim = NULL;
                continue;


                
            } 

            

            if (!cluster) {
                cluster = createNewCluster(&id, data, params, scheme);
                if (!cluster) break;
                if (LFU_EVICTION) {
                    insert_into_lfu_list(cluster);
                }
                // insert_into_lfu_list(cluster);
            }

            

            // Add data to cluster deltas
            if (data < cluster->base_lba) {
                uint32_t delta = cluster->base_lba - data;

                // === Expand Negative Delta Buffer if Needed ===
                if (cluster->neg_count >= cluster->neg_capacity) {
                    // Prevent over-expansion beyond 1000 entries
                    if (cluster->neg_capacity >= MAX_CLUSTER_CAP) {
                        
                        break;
                       
                    }

                    // Compute precise byte delta BEFORE changing capacity
                    size_t old_size = cluster->neg_capacity * (sizeof(uint32_t) + sizeof(bool));
                    size_t new_cap  = cluster->neg_capacity ? cluster->neg_capacity * 2 : 4;
                    if (new_cap > MAX_CLUSTER_CAP) new_cap = MAX_CLUSTER_CAP;  // clamp to limit
                    size_t new_size = new_cap * (sizeof(uint32_t) + sizeof(bool));
                    size_t delta_bytes = new_size - old_size;

                    uint32_t *new_deltas = realloc(cluster->neg_deltas, new_cap * sizeof(uint32_t));
                    bool     *new_freqs  = realloc(cluster->neg_freq,   new_cap * sizeof(bool));

                    if (!new_deltas || !new_freqs) break;

                    cluster->neg_deltas   = new_deltas;
                    cluster->neg_freq     = new_freqs;
                    cluster->neg_capacity = new_cap;

                    cluster->md_usage += delta_bytes;

                    // Update metadata size bookkeeping exactly by delta_bytes
                    switch (scheme) {
                        case Mithril:
                            ((Mithril_params_t *)params)->cur_metadata_size += delta_bytes;
                            break;
                        case PG:
                            ((PG_params_t *)params)->cur_metadata_size += delta_bytes;
                            break;
                        case OBL:
                            ((OBL_params_t *)params)->cur_metadata_size += delta_bytes;
                            break;
                        case CAPSULE:
                            ((CAPSULE_params_t *)params)->cur_metadata_size += delta_bytes;
                            break;
                    }

                    debug_md_usage("EXPAND", cluster, delta_bytes, params, scheme);

                }

                // Insert new delta
                uint16_t idx = cluster->neg_count++;
                cluster->neg_deltas[idx] = delta;
                cluster->neg_freq[idx]   = false;
            }

            else if (data > cluster->base_lba) {
                uint32_t delta = data - cluster->base_lba;

                // === Expand Positive Delta Buffer if Needed ===
                if (cluster->pos_count >= cluster->pos_capacity) {
                    if (cluster->pos_capacity >= MAX_CLUSTER_CAP) {
                        
                        break;
                    }

                    size_t old_size = cluster->pos_capacity * (sizeof(uint32_t) + sizeof(bool));
                    size_t new_cap  = cluster->pos_capacity ? cluster->pos_capacity * 2 : 4;
                    if (new_cap > MAX_CLUSTER_CAP) new_cap = MAX_CLUSTER_CAP;
                    size_t new_size = new_cap * (sizeof(uint32_t) + sizeof(bool));
                    size_t delta_bytes = new_size - old_size;

                    uint32_t *new_deltas = realloc(cluster->pos_deltas, new_cap * sizeof(uint32_t));
                    bool     *new_freqs  = realloc(cluster->pos_freq,   new_cap * sizeof(bool));

                    if (!new_deltas || !new_freqs) break;

                    cluster->pos_deltas   = new_deltas;
                    cluster->pos_freq     = new_freqs;
                    cluster->pos_capacity = new_cap;

                    cluster->md_usage += delta_bytes;

                    switch (scheme) {
                        case Mithril:
                            ((Mithril_params_t *)params)->cur_metadata_size += delta_bytes;
                            break;
                        case PG:
                            ((PG_params_t *)params)->cur_metadata_size += delta_bytes;
                            break;
                        case OBL:
                            ((OBL_params_t *)params)->cur_metadata_size += delta_bytes;
                            break;
                        case CAPSULE:
                            ((CAPSULE_params_t *)params)->cur_metadata_size += delta_bytes;
                            break;
                    }
                    debug_md_usage("EXPAND", cluster, delta_bytes, params, scheme);

                }

                // Insert new delta
                uint16_t idx = cluster->pos_count++;
                cluster->pos_deltas[idx] = delta;
                cluster->pos_freq[idx]   = false;
            }



            

            break;  // Move to next epsilon
        } // end while
    } // end for each epsilon
}

/**
 * Clustering Definitions
*/

#define ARRAY_SIZE 20000
#define SUBARRAY_SIZE 10000
#define SEJUTA 1000000
#define MAX_NEIGHBORS 50
#define CLUSTER_MD_SIZE 13


static int epsilon = 0; 
static int dim = 0; 
static bool enable = false; 
static uint8_t md_init_size = 39;
static uint8_t md_append_item_size = 10;
static uint8_t md_append_neighbors_size = 5;
static uint8_t min_neigh = 2;
static uint8_t chances = 1;
static uint32_t next_checker = 0;
static uint64_t next_checker_id = 0;
static uint8_t total_no_io = 0;

#define MAX_OBJID 1000
static uint64_t objids[MAX_OBJID];

static uint64_t hid_1000 = 0 ;
static uint64_t hid_10000 = 0 ;
static uint64_t hid_100000 = 0 ;
static uint64_t hid_1000000 = 0 ;

static uint64_t t_1000[MAX_OBJID];
static uint64_t t_10000[MAX_OBJID];
static uint64_t t_100000[MAX_OBJID];
static uint64_t t_1000000[MAX_OBJID];
static int num_objid = 0;
static bool postAssignment=false;

static GHashTable *checkpoint = NULL;
//end pre assign

//conventional
static int numPoints;
static uint64_t *myobj;
static int *labels;
static int interval = 100000;

#define NUM_POINTS 50000
#define MBS (1024ULL * 1024ULL)
static uint64_t base_cache = 256 * MBS;   // decimal megabytes (10^6 bytes)
static uint64_t base_cap   = 100;
static uint64_t cap_max    = 400;
static uint64_t current_cap = 100;
Point *points;

static void init(uint64_t cache_size){
    if (checkpoint == NULL) {
        checkpoint = g_hash_table_new(g_direct_hash, g_direct_equal);
    }

    size_t target_entries = cache_size / 64;
    if (target_entries < 1e6) target_entries = 1e6;
    if (target_entries > 4e6) target_entries = 4e6;

    // Meta data is updated in Capsule.c
    PREFSTAT_TABLE_SIZE = target_entries;
    global_prefstats = calloc(PREFSTAT_TABLE_SIZE, sizeof(PrefStat));
    if (!global_prefstats) {
        fprintf(stderr, "[CAPSULE] Failed to allocate PrefStat table (%zu entries)\n",
                PREFSTAT_TABLE_SIZE);
        exit(1);
    }

    if (cache_size >= 1000 * MBS)    
        current_cap = base_cap * 2;  

    if (cache_size >= 4000 * MBS)    
        current_cap = base_cap * 4;  

    // Safety cap
    if (current_cap > cap_max)
        current_cap = cap_max; 

}
// grouping function
static uint32_t group_func(uint32_t value){
  return (value / epsilon) % ARRAY_SIZE; 
}

// Function to get cluster by id
static Cluster* getClusterByHashId(uint32_t id) {
    if (id >= ARRAY_SIZE) {
        return NULL; // Out of bounds
    }
}

// Function to get cluster by id
static Cluster* getClusterById(uint32_t idx) {
    uint32_t id = group_func(idx);
    if (id >= ARRAY_SIZE) {
        return NULL; // Out of bounds
    }
    
}

static void setLeftNeighbor(cache_t *cache, uint32_t id, uint32_t real_id, int scheme){
    
  if(id <= 0){
      return;
  }
}

static void setRightNeighbor(cache_t *cache, uint32_t id, uint32_t real_id, int scheme){
  if(id >= ARRAY_SIZE){
      return;
  }
    
  
}

// Function to insert or append data into clusters
static void insertOrAppendToCluster( cache_t *cache, const request_t *req, int scheme) {
    if(!has_spatial){
        return;
    }
    void *params = NULL;
        
    // Determine the type of parameters based on the material
    if (scheme == Mithril) {
        params = (Mithril_params_t *)(cache->prefetcher->params);
    } else if (scheme == PG) {
        params = (PG_params_t *)(cache->prefetcher->params);
    } else if (scheme == OBL) {
        params = (OBL_params_t *)(cache->prefetcher->params);
    } else if (scheme == CAPSULE) {
        params = (CAPSULE_params_t *)(cache->prefetcher->params);
    } 

    
    assignDataToCluster(req->obj_id, 1, params, scheme);
}

// Check if queried_obj exists in ANY cluster bucket
static bool obj_in_clusters(uint64_t queried_obj) {
    for (int dim = 0; dim < NUM_EPSILONS; dim++) {
        uint32_t base_bucket = queried_obj / epsilons[dim];

        // check current, left, right
        int offsets[3] = {0, -1, 1};
        for (int o = 0; o < 3; o++) {
            int64_t neighbor_bucket = (int64_t)base_bucket + offsets[o];
            if (neighbor_bucket < 0) continue;

            CompositeClusterId id = {
                .eps_idx   = dim,
                .bucket_id = (uint32_t)neighbor_bucket
            };

            uint32_t hash_idx = hashCompositeClusterId(&id);
            Cluster *curr = clusterHashTable[hash_idx];

            while (curr) {
                if (!curr->is_valid) {
                    curr = curr->next;
                    continue;
                }

                if (curr->id.eps_idx != id.eps_idx ||
                    curr->id.bucket_id != id.bucket_id) {
                    curr = curr->next;
                    continue;
                }

                uint64_t base = curr->base_lba;

                if (base == queried_obj) return true;

                // check negative deltas
                for (int i = 0; i < curr->neg_count; i++) {
                    uint64_t candidate = base - curr->neg_deltas[i];
                    if (candidate == queried_obj) return true;
                }

                // check positive deltas
                for (int i = 0; i < curr->pos_count; i++) {
                    uint64_t candidate = base + curr->pos_deltas[i];
                    if (candidate == queried_obj) return true;
                }

                curr = curr->next;
            }
        }
    }
    return false; // not found anywhere
}


static void prefetchCluster(cache_t *cache, const request_t *req, int scheme, request_t *new_req) {
    if(!has_spatial){
        return;
    }

    void *params = cache->prefetcher->params;
    uint32_t ts = 0;
    GHashTable *size_map = NULL;
    bool record_stats = false;

    switch (scheme) {
        case Mithril:
            ts = ((Mithril_params_t *)params)->ts;
            size_map = ((Mithril_params_t *)params)->cache_size_map;
            record_stats = ((Mithril_params_t *)params)->output_statistics;
            break;
        case PG:
            ts = ((PG_params_t *)params)->ts;
            size_map = ((PG_params_t *)params)->cache_size_map;
            break;
        case OBL:
            ts = ((OBL_params_t *)params)->ts;
            size_map = ((OBL_params_t *)params)->cache_size_map;
            break;
        case CAPSULE:
            ts = ((CAPSULE_params_t *)params)->ts;
            size_map = ((CAPSULE_params_t *)params)->cache_size_map;
            break;
        default:
            return;
    }
    


    int max_prefetch = cache->max_prefetch;
    char filepath2[256];
    snprintf(filepath2, sizeof(filepath2), "/dev/shm/prefetched%d.txt", cache->log);
    

    for (int dim = 0; dim < NUM_EPSILONS; dim++) {
        uint32_t base_bucket = req->obj_id / epsilons[dim];

        // Process current, left neighbor, and right neighbor
        int offsets[3] = {0, -1, 1};
        // for (int o = 0; o < 1; o++) {
        int neig = 1;
        if(cache->neighbor){
            neig=3;
        }
        for (int o = 0; o < neig; o++) {
            int64_t neighbor_bucket = (int64_t)base_bucket + offsets[o];
        
            if (neighbor_bucket < 0) continue;

            CompositeClusterId id = {
                .eps_idx = dim,
                .bucket_id = (uint32_t)neighbor_bucket
            };

            uint32_t hash_idx = hashCompositeClusterId(&id);
            Cluster *curr = clusterHashTable[hash_idx];

            int total_prefetched = 0;
            


            while (curr) {

                if (!curr->is_valid) {
                    curr = curr->next;
                    continue;
                }
                
                if (curr->id.eps_idx != id.eps_idx || curr->id.bucket_id != id.bucket_id) {
                    curr = curr->next;
                    continue;
                }

                // prevent prefetching too fast
                if (ts - curr->last_prefetched_ts <= curr->neg_count + curr->pos_count) break;

                
                int last_prefetched = 0;
                uint64_t base = curr->base_lba;

                if (base != req->obj_id){

                    /** stat */
                    PrefStat *ps = get_prefstat(base);
                    ps->tries++;

                    // add freq
                    if (limiter.total_prefetches >= limiter.prefetch_limit){
                        // skip this prefetch
                    }else if (ps->tries > 2 && ps->used == 0){
                        // continue; // skip useless prefetch
                        // printf("test\n");
                    }else{
                        // otherwise, allow
                        // limiter.total_prefetches++;
                        /** stat */
                        
                        new_req->obj_id = base;
                        gpointer val = g_hash_table_lookup(size_map, GINT_TO_POINTER(base));
                        if (val){
                            new_req->obj_size = GPOINTER_TO_INT(val);
                            if (record_stats && scheme == Mithril)
                                ((Mithril_params_t *)params)->num_of_check++;
                            // printf("About to check find for obj_id = %lu\n", new_req->obj_id);

                            if (cache->find(cache, new_req, false)){

                            }else{
                                long occupancy = cache->get_occupied_byte(cache);
                            
                                while (occupancy + new_req->obj_size + cache->obj_md_size > (long)cache->cache_size) {
                                    cache->evict(cache, new_req);
                                    occupancy = cache->get_occupied_byte(cache);
                                }

                                cache->insert(cache, new_req);
                                if(cache->io){
                                    send_io();
                                }

                                
                                limiter.total_prefetches++;
                                last_prefetched++;
                                total_prefetched++;
                            }

                            
                        }
                    }
                    

                    
                }

                for (int i = curr->neg_count - 1; i >= 0; i--) {
                    if(total_prefetched >= max_prefetch){
                        break;
                    }
                    /** stat */
                    // add freq
                    if (limiter.total_prefetches >= limiter.prefetch_limit){
                        break; // skip this prefetch
                    }
                    // otherwise, allow
                    /** stat */
                    
                    if (curr->neg_freq[i] == false){
                        curr->neg_freq[i] = true;
                        continue;
                    }
                    uint64_t obj_id = base - curr->neg_deltas[i];
                    if (obj_id == req->obj_id) continue;

                    /** stat */
                    PrefStat *ps = get_prefstat(obj_id);
                    ps->tries++;
                    if (ps->tries > 2 && ps->used == 0){
                        continue; // skip useless prefetch
                    }
                    /** stat */

                    new_req->obj_id = obj_id;
                    gpointer val = g_hash_table_lookup(size_map, GINT_TO_POINTER(obj_id));
                    if (!val) continue;

                    new_req->obj_size = GPOINTER_TO_INT(val);
                    if (record_stats && scheme == Mithril)
                        ((Mithril_params_t *)params)->num_of_check++;
                    // printf("About to check find for obj_id = %lu\n", new_req->obj_id);

                    if (cache->find(cache, new_req, false)) continue;

                    long occupancy = cache->get_occupied_byte(cache);
                    
                    while (occupancy + new_req->obj_size + cache->obj_md_size > (long)cache->cache_size) {
                        
                        cache->evict(cache, new_req);
                        occupancy = cache->get_occupied_byte(cache);
                    }

                    cache->insert(cache, new_req);
                    if(cache->io){
                        send_io();
                    }

                    
                    limiter.total_prefetches++;
                    last_prefetched++;
                    total_prefetched++;
                }

                for (int i = 0; i < curr->pos_count; i++) {
                    if(total_prefetched >= max_prefetch){
                        break;
                    }
                    // add freq
                    if (limiter.total_prefetches >= limiter.prefetch_limit){
                        break; // skip this prefetch
                    }
                    
                    // add freq
                    if (curr->pos_freq[i] == false){
                        curr->pos_freq[i] = true;
                        continue;
                    }
                    uint64_t obj_id = base + curr->pos_deltas[i];
                    if (obj_id == req->obj_id) continue;

                    PrefStat *ps = get_prefstat(obj_id);
                    ps->tries++;
                    if (ps->tries > 2 && ps->used == 0){
                        continue; // skip useless prefetch
                    }

                    new_req->obj_id = obj_id;
                    gpointer val = g_hash_table_lookup(size_map, GINT_TO_POINTER(obj_id));
                    if (!val) continue;

                    new_req->obj_size = GPOINTER_TO_INT(val);
                    if (record_stats && scheme == Mithril)
                        ((Mithril_params_t *)params)->num_of_check++;

                    if (cache->find(cache, new_req, false)) continue;

                    long occupancy = cache->get_occupied_byte(cache);
                    while (occupancy + new_req->obj_size + cache->obj_md_size > (long)cache->cache_size) {
                        cache->evict(cache, new_req);
                        occupancy = cache->get_occupied_byte(cache);
                    }

                    cache->insert(cache, new_req);
                    if(cache->io){
                        send_io();
                    }
                    
                    
                    limiter.total_prefetches++;
                    last_prefetched++;
                    total_prefetched++;
                }
                if (LFU_EVICTION) {
                    update_lfu_position(curr);
                }else{
                    // update_cluster_lru_position(curr);
                }
                
                
                if(last_prefetched > 0){
                    
                    curr->last_prefetched = last_prefetched;
                    curr->last_prefetched_ts = ts;
                }
                
                break;
            }

            if (total_prefetched > 0) {
                
                prefetch_cumul += total_prefetched;
                FILE *fp = fopen(filepath2, "w");
                
                if (fp != NULL) {
                    fprintf(fp, "%d\n", prefetch_cumul);
                    fflush(fp);
                    fclose(fp);
                }
                
            }

            
        } // end for offset -1..+1
    } // end for each dim
    

    bool do_flog = true;
    if(do_flog){
        FILE *fp3 = fopen("/dev/shm/lll.txt", "a");  // "a" = append mode
        if (fp3 == NULL) {
            perror("Error opening file");
            // return 1;
        }

        fprintf(fp3, "%d,", prefetch_cumul);
        fclose(fp3);  // Always close after writing
    }
}

static int first =0;
static int nomor = 0;
static void conventional_cluster(cache_t *cache, const request_t *req, int scheme, int conventional){
    // objids[num_objid++] = req->obj_id;
    void *params = NULL;
    size_t isi = sizeof(req->obj_id);
    
    if (scheme == Mithril) {
        params = (Mithril_params_t *)(cache->prefetcher->params);
    } else if (scheme == PG) {
        params = (PG_params_t *)(cache->prefetcher->params);
    } else if (scheme == OBL) {
        params = (OBL_params_t *)(cache->prefetcher->params);
    }
    
          if(num_objid < NUM_POINTS){
            if(first == 0){
              points = (Point *)malloc(NUM_POINTS * sizeof(Point));
              first = 1;
            }

            points[num_objid].x = req->obj_id; 
            points[num_objid].cluster_id = UNCLASSIFIED;
            num_objid++;
          }
          nomor++;
          
    
    if(nomor  % interval == 0){
    // if(nomor % interval == 0 && nomor != 0){
      printf("Run DBSCAN at %d\n",nomor);
      printf("Convent %d %d\n",conventional,KMEDOIDS);
      //ready
      
      

      // Run DBSCAN algorithm

      if(conventional == DBSCAN){
          double eps = 1000.0; // Adjust the epsilon based on point density
          int min_pts = 2; // Minimum points for a cluster

          // printf("Running DBSCAN on %d points...\n", NUM_POINTS);
          dbscan(points, num_objid, eps, min_pts);

          
      }else if(conventional == KMEANS){
        int cluster_num = interval/10;
        kmeans(points, cluster_num, NUM_POINTS);
        printf("done");
        // KMEANS_kmeans(myobj, labels, numPoints, 10000);
      }else if(conventional == MEANSHIFT){

        int num_points =num_objid-1;

        // Parameters
        double bandwidth = 100000;
        double tol = 0.001;
        int max_iter = 10;

        // Array to store centroids
        uint64_t centroids[num_points];
        printf("Perform mean shift clustering num_points %d\n", num_points);
        
        // Perform mean shift clustering
        mean_shift(points, num_points, bandwidth, tol, max_iter, centroids);
        
      }else if(conventional == KMEDOIDS){
        // printf("preStart");
        int num_points =num_objid-1;

        // Parameters
        // int k = 4;          // Number of clusters
        int k = interval/10;
        int max_iter = 2;  // Maximum iterations
        uint64_t medoids[k]; // Array to store the medoids
        // printf("Start");
        // Run K-Medoids algorithm
        k_medoids(points, num_points, k, max_iter, medoids);
        
      }
      

      
      //ready
      enable=true;
    }
}
int last_prefetched_cluster;
int next_ts;
static void conventional_prefetch(cache_t *cache, const request_t *req, int scheme, request_t *new_req, int conventional){
    
    int clusterID = -1;
    if(conventional == DBSCAN){
      // clusterID = DBSCAN_findClusterID_return(myobj, labels, numPoints, req->obj_id);
      clusterID = get_cluster_id(points, num_objid, req->obj_id);
    }else if(conventional == KMEANS){
      clusterID = get_cluster_id_kmeans(points, num_objid, req->obj_id);
      // clusterID = KMEANS_findClusterID_return(myobj, labels, numPoints, req->obj_id);
    }else if(conventional == MEANSHIFT){
      clusterID = get_cluster_id(points, num_objid, req->obj_id);
      // clusterID = MEANSHIFT_findClusterID_return(myobj, labels, numPoints, req->obj_id);
    }else if(conventional == KMEDOIDS){
      clusterID = get_cluster_id(points, num_objid, req->obj_id);
      // clusterID = MEANSHIFT_findClusterID_return(myobj, labels, numPoints, req->obj_id);
    } 
    
    if(clusterID > -1){ 
      // printf("cluster id %d\n", clusterID);
      if(last_prefetched_cluster != clusterID){ 
        // printf("Ready to prefetch cluster %d\n", clusterID);
        last_prefetched_cluster = clusterID;
    
          void *params = NULL;
            
          // Determine the type of parameters based on the material
          if (scheme == Mithril) {
              params = (Mithril_params_t *)(cache->prefetcher->params);
          } else if (scheme == PG) {
              params = (PG_params_t *)(cache->prefetcher->params);
          } else if (scheme == OBL) {
              params = (OBL_params_t *)(cache->prefetcher->params);
          }
          
            
              int nop=0;
              for (int i = 0; i < num_objid; i++) {
                // printf("objid %d inserted\n", points[i].x);
                  if (points[i].cluster_id == clusterID) { 
                    if(points[i].x == req->obj_id){
                      continue;
                    }

                    
                    
                    new_req->obj_id = points[i].x;
                    
                    if (scheme == Mithril) {
                      new_req->obj_size = GPOINTER_TO_INT(g_hash_table_lookup(
                        ((Mithril_params_t *)params)->cache_size_map, GINT_TO_POINTER(new_req->obj_id)));
                      if (((Mithril_params_t *)params)->output_statistics) {
                        ((Mithril_params_t *)params)->num_of_check += 1;
                      }
                    } else if (scheme == PG) {
                      new_req->obj_size = GPOINTER_TO_INT(g_hash_table_lookup(
                        ((PG_params_t *)params)->cache_size_map, GINT_TO_POINTER(new_req->obj_id)));
                      // if (((PG_params_t *)params)->output_statistics) {
                      //   // ((PG_params_t *)params)->num_of_check += 1;
                      // }
                    } else if (scheme == OBL) {
                      new_req->obj_size = GPOINTER_TO_INT(g_hash_table_lookup(
                        ((OBL_params_t *)params)->cache_size_map, GINT_TO_POINTER(new_req->obj_id)));
                      // if (((OBL_params_t *)params)->output_statistics) {
                      //   // ((PG_params_t *)params)->num_of_check += 1;
                      // }
                    }

                    
                    
                    if (cache->find(cache, new_req, false)) {
                      continue;
                    }
                    while ((long)cache->get_occupied_byte(cache) + new_req->obj_size +
                              cache->obj_md_size >
                          (long)cache->cache_size) {
                      cache->evict(cache, new_req);
                    }
                    cache->insert(cache, new_req);
                    nop++;
                    //insert io from here to ioblazer
                    #ifndef HIT_RATIO_ONLY
                                // printf("send_io from outside \n"); 
                              send_io();
                    #endif
                  }
              }
              
              
        
      }else{
        // printf("Already %d\n", last_prefetched_cluster); 
      }
      
    }
}

// Function to compare for qsort (Ascending Order Sorting)
static int compare(const void *a, const void *b) {
    return (*(uint64_t *)a > *(uint64_t *)b) - (*(uint64_t *)a < *(uint64_t *)b);
}

// addition
// Define a custom data type Cluster2 for cluster statistics.
typedef struct {
    uint64_t cluster_id; // computed as (objid / epsilon)
    uint64_t count;      // number of items in the cluster
    uint64_t sum;        // sum of items for computing centroid
    uint64_t min;        // minimum value in the cluster
    uint64_t max;        // maximum value in the cluster
    double centroid;     // computed centroid (average)
    double dispersion;   // computed dispersion (max - min)
} Cluster2;

typedef struct {
    uint64_t cluster_id;  // Cluster identifier computed as objid / epsilon.
    uint64_t sum;         // Sum of the values in the cluster (for centroid computation).
    int count;            // Number of points in the cluster.
    double centroid;      // Mean value (centroid) of the cluster.
    double scatter;       // Average absolute deviation (scatter) from the centroid.
} Cluster3;

// Comparator for qsort: sort clusters by centroid (ascending).
static int compare_clusters(const void *a, const void *b) {
    const Cluster2 *c1 = (const Cluster2 *)a;
    const Cluster2 *c2 = (const Cluster2 *)b;
    if (c1->centroid < c2->centroid)
        return -1;
    else if (c1->centroid > c2->centroid)
        return 1;
    else
        return 0;
}

static double compute_dbi(const uint64_t *objids, int num_objids, uint64_t epsilon,  bool *pickme, bool sparse) {
    if (num_objids < 2 || epsilon == 0) return 0.0;

    int min = 5;
    if(sparse){
        min = 25;
    }

    Cluster3 *clusters = malloc(num_objids * sizeof(Cluster3));
    int *cluster_indices = malloc(num_objids * sizeof(int));
    if (!clusters || !cluster_indices) exit(EXIT_FAILURE);

    int num_clusters = 0;

    for (int i = 0; i < num_objids; i++) {
        uint64_t cid = objids[i] / epsilon;
        int found = -1;
        for (int j = 0; j < num_clusters; j++) {
            if (clusters[j].cluster_id == cid) {
                found = j;
                break;
            }
        }
        if (found == -1) {
            clusters[num_clusters].cluster_id = cid;
            clusters[num_clusters].sum = objids[i];
            clusters[num_clusters].count = 1;
            clusters[num_clusters].scatter = 0.0;
            cluster_indices[i] = num_clusters;
            num_clusters++;
        } else {
            clusters[found].sum += objids[i];
            clusters[found].count++;
            cluster_indices[i] = found;
        }
    }

    for (int i = 0; i < num_clusters; i++) {
        clusters[i].centroid = (double)clusters[i].sum / clusters[i].count;
    }

    for (int i = 0; i < num_objids; i++) {
        int idx = cluster_indices[i];
        double diff = (double)objids[i] - clusters[idx].centroid;
        clusters[idx].scatter += diff * diff;
    }

    for (int i = 0; i < num_clusters; i++) {
        clusters[i].scatter = sqrt(clusters[i].scatter / clusters[i].count);
        
        if(clusters[i].count > min){
            *pickme=true;
        }
    }

    if (num_clusters <= 1) {
        free(clusters);
        free(cluster_indices);
        return 0.0;
    }

    double dbi_sum = 0.0;
    for (int i = 0; i < num_clusters; i++) {
        double max_ratio = 0.0;
        for (int j = 0; j < num_clusters; j++) {
            if (i == j) continue;
            double dist = fabs(clusters[i].centroid - clusters[j].centroid);
            if (dist == 0.0) {
                max_ratio = INFINITY;
                break;
            }
            double ratio = (clusters[i].scatter + clusters[j].scatter) / dist;
            if (ratio > max_ratio) max_ratio = ratio;
        }
        dbi_sum += max_ratio;
    }

    free(clusters);
    free(cluster_indices);
    return dbi_sum / num_clusters;
}


static double compute_ncg(const uint64_t *objids, int num_objids, uint64_t epsilon) {
    // Allocate an array to hold clusters.
    Cluster2 *clusters = malloc(num_objids * sizeof(Cluster2));
    if (!clusters) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    int num_clusters = 0;

    // Group objids into clusters based on: cluster_id = objid / epsilon.
    for (int i = 0; i < num_objids; i++) {
        uint64_t cid = objids[i] / epsilon;
        int found = 0;
        for (int j = 0; j < num_clusters; j++) {
            if (clusters[j].cluster_id == cid) {
                // Update existing cluster.
                clusters[j].count++;
                clusters[j].sum += objids[i];
                if (objids[i] < clusters[j].min)
                    clusters[j].min = objids[i];
                if (objids[i] > clusters[j].max)
                    clusters[j].max = objids[i];
                found = 1;
                break;
            }
        }
        if (!found) {
            // Create a new cluster.
            clusters[num_clusters].cluster_id = cid;
            clusters[num_clusters].count = 1;
            clusters[num_clusters].sum = objids[i];
            clusters[num_clusters].min = objids[i];
            clusters[num_clusters].max = objids[i];
            num_clusters++;
        }
    }

    // Compute centroid and dispersion for each cluster.
    for (int i = 0; i < num_clusters; i++) {
        clusters[i].centroid = (double)clusters[i].sum / clusters[i].count;
        if (clusters[i].count == 1)
            clusters[i].dispersion = 1e-6;  // For a single-point cluster.
        else
            clusters[i].dispersion = (double)(clusters[i].max - clusters[i].min);
    }

    // If fewer than 2 clusters exist, return 0.
    if (num_clusters < 2) {
        if (clusters) {
        free(clusters);
        }
        return 0.0;
    }

    // Sort clusters by centroid.
    qsort(clusters, num_clusters, sizeof(Cluster2), compare_clusters);

    // Compute normalized gap ratios between adjacent clusters.
    double total_ratio = 0.0;
    int num_gaps = num_clusters - 1;
    for (int i = 0; i < num_gaps; i++) {
        double gap = clusters[i+1].centroid - clusters[i].centroid;
        double avg_dispersion = (clusters[i].dispersion + clusters[i+1].dispersion) / 2.0;
        if (avg_dispersion < 1e-6)
            avg_dispersion = 1e-6; // Prevent division by zero.
        double ratio = gap / avg_dispersion;
        total_ratio += ratio;
    }

    double ncg = total_ratio / num_gaps;
    if (clusters) {
    free(clusters);
    }
    return ncg;
}


static int compare_uint64(const void *a, const void *b) {
    uint64_t x = *(uint64_t *)a;
    uint64_t y = *(uint64_t *)b;
    return (x > y) - (x < y);
}

static double compute_avg_gap_without_outliers(uint64_t *objids, int num_objid) {
    printf("%d\n",num_objid);
    if (num_objid < 3) return 0.0;

    // Step 1: Sort objids
    qsort(objids, num_objid, sizeof(uint64_t), compare_uint64);

    // Step 2: Compute gaps
    int num_gaps = num_objid - 1;
    uint64_t *gaps = malloc(num_gaps * sizeof(uint64_t));
    if (!gaps) return 0.0;

    for (int i = 0; i < num_gaps; i++) {
        gaps[i] = objids[i + 1] - objids[i];
        // printf("%d-%d=%d\n",objids[i + 1], objids[i], gaps[i]);
    }

    // Step 3: Sort the gaps
    qsort(gaps, num_gaps, sizeof(uint64_t), compare_uint64);

    // Step 4: Compute IQR to filter outliers
    int q1_index = num_gaps / 4;
    int q3_index = (3 * num_gaps) / 4;
    uint64_t q1 = gaps[q1_index];
    uint64_t q3 = gaps[q3_index];
    uint64_t iqr = q3 - q1;

    uint64_t lower_bound = (q1 > iqr * 1.5) ? (q1 - iqr * 1.5) : 0;
    uint64_t upper_bound = q3 + iqr * 1.5;

    // Step 5: Filter gaps within IQR bounds and compute average
    uint64_t gap_sum = 0;
    int gap_count = 0;
    for (int i = 0; i < num_gaps; i++) {
        if (gaps[i] >= lower_bound && gaps[i] <= upper_bound) {
            gap_sum += gaps[i];
            gap_count++;
        }
    }

    free(gaps);
    if (gap_count == 0) return 0.0;

    return (double)gap_sum / gap_count;
}

// end addition

static int totalpref=0;
static int hitungan=0;

static void preAssignment(cache_t *cache, const request_t *req, int scheme) {

    if(!check_type){
        if(req->obj_id > BILLION_THRESHOLD){
            billion_type=true;
        }
        check_type=true;
    }
    /** stat addition */
    PrefStat *ps = get_prefstat(req->obj_id);
    if (ps->tries > 0) {
        // means it was prefetched at least once before
        ps->used = 1;
        // ps->tries = 0;  // optional reset, so it can relearn
    } else {
        // first time seen, never prefetched → skip marking used
    } 

    // ps->used = 1; 

    limiter.total_requests++;
    if (limiter.total_requests % limiter.window_size == 0) {
        // Reset every window
        limiter.total_requests = 0;
        limiter.total_prefetches = 0;
    }
    /** stat addition */
    hitungan++;
    
    void *params = NULL;
    // Determine parameter type based on scheme.
    uint64_t current_ts = 0;
    if (scheme == Mithril) {
        params = (Mithril_params_t *)(cache->prefetcher->params);
        current_ts = ((Mithril_params_t *)params)->ts; 
    } else if (scheme == PG) {
        params = (PG_params_t *)(cache->prefetcher->params);
        current_ts = ((PG_params_t *)params)->ts; 
    } else if (scheme == OBL) {
        params = (OBL_params_t *)(cache->prefetcher->params);
        current_ts = ((OBL_params_t *)params)->ts; 
    } else if (scheme == CAPSULE) {
        params = (CAPSULE_params_t *)(cache->prefetcher->params);
        current_ts = ((CAPSULE_params_t *)params)->ts; 
    }
    
    // Define the training sample size and full cycle length.
    const uint64_t sample_size = 1000;      // first 1000 requests in a cycle are used for training
    const uint64_t cycle_size = 10000;        // each cycle is 100,000 requests

    // Training mode: if we are within the first 1000 requests of a cycle.
    if (current_ts % cycle_size < sample_size) {
        // Collect sample data.
        if (num_objid < sample_size) {  // ensure we don't overflow our sample buffer
            objids[num_objid++] = req->obj_id;
        }
        // During training, disable inference.
        enable = false;
    } else {
        // Inference mode.
        // If we've already computed the adaptive dimension in this cycle, simply return.
        if (enable) {
            return;
        } 
        double avg_clean_gap=0;
        // printf("%" PRIu64 "\n",  req->obj_id);
        // printf("%" PRIu64 "\n",   objids[0]);
        // printf("%" PRIu64 "\n",   objids[1]);
        // printf("%" PRIu64 "\n",   objids[2]);
        // printf("%" PRIu64 "\n",   objids[3]);
        if(sekali){
            sekali=false;
            avg_clean_gap = compute_avg_gap_without_outliers(objids, num_objid) * 4;
            printf("Cleaned avg LBA gap: %.2f %d\n", avg_clean_gap, req->obj_id); 
            
            if(avg_clean_gap < epsilons[0]){
                sparse= false; 
                // if(avg_clean_gap > 0){
                //     if(objids[0] > BILLION_THRESHOLD && objids[1] > BILLION_THRESHOLD){

                //     }else{
                //         billion_type=false;
                //         printf("Not billion type\n"); 
                //     }
                    
                // }
                printf("%" PRIu64 "\n",   objids[0]);
                printf("%" PRIu64 "\n",   objids[1]);
                if (avg_clean_gap > 0 &&
                    !(objids[0] > LBA_THRESHOLD && objids[1] > LBA_THRESHOLD)) {
                    billion_type = false;
                    printf("Not billion type\n");
                }

            }
            
            int gap_2dp = (int) round(avg_clean_gap * 100.0);
            // if no spatial locality then don't do any capsule prefetching
            // if (gap_2dp == 0 && req->obj_id > LBA_THRESHOLD ) {
            if (gap_2dp == 0 && req->obj_id > LBA_THRESHOLD ) {
            //if (gap_2dp == 0) {
                //enable_prefetching = false;
                has_spatial = false;
                printf(" %.2f\n", avg_clean_gap);
            } else {
                //enable_prefetching = true;
            } 
        }
        
        
        memset(useful_epsilon, 0, sizeof(useful_epsilon));
        // Compute the best candidate epsilon using the training sample.
        int num_epsilons = sizeof(epsilons) / sizeof(epsilons[0]);
        double best_ncg = -1.0;
        uint64_t best_epsilon = 0;
        int best_dim = 0;
        // double best_dbi = 1.0;
        double best_dbi = 0;
        for (int i = 0; i < num_epsilons; i++) {
            uint64_t candidate = epsilons[i];
            // double ncg = compute_ncg(objids, num_objid, candidate);
            bool pickme=false;
            double dbi = compute_dbi(objids, num_objid, candidate, &pickme, sparse);
            
            
            if(pickme){
                best_dbi = dbi;
                best_epsilon = candidate;
                best_dim = i;
                // printf("Davies–Bouldin Index (DBI): %f\n", dbi);
                break;
            }
            // if (dbi < best_dbi && dbi > 0) { 
            if (dbi > best_dbi) {
                best_dbi = dbi;
                best_epsilon = candidate;
                best_dim = i;
            }
            
        } 
        
        // Update global adaptive parameters.
        epsilon = best_epsilon;
        
        if(!cache->adaptive){
            best_dim = cache->fixed;
            epsilon = epsilons[best_dim];
        }
        // ssd has larger spatial locality
        // if(req->obj_id > LBA_THRESHOLD && limiter.prefetch_limit < SSD_LIMITER){
        if(billion_type && limiter.prefetch_limit < BILLION_L){
            best_dim = 9;
            limiter.prefetch_limit = BILLION_L; 
            // printf("%d\n", best_dim); 
        }else{
            limiter.prefetch_limit = current_cap;
        } 
        dim = best_dim; 
        useful_epsilon[best_dim] = true; 

        

        // Reset the training sample buffer for the next cycle.
        memset(objids, 0, sizeof(objids));
        num_objid = 0;
        
        // Enable inference mode (prevent re-calculation until next training period).
        enable = true;
        postAssignment = true;
    }
} 

/**
 * End of clustering definitions
*/

