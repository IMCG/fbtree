/* NTT operation */
NTTEntry* NTT_get(pgno_t pgno);
void NTT_init();
void NTT_new(pgno_t nid, u_int32_t flags);
void NTT_add_pgno(pgno_t nodeID, pgno_t pgno);
void NTT_switch(pgno_t nid);
void NTT_del_list(NTTEntry* entry);
void NTT_dump();


/* Transfom between LOG mode entry and DISK mode entry */
void* log2disk( BLOG* blog);
BLOG* disk2log_bi(BINTERNAL* bi, pgno_t nodeID, u_int32_t seqnum, u_int32_t logversion);
BLOG* disk2log_bl(BLEAF* bl, pgno_t nodeID, u_int32_t seqnum, u_int32_t logversion);
BLOG* disk2log_bl_dbt(const DBT* key, const DBT* data, pgno_t nodeID, u_int32_t seqnum, u_int32_t logversion);

/* Log Buffer Management */
void logpool_init(BTREE* t);
pgno_t logpool_put(BTREE* t, pgno_t nid, const DBT* key, const DBT* data, pgno_t pgno, u_int32_t op);

/* node operation */
void node_addkey(BTREE* t, PAGE* h, const DBT* key, const DBT* data, pgno_t pgno, indx_t index, u_int32_t nbytes);
PAGE * new_node( BTREE *t, pgno_t* nid ,u_int32_t flags);
PAGE* read_node(BTREE* mp , pgno_t x);

indx_t search_node( PAGE * h, u_int32_t ksize, char bytes[]);

void mem2log(BTREE* t, PAGE* pg);
int free_node(BTREE* t, PAGE* h);

/* Other Operation */
int Mpool_put( MPOOL *mp, void *page, u_int flags);
void bt_tosspinned(BTREE* t);
char * makeroom(PAGE*h, indx_t skip, u_int32_t nbytes);
void shrinkroom(PAGE*h, indx_t index, u_int32_t nbytes);
int is_enough_room(PAGE* h , u_int32_t nbytes);

/* fuction for debug */
void logpage_dump(PAGE* h);
void log_dump(BLOG* log);
void disk_entry_dump(void* entry, u_int32_t flags);
