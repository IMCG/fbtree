#	@(#)Makefile	8.9 (Berkeley) 7/14/94

LIBDB=	libdb.a
# - bt_delete bt_overflow bt_seq
OBJ2=	bt_close.o bt_conv.o bt_debug.o bt_get_st.o bt_open.o \
	bt_page_st.o bt_put_st.o bt_search_st.o bt_split_st.o \
	bt_utils.o
OBJ3=	db.o
OBJ4=	mpool.o
OBJ5=	error.o errorlog.o NTT.o log.o node.o

MISC=

${LIBDB}: ${OBJ2} ${OBJ3} ${OBJ4} ${OBJ5} ${MISC}
	rm -f $@
	ar cq $@ \
	    `lorder ${OBJ2} ${OBJ3} ${OBJ4} ${OBJ5} ${MISC} | tsort`
	ranlib $@

clean:
	rm -f ${LIBDB} ${OBJ2} ${OBJ3} ${OBJ4} ${OBJ5} ${MISC}

OORG=	-O
CL=	${CC} -g -c \
	-DLOG_DEBUG -DNODE_DEBUG -DNTT_DEBUG -DBT_PUT_DEBUG -DBT_SPLIT_DEBUG \
	-D__DBINTERFACE_PRIVATE ${OORG} -I../include -I../linux/include -I../../include
#-DNTT_DEBUG -DNODE_DEBUG -DBT_PUT_DEBUG -DBT_SPLIT_DEBUG\

bt_close.o: ../../btree-st/bt_close.c
	${CL} -I../../btree-st ../../btree-st/bt_close.c
bt_conv.o: ../../btree-st/bt_conv.c
	${CL} -I../../btree-st ../../btree-st/bt_conv.c
bt_debug.o: ../../btree-st/bt_debug.c
	${CL} -I../../btree-st ../../btree-st/bt_debug.c
bt_delete.o: ../../btree-st/bt_delete.c
	${CL} -I../../btree-st ../../btree-st/bt_delete.c
bt_get_st.o: ../../btree-st/bt_get_st.c
	${CL} -I../../btree-st ../../btree-st/bt_get_st.c
bt_open.o: ../../btree-st/bt_open.c
	${CL} -I../../btree-st ../../btree-st/bt_open.c
bt_overflow.o: ../../btree-st/bt_overflow.c
	${CL} -I../../btree-st ../../btree-st/bt_overflow.c
bt_page_st.o: ../../btree-st/bt_page_st.c
	${CL} -I../../btree-st ../../btree-st/bt_page_st.c
bt_put_st.o: ../../btree-st/bt_put_st.c
	${CL} -I../../btree-st ../../btree-st/bt_put_st.c
bt_search_st.o: ../../btree-st/bt_search_st.c
	${CL} -I../../btree-st ../../btree-st/bt_search_st.c
bt_seq.o: ../../btree-st/bt_seq.c
	${CL} -I../../btree-st ../../btree-st/bt_seq.c
bt_split_st.o: ../../btree-st/bt_split_st.c
	${CL} -I../../btree-st ../../btree-st/bt_split_st.c
bt_stack.o: ../../btree-st/bt_stack.c
	${CL} -I../../btree-st ../../btree-st/bt_stack.c
bt_utils.o: ../../btree-st/bt_utils.c
	${CL} -I../../btree-st ../../btree-st/bt_utils.c

db.o: ../../db/db.c
	${CL} ../../db/db.c

mpool.o: ../../mpool/mpool.c
	${CL} -I../../mpool ../../mpool/mpool.c

error.o: ../../error/error.c
	${CL} ../../error/error.c

errorlog.o: ../../error/errorlog.c
	${CL} ../../error/errorlog.c

NTT.o: ../../btree-st/NTT.c
	${CL} -I../../btree-st ../../btree-st/NTT.c
log.o: ../../btree-st/log.c
	${CL} -I../../btree-st ../../btree-st/log.c
node.o: ../../btree-st/node.c
	${CL} -I../../btree-st ../../btree-st/node.c

memmove.o:
	${CC} -DMEMMOVE -c -O -I. -Iinclude clib/memmove.c
mktemp.o:
	${CC} -c -O -I. -Iinclude clib/mktemp.c
snprintf.o:
	${CC} -c -O -I. -Iinclude clib/snprintf.c
