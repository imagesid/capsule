#ifndef PREFETCHINGALGO_H
#define PREFETCHINGALGO_H

#include <strings.h>

#include "cache.h"
#include "request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct prefetcher;
typedef struct prefetcher *(*prefetcher_create_func_ptr)(const char *);
typedef void (*prefetcher_prefetch_func_ptr)(cache_t *, const request_t *);
typedef void (*prefetcher_handle_find_func_ptr)(cache_t *, const request_t *,
                                                bool);
typedef void (*prefetcher_handle_insert_func_ptr)(cache_t *, const request_t *);
typedef void (*prefetcher_handle_evict_func_ptr)(cache_t *, const request_t *);
typedef void (*prefetcher_free_func_ptr)(struct prefetcher *);
typedef struct prefetcher *(*prefetcher_clone_func_ptr)(struct prefetcher *,
                                                        uint64_t);

typedef struct prefetcher {
  void *params;
  void *init_params;
  prefetcher_prefetch_func_ptr prefetch;
  prefetcher_handle_find_func_ptr handle_find;
  prefetcher_handle_insert_func_ptr handle_insert;
  prefetcher_handle_evict_func_ptr handle_evict;
  prefetcher_free_func_ptr free;
  prefetcher_clone_func_ptr clone;
} prefetcher_t;

enum {
  DBSCAN,
  KMEANS,
  MEANSHIFT,
  KMEDOIDS
};

// affinity propagation Save 2D data of similarity of each pairs
// integer overflow in expression of type ‘int’ results in ‘891396832’

prefetcher_t *create_Mithril_prefetcher(const char *init_paramsm,
                                        uint64_t cache_size);
prefetcher_t *create_OBL_prefetcher(const char *init_paramsm, uint64_t cache_size);
prefetcher_t *create_PG_prefetcher(const char *init_paramsm, uint64_t cache_size);

prefetcher_t *create_Cluster4_prefetcher(const char *init_paramsm, uint64_t cache_size, int conventional);
prefetcher_t *create_CAPSULE_prefetcher(const char *init_paramsm, uint64_t cache_size, double max_metadata);
prefetcher_t *create_PGCluster_prefetcher(const char *init_paramsm, uint64_t cache_size);
prefetcher_t *create_OBLCluster_prefetcher(const char *init_paramsm, uint64_t cache_size);

static inline prefetcher_t *create_prefetcher(const char *prefetching_algo,
                                              const char *prefetching_params,
                                              uint64_t cache_size,
                                              const char *conv_algo,
                                              double max_metadata) {
  int conventional = -1;
  if (strcasecmp(conv_algo, "DBSCAN") == 0) {
    conventional = DBSCAN;
  }else if (strcasecmp(conv_algo, "KMEANS") == 0) {
    conventional = KMEANS;
  }else if (strcasecmp(conv_algo, "MEANSHIFT") == 0) {
    conventional = MEANSHIFT;
  }else if (strcasecmp(conv_algo, "KMEDOIDS") == 0) {
    conventional = KMEDOIDS;
  }
  prefetcher_t *prefetcher = NULL;
  if (strcasecmp(prefetching_algo, "Mithril") == 0) {
    prefetcher = create_Mithril_prefetcher(prefetching_params, cache_size);
  } else if (strcasecmp(prefetching_algo, "OBL") == 0) {
    prefetcher = create_OBL_prefetcher(prefetching_params, cache_size);
  } else if (strcasecmp(prefetching_algo, "PG") == 0) {
    prefetcher = create_PG_prefetcher(prefetching_params, cache_size);
  } else if (strcasecmp(prefetching_algo, "MithrilCluster") == 0) {
    prefetcher = create_Cluster4_prefetcher(prefetching_params, cache_size, conventional); 
  } else if (strcasecmp(prefetching_algo, "CAPSULE") == 0) {
    prefetcher = create_CAPSULE_prefetcher(prefetching_params, cache_size, max_metadata); 
  }else if (strcasecmp(prefetching_algo, "PGCluster") == 0) { 
    prefetcher = create_PGCluster_prefetcher(prefetching_params, cache_size);
  } else if (strcasecmp(prefetching_algo, "OBLCluster") == 0) { 
    prefetcher = create_OBLCluster_prefetcher(prefetching_params, cache_size);
  } else { 
    ERROR("prefetching algo %s not supported\n", prefetching_algo);
  }

  return prefetcher;
}

#ifdef __cplusplus
}
#endif
#endif