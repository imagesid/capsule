//  Capsule.c
//  libCacheSim
//
//  Created by imagesid on 25/02/03.
//  Copyright © 2025 imagesid. All rights reserved.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include "ClusterPrefetcher.h"
#include <stdbool.h>
#include "../../../libCacheSim/bin/cachesim/debug.h"

static int scheme = CAPSULE;


#ifdef __cplusplus
extern "C" {
#endif 

// ***********************************************************************
// ****                                                               ****
// ****               helper function declarations                    ****
// ****                                                               ****
// ***********************************************************************

// Define the struct BEFORE you declare meta
typedef struct {
    uint64_t last_access;
    bool was_prefetched;
} BlockMeta;

// now you can declare meta
static BlockMeta *meta = NULL;

// define the threshold correctly

static size_t meta_capacity = 0;

static const char *CAPSULE_default_params(void) { return "block-size=512, sequential-confidence-k=4, max_metadata_size=0.1"; }

static void set_CAPSULE_default_init_params(CAPSULE_init_params_t *init_params, double max_metadata) {
  init_params->block_size = 1;  // for general use
  // init_params->max_metadata_size = 0.1;
  // printf("Max Metadata: %f\n", max_metadata);
  init_params->max_metadata_size = max_metadata;
  init_params->sequential_confidence_k = 4;   
}

static void CAPSULE_parse_init_params(const char *cache_specific_params, CAPSULE_init_params_t *init_params, double max_metadata) {
  char *params_str = strdup(cache_specific_params);

    
  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }
    
    if (strcasecmp(key, "block-size") == 0) {
      init_params->block_size = atoi(value);
    } else if (strcasecmp(key, "sequential-confidence-k") == 0) {
      init_params->sequential_confidence_k = atoi(value);
    } else if (strcasecmp(key, "max-metadata-size") == 0) {
      init_params->max_metadata_size = atof(value);
    } else {
      ERROR("OBL does not have parameter %s\n", key);
      printf("default params: %s\n", CAPSULE_default_params());
      exit(1);
    }
  }
  // init_params->max_metadata_size = max_metadata;
}

static void set_CAPSULE_params(CAPSULE_params_t *CAPSULE_params, CAPSULE_init_params_t *init_params, uint64_t cache_size, double max_metadata) {
  CAPSULE_params->block_size = init_params->block_size;
  CAPSULE_params->sequential_confidence_k = init_params->sequential_confidence_k;
  CAPSULE_params->do_prefetch = false;
  if (CAPSULE_params->sequential_confidence_k <= 0) {
    printf("sequential_confidence_k should be positive\n");
    exit(1);
  }
  CAPSULE_params->prev_access_block = (obj_id_t *)malloc(CAPSULE_params->sequential_confidence_k * sizeof(obj_id_t));
  for (int i = 0; i < CAPSULE_params->sequential_confidence_k; i++) {
    CAPSULE_params->prev_access_block[i] = UINT64_MAX;
  }
  CAPSULE_params->curr_idx = 0; 

  CAPSULE_params->cur_metadata_size = 0;

  // Add global/static metadata size:
  CAPSULE_params->cur_metadata_size += (size_t)HASH_SIZE * sizeof(Cluster*); // e.g. 4096 * 8 = 32 KB

  CAPSULE_params->cur_metadata_size += (size_t)MAX_FREQ * sizeof(Cluster*);  // e.g. 65536 * 8 = 512 KB
  
  CAPSULE_params->max_metadata_size = (uint64_t)(init_params->block_size * cache_size * max_metadata);
  
  /** add metadata stats*/
  size_t target_entries = (uint64_t)(init_params->block_size * cache_size) / 64;
  if (target_entries < 1e6) target_entries = 1e6;
  if (target_entries > 4e6) target_entries = 4e6;

  // track PrefStat metadata size
  size_t md_init_size = target_entries * sizeof(PrefStat);
  CAPSULE_params->cur_metadata_size += md_init_size; 
  // printf("cache_size %d\n", cache_size);
  // printf("prefstat size %d\n", md_init_size);
  /** add metadata stats*/
  
  CAPSULE_params->ts = 0;
  CAPSULE_params->cache_size_map =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);


}

static void ensure_meta_capacity(uint64_t obj_id) {
    if (obj_id >= meta_capacity) {
        size_t new_capacity = meta_capacity == 0 ? 1024 : meta_capacity;
        while (new_capacity <= obj_id) {
            new_capacity *= 2;  // grow exponentially
        }

        BlockMeta *new_meta = realloc(meta, new_capacity * sizeof(BlockMeta));
        if (!new_meta) {
            fprintf(stderr, "Error: realloc failed for meta table\n");
            exit(1);
        }

        // zero-initialize the new region
        memset(new_meta + meta_capacity, 0,
               (new_capacity - meta_capacity) * sizeof(BlockMeta));

        meta = new_meta;
        meta_capacity = new_capacity;
    }
}


/**************************************************************************
 **                      prefetcher interfaces
 **
 ** create, free, clone, handle_find, handle_insert, handle_evict, prefetch
 **************************************************************************/
prefetcher_t *create_CAPSULE_prefetcher(const char *init_params, uint64_t cache_size, double max_metadata);
/**
 check if the previous access is sequential. If true, set do_prefetch to true.

@param cache the cache struct
@param req the request containing the request
@return
*/ 
static void CAPSULE_handle_find(cache_t *cache, const request_t *req, bool hit) {
  
  CAPSULE_params_t *CAPSULE_params = (CAPSULE_params_t *)(cache->prefetcher->params);
  int32_t sequential_confidence_k = CAPSULE_params->sequential_confidence_k;

  /*use cache_size_map to record the current requested obj's size*/
  g_hash_table_insert(CAPSULE_params->cache_size_map,
                      GINT_TO_POINTER(req->obj_id),
                      GINT_TO_POINTER(req->obj_size));

 
  uint64_t current_time = CAPSULE_params->ts;  
  uint64_t reuse_dist = 0;

  preAssignment(cache, req, scheme);
  if(!hit){
    insertOrAppendToCluster(cache, req, scheme); 
  } 

}

/**
 @param cache the cache struct
 @param req the request containing the request
 @return
 */
static void CAPSULE_prefetch(cache_t *cache, const request_t *req) {
  CAPSULE_params_t *CAPSULE_params = (CAPSULE_params_t *)(cache->prefetcher->params);

  request_t *new_req = new_request();
    
    new_req->obj_id = req->obj_id + 1;
    new_req->obj_size = GPOINTER_TO_INT(g_hash_table_lookup(
          CAPSULE_params->cache_size_map, GINT_TO_POINTER(new_req->obj_id)));
  
  prefetchCluster(cache, req, scheme, new_req);
  free_request(new_req);
    
  CAPSULE_params->ts++;
}

static void free_CAPSULE_prefetcher(prefetcher_t *prefetcher) {
  CAPSULE_params_t *CAPSULE_params = (CAPSULE_params_t *)prefetcher->params;
  free(CAPSULE_params->prev_access_block);
  g_hash_table_destroy(CAPSULE_params->cache_size_map);
  my_free(sizeof(CAPSULE_params_t), CAPSULE_params);
  if (prefetcher->init_params) {
    free(prefetcher->init_params);
  }
  my_free(sizeof(prefetcher_t), prefetcher);
}

static prefetcher_t *clone_CAPSULE_prefetcher(prefetcher_t *prefetcher, uint64_t cache_size) {
  
  return create_CAPSULE_prefetcher(prefetcher->init_params, cache_size, 0.1);
}

prefetcher_t *create_CAPSULE_prefetcher(const char *init_params, uint64_t cache_size, double max_metadata) {
  init(cache_size);
  
  CAPSULE_init_params_t *CAPSULE_init_params = my_malloc(CAPSULE_init_params_t);
  memset(CAPSULE_init_params, 0, sizeof(CAPSULE_init_params_t));

  set_CAPSULE_default_init_params(CAPSULE_init_params, max_metadata);
  if (init_params != NULL) {
    CAPSULE_parse_init_params(init_params, CAPSULE_init_params, max_metadata);
  }

  CAPSULE_params_t *CAPSULE_params = my_malloc(CAPSULE_params_t);
  set_CAPSULE_params(CAPSULE_params, CAPSULE_init_params, cache_size, max_metadata);

  prefetcher_t *prefetcher = (prefetcher_t *)my_malloc(prefetcher_t);
  memset(prefetcher, 0, sizeof(prefetcher_t));
  prefetcher->params = CAPSULE_params;
  prefetcher->prefetch = CAPSULE_prefetch;
  prefetcher->handle_find = CAPSULE_handle_find;
  prefetcher->handle_insert = NULL;
  prefetcher->handle_evict = NULL;
  prefetcher->free = free_CAPSULE_prefetcher;
  prefetcher->clone = clone_CAPSULE_prefetcher;
  if (init_params) {
    prefetcher->init_params = strdup(init_params);
  }

  my_free(sizeof(CAPSULE_init_params_t), CAPSULE_init_params);
  return prefetcher;
}