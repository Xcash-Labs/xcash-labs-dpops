#ifndef __XCASH_DB_HELPERS_H
#define __XCASH_DB_HELPERS_H

int upsert_json_to_db(const char *db_name, const xcash_dbs_t collection_id, int db_file_index,
                      const char *db_data_source, bool json_array);

int count_db_delegates(void);
int count_db_statistics(void);
int count_db_reserve_proofs(void);
int count_db_reserve_bytes(void);

int count_recs(const bson_t *recs);

int get_db_max_block_height(const char *dbname, size_t *max_block_heigh, size_t *max_reserve_bytes);

#endif