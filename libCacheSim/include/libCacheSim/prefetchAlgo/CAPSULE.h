#ifndef CAPSULE_H
#define CAPSULE_H
#include <assert.h>
#include <glib.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "../cache.h"

typedef struct CAPSULE_params {
  int32_t block_size;
  bool do_prefetch;

  uint32_t curr_idx;                // current index in the prev_access_block
  int32_t sequential_confidence_k;  // number of prev sequential accesses to be
                                    // considered as a sequential access
  obj_id_t* prev_access_block;      // prev k accessed

  /** similar to the one in init_params,
   *  but this one uses byts, not percentage
   **/
  gint64 max_metadata_size;

  /** current used metadata_size (unit: byte) **/
  gint64 cur_metadata_size;

  GHashTable *cache_size_map;
  
  /** timestamp, currently reference number **/
  guint64 ts;

  // GHashTable *hash_1000;
  // GHashTable *hash_10000;
  // GHashTable *hash_100000;
  // GHashTable *hash_1000000; 
} CAPSULE_params_t;

typedef struct CAPSULE_init_params {
  int32_t block_size;
  int32_t sequential_confidence_k;
  uint64_t cur_metadata_size;
  uint64_t max_metadata_size;  // unit byte
  
} CAPSULE_init_params_t;

#define DO_PREFETCH(id)

#endif