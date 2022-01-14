/*
 *	icat plugin hook
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2020, IRIT UPS.
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

#include <elm/avl/Map.h>
#include <otawa/hard/Cache.h>
#include <otawa/hard/Memory.h>
#include <otawa/proc/ProcessorPlugin.h>
#include <otawa/program.h>

#include "otawa/dcache/features.h"

using namespace elm;
using namespace otawa;

namespace otawa { namespace dcache {

/**
 * @defgroup dcache Data Cache
 *
 * This module is dedicated to the categorisation of data cache accesses.
 * As for the instruction cache, four categories are handled:
 *	* @ref	otawa::cache::ALWAYS_HIT if the access results always in a hit,
 *	* @ref	otawa::cache::FIRST_MISS (also named persistent) if the first access
 *		is unknown and the following accesses results in hits,
 *	* @ref otawa::cache::ALWAYS_MISS if the access results always in a miss,
 *	* @ref otawa::cache::NOT_CLASSIFIED if the previous categories do not apply.
 *
 * This module supports the following data cache configuration:
 *	* replacement policy -- LRU
 *	* write policy -- write-through, write-back (with dirty+purge analysis).
 *
 * The data cache description is obtained from the @ref otawa::hard::CACHE_CONFIGURATION_FEATURE
 * feature and the cache addresses are obtained from @ref otawa::ADDRESS_ANALYSIS_FEATURE.
 * In OTAWA, there are different feature to obtain the addresses represented by
 * the following features:
 *	* otawa::dcache::CLP_BLOCK_FEATURE -- use the plug-in CLP for address representation.
 *
 * To select which address provider to use, one has to requireone of the previous
 * by hand before running other data cache analyses.
 *
 * The different phases to perform data cache analyses are:
 *	* obtain data cache blocks with one data block provider (listed above) --
 *		the result is a list of block accesses (@ref otawa::dcache::BlockAccess)
 * 		hooked to basic blocks with dcache::DATA_BLOCKS properties,
 *	* ACS computation -- according to the accesses list, the ACS (Abstract Cache
 *		State) are computed for each mode MUST, PERS and/or MAY analysis
 *		(dcache::MUST_ACS_FEATURE, dcache::PERS_ACS_FEATURE, dcache::MAY_ACS_FEATURE),
 *	* category derivation -- from the ACS computed in the previous phases,
 *		a category is computed and linked to each block access (dcache::CATEGORY_FEATURE),
 *	* constraint generation in ILP system to bound the variable count the misses
 *		(dcache::CONSTRAINTS_FEATURE),
 *	* time computation -- from the categories, the execution time of a block may
 *		be computed and this feature provides a very trivial way to include
 *		this time in the objective function of ILP system (dcache::WCET_FUNCTION_FEATURE),
 *	* dirtiness and purge analysis is only required for write-back data caches --
 *	  it analyze the dirty bit of cache blocks and depending on their value
 *	  derives if a cache block may/must be written back to memory at replacement
 *	  time (dcache::DIRTY_FEATURE, dcache::PURGE_FEATURE).
 *
 * dcache::WCET_FUNCTION_FEATURE naively add the miss time to the block time.
 * An alternate and more precise approach is to use @ref etime Execution Graph
 * to embed the misses as event in the pipeline execution time calculation.
 *
 * Notice that the MAY is only optional and must be called by hand. In the same way, there is no persistence
 * analysis unless the persistence level is passed at configuration.
 *
 * To use this module, pass it name to the @c otawa-config utility: otawa-config dcache.
 */


/**
 * @class Access
 * This class represents a data cache access that is composed by;
 *	* the instruction that performs it,
 *	* the performed action (BlockAccess::LOAD or BlockAccess::STORE),
 *	* the accessed memory.
 *
 * The action is defined by BlockAccess::action_t that may be:
 * @li @ref NONE -- invalid action (only for convenience),
 * @li @ref READ -- read of cache,
 * @li @ref WRITE -- write of cache,
 * @li @ref PURGE -- target block are purged (possibly written back to memory).
 *
 * Possible kinds of data accesses include:
 * @li ANY		Most imprecised access: one memory accessed is performed but the address is unknown.
 * @li BLOCK	A single block is accessed (given by @ref block() method).
 * @li RANGE	A range of block may be accessed (between @ref first() and @ref last() methods addresses).
 * 
 * @ingroup dcache
 */

/**
 * Build a null block access.
 */
Access::Access(): inst(nullptr), _kind(ANY), _action(NO_ACCESS) {
}

/**
 * Build a block access of type ANY.
 * @param instruction	Instruction performing the access.
 * @param action		Type of action.
 */
Access::Access(Inst *instruction, action_t action)
: inst(instruction), _kind(ANY), _action(action) {
	ASSERT(instruction != nullptr);
}


/**
 * Build a block access to a single block.
 * @param instruction	Instruction performing the access.
 * @param action		Type of action.
 * @param block			Accessed block.
 */
Access::Access(Inst *instruction, action_t action, const Block *block)
: inst(instruction), _kind(BLOCK), _action(action) {
	ASSERT(instruction != nullptr);
	data.blk = block;
}

/**
 * Build a block access of type range. Notice the address of first block may be
 * greater than the address of the second block, meaning that the accessed addresses
 * ranges across the address modulo by 0.
 * @param instruction	Instruction performing the access.
 * @param action		Type of action.
 * @param first			First accessed block (must a cache block boundary address).
 * @param last			Last access block (must a cache block boundary address).
 */
Access::Access(Inst *instruction, action_t action, const Vector<const Block *>& blocks)
: inst(instruction), _kind(ENUM), _action(action) {
	ASSERT(instruction != nullptr);
	data.enm = new enum_t;
	data.enm->fst = blocks.first()->set();
	data.enm->lst = blocks.last()->set();
	data.enm->bs = blocks;
}


/**
 * Clone a block access.
 * @param b		Cloned block access.
 */
Access::Access(const Access& b) {
	set(b);
	
}

///
void Access::clear() {
	switch(_kind) {
	case RANGE:
		delete data.range;
		break;
	case ENUM:
		delete data.enm;
		break;
	default:
		break;
	}	
}

///
void Access::set(const Access& a) {
	inst = a.inst;
	_kind = a._kind;
	_action = a._action;
	switch(_kind) {
	case ANY:
		break;
	case BLOCK:	
		data.blk = a.data.blk;
		break;
	case RANGE:
		data.range = new range_t;
		*data.range = *a.data.range;
		break;
	case ENUM:
		data.enm = new enum_t();
		*data.enm = *a.data.enm;
		break;
	}
}


///
Access::~Access() {
	clear();
}


/**
 * Block access assignement.
 * @param acc	Assigned access.
 * @return		Current block.
 */
Access& Access::operator=(const Access& a) {
	clear();
	set(a);
	return *this;
}




/**
 * @fn Inst *Access::instruction(void) const;
 * Get the instruction performing the access.
 * @return	Instruction performing the access (must be an instruction of the basic block the access is applied to).
 */

/**
 * @fn kind_t Access::kind();
 * Get the kind of the access.
 * @return	Access kind.
 */


/**
 * @fn action_t Access::action() const;
 * Get the performed action.
 * @return	Performed action.
 */

/**
 * @fn const Block *Access::block() const;
 * Only for kind BLOCK, get the accessed block.
 * @return	Accessed block.
 */

/**
 * @fn int Access::first() const;
 * Only for the RANGE kind, get the first accessed block.
 * @return	First accessed block.
 */

/**
 * @fn int Access::last() const
 * Only for the RANGE kind, get the last accessed block.
 * @return	Last accessed block.
 */


/**
 * @fn bool Access::inRange(const Block *block) const;
 * Test if the given block is the range of the given access.
 * @param block		Address of the cache block.
 * @return			True if it is in the range, false else.
 */

/**
 */
void Access::print(io::Output& out) const {
	out << inst->address() << " (" << inst << "): "
		<< action_t(_action) << " @ ";
	switch(_kind) {
	case ANY:
		out << "ANY";
		break;
	case BLOCK:
		out << *data.blk;
		break;
	case RANGE:
		out << '[' << first() << ", " << last() << ']'
			<< "(multiple cache-blocks)";
		break;
	case ENUM:
		out << "{";
		for(auto b: data.enm->bs)
			out << " " << *b;
		out << " }";
		break;
	default:
		ASSERTP(false, "invalid block access kind: " << int(_kind));
		break;
	}
}


/**
 * Test if the given set concerns the range access.
 * @param set	Set to test for.
 * @return		True if the set contains a block of the range, false else.
 */
bool Access::access(int set) const {
	switch(_kind) {
	case ANY:
		return true;
	case BLOCK:
		return data.blk->set() == set;
	default:
		auto f = first(), l = last();
		if(f < l)
			return f <= set && set <= l;
		else
			return f <= set || set <= l;
		break;
	}
}


/**
 * Test if the given block may be concerned by the current access.
 * @param block		Block to test.
 * @return			True if it concerned, false else.
 */
bool Access::access(const Block *block) const {
	switch(kind()) {
	case ANY:
		return true;
	case BLOCK:
		return data.blk == block;
	case ENUM:
		return data.enm->bs.contains(block);
		break;
	case RANGE:
		return access(block->set());
	default:
		ASSERT(false);
		return false;
	}
}

/**
 * @fn const Vector<block_t>& Access::getBlocks(void) const;
 * Get the list of accessed blocks.
 * @warning This function is only valid for a RANGE access.
 * @return	Accessed blocks.
 */

/**
 * Get the block corresponding to the given set.
 * @warning This function is only valid for a RANGE access.
 * @return	Corresponding block or null pointer.
 */
const Block *Access::blockIn(int set) const {
	switch(_kind) {
	case BLOCK:
		if(data.blk->set() == set)
			return data.blk;
		break;
	case ENUM:
		if(access(set)) {
			auto f = first(), l = last();
			auto bs = blocks();
			if(f <= l || set >= f)
				return bs[set];
			else
				return bs[set + (bs.length() - l)];
		}
		break;
	default:
		break;
	}
	return nullptr;
}


///
io::Output& operator<<(io::Output& out, action_t action) {
	static cstring action_names[] = {
		"none",		// NONE = 0
		"load",		// READ = 1
		"store",	// WRITE = 2
		"purge"		// PURGE = 3
	};
	out << action_names[action];
	return out;
}


/**
 * Property providing the list of data accesses to the mempory for a BB.
 * 
 * Feature:
 * * @ref otawa::dcache::ACCESS_FEATURE
 * 
 * @ingroup dcache
 */
p::id<AccessList> ACCESSES("otawa::dcache::ACCESSES");


/**
 * Feature ensuring that a BB has been scanned in order to extract data accesses
 * to the memory.
 * 
 * Properties:
 * * @ref otawa::dcache::ACCESSES
 * 
 * Processors:
 * * @ref otawa::dcache::CLPAccessBuilder
 * 
 * @ingroup dcache
 */
p::interfaced_feature<const SetCollection> ACCESS_FEATURE(
	"otawa::dcache::ACCESS_FEATURE",
	p::make<NoProcessor>()
);


///
class BlockCollection {
public:
	typedef avl::Map<int, Block *> map_t;
	
	inline BlockCollection(const hard::Cache& cache, int set)
		: _cache(cache), _set(set), _cnt(0) { }
	
	~BlockCollection() {
		for(auto b: _map)
			delete b;
	}

	inline const hard::Cache& cache() const { return _cache; }
	inline int count() const { return _cnt; }

	inline Address address(const Block *block) const {
		ASSERT(block->set() == _set);
		return block->tag() << (_set + _cache.blockBits());
	}
	
	const Block *at(Address a) const {
		if(int(_cache.set(a)) != _set)
			return nullptr;
		return _map.get(_cache.tag(a), nullptr);		
	}
	
	const Block *add(int tag, const hard::Bank *bank) {
		int id = -1;
		if(bank->isCached())
			id = _cnt++;
		auto b = new Block(tag, _set, id, bank);
		if(id >= 0)
			blks.add(b);
		_map.put(tag, b);
		return b;
	}
	
private:
	Vector<Block *> blks;
	const hard::Cache& _cache;
	int _set;
	int _cnt;
	map_t _map;
};


/**
 * @class SetCollection
 * Collection of information about all set accesses for the instruction cache access.
 *
 * Interface for ACCESS_FEATURE.
 * @ingroup dcache
 */

/**
 * Build a set collection.
 */
SetCollection::SetCollection(const hard::Cache& cache, const hard::Memory& mem)
: _cache(cache), _mem(mem), _sets(new BlockCollection *[cache.setCount()]) {
	for(int i = 0; i < cache.setCount(); i++)
		_sets[i] = new BlockCollection(cache, i);
}

///
SetCollection::~SetCollection() {
	for(int i = 0; i < _cache.setCount(); i++)
		delete _sets[i];
	delete [] _sets;
}

/**
 * Get the block number corresponding to the given address.
 * @return	Block or null.
 */
const Block *SetCollection::at(Address a) {
	return _sets[_cache.set(a)]->at(a);
}

/**
 * Add a new block corresponding to the given address.
 * @param a		Address of access to get block for.
 * @return		Block for the address.
 */
const Block *SetCollection::add(Address a) {
	
	// already recorded
	int s = _cache.set(a);
	auto b = _sets[s]->at(a);
	if(b != nullptr)
		return b;
	
	// determine bank and identifier
	const hard::Bank *bank = _mem.get(a);
	if(bank == nullptr)
		return nullptr;
	
	// create the block
	return _sets[s]->add(_cache.tag(a), bank);
}

/**
 * Get the count of sets.
 * @return	Count of sets.
 */
int SetCollection::setCount() const {
	return _cache.setCount();
}

/**
 * Get the count of block for the given set.
 * @return	Count of used block.
 */
int SetCollection::blockCount(int set) const {
	return _sets[set]->count();
}


/**
 * Get the address of a cache block from its index.
 * @param set	Container set.
 * @param b		Block index.
 * @return		Address of the corresponding block.
 */
Address SetCollection::address(const Block *block) const {
	return _sets[block->set()]->address(block);
}


///
io::Output& operator<<(io::Output& out, const Block& b) {
	out << "CB" << b.id() << " (set " << b.set() << ", tag " << b.tag()
		<< ", " << b.bank()->name() << ")";
	return out;
}


/// plug-in descriptor
class Plugin: public ProcessorPlugin {
public:
	Plugin(void): ProcessorPlugin("otawa::dcache", Version(1, 0, 0), OTAWA_PROC_VERSION) { }
};

} } // otawa::icat

otawa::dcache::Plugin otawa_dcache;
ELM_PLUGIN(otawa_dcache, OTAWA_PROC_HOOK);
