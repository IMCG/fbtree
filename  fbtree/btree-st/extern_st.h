/* NTT operation */
NTTEntry* NTT_get(pgno_t pgno);
void NTT_add(pgno_t nid, PAGE* pg);
void NTT_add_pgno(pgno_t nodeID, pgno_t pgno);
void NTT_del_list(NTTEntry* entry);
void NTT_dump();

/* Log Operation */
void log_dump(BLOG* log);
void disk_entry_dump(void* entry, u_int32_t flags);
void append_log(PAGE* p , BLOG* blog);
/* Transfom between LOG mode entry and DISK mode entry */
void* log2disk( BLOG* blog);
BLOG* disk2log_bi(BINTERNAL* bi, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion);
BLOG* disk2log_bl(BLEAF* bl, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion);
BLOG* disk2log_bl_dbt(DBT* key, DBT* data, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion);

/* Log Buffer Management */
void logpool_init(BTREE* t);
pgno_t logpool_put(BTREE* t,BLOG* blog);

/* Node Operation */
PAGE * new_node( BTREE *t, pgno_t* nid ,u_int32_t flags);
PAGE* read_node(BTREE* mp , pgno_t x);

void addkey2node_log(PAGE* h ,BLOG* blog);
void addkey2node( PAGE* h, void* bi, indx_t skip);

indx_t search_node( PAGE * h, u_int32_t ksize, char bytes[]);
PAGE* init_node_mem(BTREE* t, pgno_t nid, u_char flags );

void genLogFromNode(BTREE* t, PAGE* pg);
/* Other Operation */
int Mpool_put( MPOOL *mp, void *page, u_int flags);
