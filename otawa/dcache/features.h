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


class Block {
public:
	inline Block(int tag, int set, int id, const hard::Bank *bank)
		: _tag(tag), _set(set), _id(id), _bank(bank){ }
	inline int tag() const { return _tag; }
	inline int set() const { return _set; }
	inline int id() const { return _id; }
	inline const hard::Bank *bank() const { return _bank; }
private:
	int _tag, _set, _id;
	const hard::Bank *_bank;
};
io::Output& operator<<(io::Output& out, const Block& b);


// Data
class Access: public PropList {
public:
	
	Access();
	Access(Inst *instruction, action_t action);
	Access(Inst *instruction, action_t action, const Block *block);
	Access(Inst *instruction, action_t action, int fst, int lst);
	Access(Inst *instruction, action_t action, const Vector<const Block *>& blocks);
	Access(const Access& b);
	~Access();
	Access& operator=(const Access& a);

	inline Inst *instruction() const { return inst; }
	inline kind_t kind() const { return _kind; }
	inline bool isAny() const { return _kind == ANY; }
	inline action_t action() const { return _action; }
	inline const Block *block() const { ASSERT(_kind == BLOCK); return data.blk; }
	inline int first() const
		{ ASSERT(_kind == RANGE || _kind == ENUM); return data.range->fst; }
	inline int last() const
		{ ASSERT(_kind == RANGE || _kind == ENUM); return data.range->lst; }
	inline bool inRange(const Block *block) const {
		auto set = block->set();
		if(first() <= last())
			return first() <= set && set <= last();
		else
			return set <= last() || first() <= set;
	}
	bool access(int set) const;
	bool access(const Block *block) const;

	void print(io::Output& out) const;

	inline const Vector<const Block *>& blocks(void) const
		{ ASSERT(_kind == ENUM); return data.enm->bs; }
	const Block *blockIn(int set) const;

private:
	void clear();
	void set(const Access& a);

	Inst *inst;
	kind_t _kind;
	action_t _action;

	typedef struct range_t {
		int fst, lst;
	} range_t;
	typedef struct enum_t: range_t {
		Vector<const Block *> bs;		
	} enum_t;
	union {
		const Block *blk;
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
	const Block *at(Address a);
	const Block *add(Address a);
	int setCount() const;
	int blockCount(int set) const;
	Address address(const Block *block) const;
	
	inline const hard::Cache& cache() const { return _cache; }
private:
	const hard::Cache& _cache;
	const hard::Memory& _mem;
	BlockCollection **_sets;
};

int actualAssoc(const hard::Cache& cache);


// useful typedefs
typedef Slice<FragTable<Access> > AccessList;
extern p::id<AccessList> ACCESSES;
extern p::interfaced_feature<const SetCollection> ACCESS_FEATURE;
extern p::interfaced_feature<const SetCollection> CLP_ACCESS_FEATURE;

} }		// otawa::dcache

#endif /* OTAWA_DCACHE_FEATURES_H_ */
