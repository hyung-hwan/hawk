/*
 * $Id$
 *
    Copyright (c) 2006-2020 Chung, Hyung-Hwan. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _HAWK_MAP_H_
#define _HAWK_MAP_H_

/* 
 * it is a convenience header file to switch easily between a red-black tree 
 * and a hash table. You must define either HAWK_MAP_IS_HTB or HAWK_MAP_IS_RBT
 * before including this file.
 */

#if defined(HAWK_MAP_IS_HTB)
#	include <hawk-htb.h>
#	define HAWK_MAP_STYLE_DEFAULT                    HAWK_HTB_STYLE_DEFAULT
#	define HAWK_MAP_STYLE_INLINE_COPIERS             HAWK_HTB_STYLE_INLINE_COPIERS
#	define HAWK_MAP_STYLE_INLINE_KEY_COPIER          HAWK_HTB_STYLE_INLINE_KEY_COPIER
#	define HAWK_MAP_STYLE_INLINE_VALUE_COPIER        HAWK_HTB_STYLE_INLINE_VALUE_COPIER
#	define hawk_getmapstyle(kind)                    hawk_gethtbstyle(kind)
#	define hawk_map_open(mmgr,ext,capa,factor,ks,vs) hawk_htb_open(mmgr,ext,capa,factor,ks,vs)
#	define hawk_map_close(map)                       hawk_htb_close(map)
#	define hawk_map_init(map,mmgr,capa,factor,ks,vs) hawk_htb_init(map,mmgr,capa,factor,ks,vs)
#	define hawk_map_fini(map)                        hawk_htb_fini(map)
#	define hawk_map_getxtn(map)                      hawk_htb_getxtn(map)
#	define hawk_map_getsize(map)                     hawk_htb_getsize(map)
#	define hawk_map_getcapa(map)                     hawk_htb_getcapa(map)
#	define hawk_map_getstyle(map)                    hawk_htb_getstyle(map)
#	define hawk_map_setstyle(map,cbs)                hawk_htb_setstyle(map,cbs)
#	define hawk_map_search(map,kptr,klen)            hawk_htb_search(map,kptr,klen)
#	define hawk_map_upsert(map,kptr,klen,vptr,vlen)  hawk_htb_upsert(map,kptr,klen,vptr,vlen)
#	define hawk_map_ensert(map,kptr,klen,vptr,vlen)  hawk_htb_ensert(map,kptr,klen,vptr,vlen)
#	define hawk_map_insert(map,kptr,klen,vptr,vlen)  hawk_htb_insert(map,kptr,klen,vptr,vlen)
#	define hawk_map_update(map,kptr,klen,vptr,vlen)  hawk_htb_update(map,kptr,klen,vptr,vlen)
#	define hawk_map_cbsert(map,kptr,klen,cb,ctx)     hawk_htb_cbsert(map,kptr,klen,cb,ctx)
#	define hawk_map_delete(map,kptr,klen)            hawk_htb_delete(map,kptr,klen)
#	define hawk_map_clear(map)                       hawk_htb_clear(map)
#	define hawk_init_map_itr(itr,dir)                hawk_init_htb_itr(itr) /* dir not used. not supported */
#	define hawk_map_getfirstpair(map,itr)            hawk_htb_getfirstpair(map,itr)
#	define hawk_map_getnextpair(map,itr)             hawk_htb_getnextpair(map,itr)
#	define hawk_map_walk(map,walker,ctx)             hawk_htb_walk(map,walker,ctx)
#	define HAWK_MAP_WALK_STOP                        HAWK_HTB_WALK_STOP
#	define HAWK_MAP_WALK_FORWARD                     HAWK_HTB_WALK_FORWARD
#	define hawk_map_walk_t                           hawk_htb_walk_t
#	define HAWK_MAP_KEY                              HAWK_HTB_KEY
#	define HAWK_MAP_VAL                              HAWK_HTB_VAL
#	define hawk_map_id_t                             hawk_htb_id_t
#	define hawk_map_t                                hawk_htb_t
#	define hawk_map_pair_t                           hawk_htb_pair_t
#	define hawk_map_style_t                          hawk_htb_style_t
#	define hawk_map_cbserter_t                       hawk_htb_cbserter_t
#	define hawk_map_itr_t                            hawk_htb_itr_t
#	define hawk_map_walker_t                         hawk_htb_walker_t
#	define HAWK_MAP_COPIER_SIMPLE                    HAWK_HTB_COPIER_SIMPLE
#	define HAWK_MAP_COPIER_INLINE                    HAWK_HTB_COPIER_INLINE
#	define HAWK_MAP_COPIER_DEFAULT                   HAWK_HTB_COPIER_DEFAULT
#	define HAWK_MAP_FREEER_DEFAULT                   HAWK_HTB_FREEER_DEFAULT
#	define HAWK_MAP_COMPER_DEFAULT                   HAWK_HTB_COMPER_DEFAULT
#	define HAWK_MAP_KEEPER_DEFAULT                   HAWK_HTB_KEEPER_DEFAULT
#	define HAWK_MAP_SIZER_DEFAULT                    HAWK_HTB_SIZER_DEFAULT
#	define HAWK_MAP_HASHER_DEFAULT                   HAWK_HTB_HASHER_DEFAULT
#	define HAWK_MAP_SIZE(map)                        HAWK_HTB_SIZE(map)
#	define HAWK_MAP_KCOPIER(map)                     HAWK_HTB_KCOPIER(map)
#	define HAWK_MAP_VCOPIER(map)                     HAWK_HTB_VCOPIER(map)
#	define HAWK_MAP_KFREEER(map)                     HAWK_HTB_KFREEER(map)
#	define HAWK_MAP_VFREEER(map)                     HAWK_HTB_VFREEER(map)
#	define HAWK_MAP_COMPER(map)                      HAWK_HTB_COMPER(map) 
#	define HAWK_MAP_KEEPER(map)                      HAWK_HTB_KEEPER(map)
#	define HAWK_MAP_KSCALE(map)                      HAWK_HTB_KSCALE(map) 
#	define HAWK_MAP_VSCALE(map)                      HAWK_HTB_VSCALE(map) 
#	define HAWK_MAP_KPTL(p)                          HAWK_HTB_KPTL(p)
#	define HAWK_MAP_VPTL(p)                          HAWK_HTB_VPTL(p)
#	define HAWK_MAP_KPTR(p)                          HAWK_HTB_KPTR(p)
#	define HAWK_MAP_KLEN(p)                          HAWK_HTB_KLEN(p)
#	define HAWK_MAP_VPTR(p)                          HAWK_HTB_VPTR(p)
#	define HAWK_MAP_VLEN(p)                          HAWK_HTB_VLEN(p)
#elif defined(HAWK_MAP_IS_RBT)
#	include <hawk-rbt.h>
#	define HAWK_MAP_STYLE_DEFAULT                    HAWK_RBT_STYLE_DEFAULT
#	define HAWK_MAP_STYLE_INLINE_COPIERS             HAWK_RBT_STYLE_INLINE_COPIERS
#	define HAWK_MAP_STYLE_INLINE_KEY_COPIER          HAWK_RBT_STYLE_INLINE_KEY_COPIER
#	define HAWK_MAP_STYLE_INLINE_VALUE_COPIER        HAWK_RBT_STYLE_INLINE_VALUE_COPIER
#	define hawk_getmapstyle(kind)                    hawk_getrbtstyle(kind)
#	define hawk_map_open(mmgr,ext,capa,factor,ks,vs) hawk_rbt_open(mmgr,ext,ks,vs)
#	define hawk_map_close(map)                       hawk_rbt_close(map)
#	define hawk_map_init(map,mmgr,capa,factor,ks,vs) hawk_rbt_init(map,mmgr,ks,vs)
#	define hawk_map_fini(map)                        hawk_rbt_fini(map)
#	define hawk_map_getxtn(map)                      hawk_rbt_getxtn(map)
#	define hawk_map_getsize(map)                     hawk_rbt_getsize(map)
#	define hawk_map_getcapa(map)                     hawk_rbt_getsize(map)
#	define hawk_map_getstyle(map)                    hawk_rbt_getstyle(map)
#	define hawk_map_setstyle(map,cbs)                hawk_rbt_setstyle(map,cbs)
#	define hawk_map_search(map,kptr,klen)            hawk_rbt_search(map,kptr,klen)
#	define hawk_map_upsert(map,kptr,klen,vptr,vlen)  hawk_rbt_upsert(map,kptr,klen,vptr,vlen)
#	define hawk_map_ensert(map,kptr,klen,vptr,vlen)  hawk_rbt_ensert(map,kptr,klen,vptr,vlen)
#	define hawk_map_insert(map,kptr,klen,vptr,vlen)  hawk_rbt_insert(map,kptr,klen,vptr,vlen)
#	define hawk_map_update(map,kptr,klen,vptr,vlen)  hawk_rbt_update(map,kptr,klen,vptr,vlen)
#	define hawk_map_cbsert(map,kptr,klen,cb,ctx)     hawk_rbt_cbsert(map,kptr,klen,cb,ctx)
#	define hawk_map_delete(map,kptr,klen)            hawk_rbt_delete(map,kptr,klen)
#	define hawk_map_clear(map)                       hawk_rbt_clear(map)
#	define hawk_init_map_itr(itr,dir)                hawk_init_rbt_itr(itr,dir)
#	define hawk_map_getfirstpair(map,itr)            hawk_rbt_getfirstpair(map,itr)
#	define hawk_map_getnextpair(map,itr)             hawk_rbt_getnextpair(map,itr)
#	define hawk_map_walk(map,walker,ctx)             hawk_rbt_walk(map,walker,ctx)
#	define HAWK_MAP_WALK_STOP                        HAWK_RBT_WALK_STOP
#	define HAWK_MAP_WALK_FORWARD                     HAWK_RBT_WALK_FORWARD
#	define hawk_map_walk_t                           hawk_rbt_walk_t
#	define HAWK_MAP_KEY                              HAWK_RBT_KEY
#	define HAWK_MAP_VAL                              HAWK_RBT_VAL
#	define hawk_map_id_t                             hawk_rbt_id_t
#	define hawk_map_t                                hawk_rbt_t
#	define hawk_map_pair_t                           hawk_rbt_pair_t
#	define hawk_map_style_t                          hawk_rbt_style_t
#	define hawk_map_cbserter_t                       hawk_rbt_cbserter_t
#	define hawk_map_itr_t                            hawk_rbt_itr_t
#	define hawk_map_walker_t                         hawk_rbt_walker_t
#	define HAWK_MAP_COPIER_SIMPLE                    HAWK_RBT_COPIER_SIMPLE
#	define HAWK_MAP_COPIER_INLINE                    HAWK_RBT_COPIER_INLINE
#	define HAWK_MAP_COPIER_DEFAULT                   HAWK_RBT_COPIER_DEFAULT
#	define HAWK_MAP_FREEER_DEFAULT                   HAWK_RBT_FREEER_DEFAULT
#	define HAWK_MAP_COMPER_DEFAULT                   HAWK_RBT_COMPER_DEFAULT
#	define HAWK_MAP_KEEPER_DEFAULT                   HAWK_RBT_KEEPER_DEFAULT
#	define HAWK_MAP_SIZER_DEFAULT                    (HAWK_NULL)
#	define HAWK_MAP_HASHER_DEFAULT                   (HAWK_NULL)
#	define HAWK_MAP_SIZE(map)                        HAWK_RBT_SIZE(map)
#	define HAWK_MAP_KCOPIER(map)                     HAWK_RBT_KCOPIER(map)
#	define HAWK_MAP_VCOPIER(map)                     HAWK_RBT_VCOPIER(map)
#	define HAWK_MAP_KFREEER(map)                     HAWK_RBT_KFREEER(map)
#	define HAWK_MAP_VFREEER(map)                     HAWK_RBT_VFREEER(map)
#	define HAWK_MAP_COMPER(map)                      HAWK_RBT_COMPER(map) 
#	define HAWK_MAP_KEEPER(map)                      HAWK_RBT_KEEPER(map)
#	define HAWK_MAP_KSCALE(map)                      HAWK_RBT_KSCALE(map) 
#	define HAWK_MAP_VSCALE(map)                      HAWK_RBT_VSCALE(map) 
#	define HAWK_MAP_KPTL(p)                          HAWK_RBT_KPTL(p)
#	define HAWK_MAP_VPTL(p)                          HAWK_RBT_VPTL(p)
#	define HAWK_MAP_KPTR(p)                          HAWK_RBT_KPTR(p)
#	define HAWK_MAP_KLEN(p)                          HAWK_RBT_KLEN(p)
#	define HAWK_MAP_VPTR(p)                          HAWK_RBT_VPTR(p)
#	define HAWK_MAP_VLEN(p)                          HAWK_RBT_VLEN(p)
#else
#	error define HAWK_MAP_IS_HTB or HAWK_MAP_IS_RBT before including this file
#endif

#endif
