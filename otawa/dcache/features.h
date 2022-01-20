/*
 *	otawa::dcache module features
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2013, IRIT UPS.
 *
 *	OTAWA is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	OTAWA is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with OTAWA; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef OTAWA_DCACHE_FEATURES_H_
#define OTAWA_DCACHE_FEATURES_H_

#include <elm/data/Slice.h>
#include <otawa/cache/features.h>
#include <otawa/prop/PropList.h>
#include <otawa/dfa/BitSet.h>
#include <otawa/hard/Cache.h>
#include <otawa/util/Bag.h>
#include <otawa/ai/CFGAnalyzer.h>

namespace otawa {

namespace ilp { class Var; }
namespace hard { class Bank; }
class Inst;

namespace dcache {

typedef enum action_t: t::uint8 {
	NO_ACCESS = 0,
	LOAD = 1,
	STORE = 2,
	PURGE = 3,
	DIRECT_LOAD = 4,
	DIRECT_STORE = 5
} action_t;

typedef enum kind_t: t::uint8 {
	ANY = 0,
	BLOCK = 1,
	RANGE = 2,
	ENUM = 3
} kind_t;


class CacheBlock {
public:
	inline CacheBlock(int tag, int set, int id, const hard::Bank *bank)
		: _tag(tag), _set(set), _id(id), _bank(bank){ }
	inline int tag() const { return _tag; }
	inline int set() const { return _set; }
	inline int id() const { return _id; }
	inline const hard::Bank *bank() const { return _bank; }
private:
	int _tag, _set, _id;
	const hard::Bank *_bank;
};
io::Output& operator<<(io::Output& out, const CacheBlock& b);


// Access
class Access: public PropList {
public:
	
	Access();
	Access(Inst *instruction, action_t action);
	Access(Inst *instruction, action_t action, const CacheBlock *block);
	Access(Inst *instruction, action_t action, int fst, int lst);
	Access(Inst *instruction, action_t action, const Vector<const CacheBlock *>& blocks);
	Access(const Access& b);
	~Access();
	Access& operator=(const Access& a);

	inline Inst *inst() const { return _inst; }
	inline kind_t kind() const { return _kind; }
	inline bool isAny() const { return _kind == ANY; }
	inline action_t action() const { return _action; }
	inline const CacheBlock *block() const { ASSERT(_kind == BLOCK); return data.blk; }
	inline int first() const
		{ ASSERT(_kind == RANGE || _kind == ENUM); return data.range->fst; }
	inline int last() const
		{ ASSERT(_kind == RANGE || _kind == ENUM); return data.range->lst; }
	bool access(int set) const;
	bool access(const CacheBlock *block) const;

	void print(io::Output& out) const;

	inline const Vector<const CacheBlock *>& blocks(void) const
		{ ASSERT(_kind == ENUM); return data.enm->bs; }
	const CacheBlock *blockIn(int set) const;

private:
	void clear();
	void set(const Access& a);

	Inst *_inst;
	kind_t _kind;
	action_t _action;

	typedef struct range_t {
		int fst, lst;
	} range_t;
	typedef struct enum_t: range_t {
		Vector<const CacheBlock *> bs;		
	} enum_t;
	union {
		const CacheBlock *blk;
		range_t *range;
		enum_t *enm;
	} data;
};
inline io::Output& operator<<(io::Output& out, const Access& acc)
	{ acc.print(out); return out; }
io::Output& operator<< (io::Output& out, action_t action);


class BlockCollection;
class SetCollection {
public:
	SetCollection(const hard::Cache& cache, const hard::Memory& mem);
	~SetCollection();
	const CacheBlock *at(Address a);
	const CacheBlock *add(Address a);
	int setCount() const;
	int blockCount(int set) const;
	const CacheBlock *block(int set, int id) const;
	Address address(const CacheBlock *block) const;
	
	inline const hard::Cache& cache() const { return _cache; }
private:
	const hard::Cache& _cache;
	const hard::Memory& _mem;
	BlockCollection **_sets;
};

int actualAssoc(const hard::Cache& cache);


typedef Slice<FragTable<Access> > AccessList;
extern p::id<AccessList> ACCESSES;
extern p::interfaced_feature<const SetCollection> ACCESS_FEATURE;
extern p::interfaced_feature<const SetCollection> CLP_ACCESS_FEATURE;


// Age information
class ACS;
class AgeInfo {
public:
	virtual ~AgeInfo();
	virtual int wayCount() = 0;
	virtual int age(otawa::Block *v, const Access& a, const CacheBlock *b) = 0;
	virtual int age(Edge *e, const Access& a, const CacheBlock *b) = 0;
	virtual ACS *acsAfter(Block *b, int S) = 0;
	ACS *acsBefore(Edge *e, int S);
	virtual ACS *acsBefore(Block *b, int S) = 0;
	virtual ACS *acsAfter(Edge *e, int S) = 0;
	virtual void release(ACS *a) = 0;
};
extern p::interfaced_feature<AgeInfo> MUST_FEATURE;
extern p::interfaced_feature<AgeInfo> MAY_FEATURE;
extern p::interfaced_feature<AgeInfo> PERS_FEATURE;


// MultiAgeInfo information
class MultiACS;
class MultiAgeInfo {
public:
	virtual ~MultiAgeInfo();
	virtual int wayCount() = 0;
	virtual int level(Block *b, const Access& a, const CacheBlock *cb) = 0;
	virtual int level(Edge *e, const Access& a, const CacheBlock *cb) = 0;
	virtual MultiACS *acsAfter(Block *b,int s) = 0;
	virtual MultiACS *acsBefore(Edge *e, int s) = 0;
	virtual MultiACS *acsBefore(Block *b, int s) = 0;
	virtual MultiACS *acsAfter(Edge *e, int s) = 0;
	virtual void release(MultiACS *a) = 0;
};

extern p::interfaced_feature<MultiAgeInfo> MULTI_PERS_FEATURE;


// categories
typedef enum {
	NO_CAT = 0,
	AH = 1,
	AM = 2,
	PE = 3,
	NC = 4,
	CAT_CNT = 5
} category_t;
io::Output& operator<<(io::Output& out, category_t c);

extern p::id<category_t> CATEGORY;
extern p::id<Block *> RELATIVE_TO;
extern p::feature CATEGORY_FEATURE;


// events
extern p::feature EVENT_FEATURE;

} }		// otawa::dcache

#endif /* OTAWA_DCACHE_FEATURES_H_ */
