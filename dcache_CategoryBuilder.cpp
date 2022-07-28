/*
 *	CategoryBuilder class
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2022, IRIT UPS.
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

#include <otawa/cfg/features.h>
#include <otawa/cfg/Loop.h>
#include <otawa/hard/Memory.h>
#include <otawa/proc/BBProcessor.h>

#include "otawa/dcache/features.h"

namespace otawa { namespace dcache { 

///
class CategoryBuilder: public BBProcessor {
public:
	static p::declare reg;
	CategoryBuilder(p::declare& r = reg):
		BBProcessor(r),
		must(nullptr),
		may(nullptr),
		pers(nullptr),
		mpers(nullptr),
		mem(nullptr),
		A(0)
	{
		array::set(cnt, CAT_CNT, 0);
	}

protected:

	void setup(WorkSpace *ws) override {

		// get MUST analysis
		must = MUST_FEATURE.get(ws);
		ASSERT(must);
		A = must->wayCount();

		// get MAY analysis
		if(ws->provides(MAY_FEATURE)) {
			may = MAY_FEATURE.get(ws);
			ASSERT(may != nullptr);
		}

		// get PERS analysis
		if(ws->provides(PERS_FEATURE)) {
			pers = PERS_FEATURE.get(ws);
			ASSERT(pers != nullptr);
		}

		// get multi-PERS analysis
		if(ws->provides(MULTI_PERS_FEATURE)) {
			mpers = MULTI_PERS_FEATURE.get(ws);
			ASSERT(mpers != nullptr);
		}
		
		// get the memory
		mem = hard::MEMORY_FEATURE.get(ws);
		
		// get the cache
		cache = &ACCESS_FEATURE.get(ws)->cache();
	}

	category_t classify(Edge *e, const Access& a, const CacheBlock *cb, Block*& h) {
		h = nullptr;
		
		// AH?
		if(must->age(e, a, cb) < A)
			return AH;

		// PE?
		else if(mpers != nullptr) {
			auto n = mpers->level(e, a, cb);
			if(n != 0) {
				auto l = Loop::of(e->sink());
				for(int i = 1; i < n; i++) {
					if(!l->isTop())
						l = l->parent();
					else if(e->sink()->cfg()->callCount() == 1)
						l = Loop::of(*e->sink()->cfg()->callers().begin());
					else
						break;
				}
				if(l->isTop())
					h = l->cfg()->entry()->outEdges().begin()->sink();
				else
					h = l->header();
				return PE;
			}
		}
			
		// PE?
		if(pers != nullptr && pers->age(e, a, cb) < A) {
			auto l = Loop::of(e->sink());
			if(!l->isTop())
				while(!l->parent()->isTop())
					l = l->parent();
			if(l->isTop())
				h = l->cfg()->entry()->outEdges().begin()->sink();
			else
				h = l->header();
			return PE;
		}
			
		// AM?
		if(may != nullptr && may->age(e, a, cb) >= A)
			return AM;
			
		// NOT-CLASSIFIED
		else
			return NC;
	}

	void processAny(Edge *e, Access &a) {
		CATEGORY(a) = NC;
	}

	void processBlock(Edge *e, Access& a) {
		Block *h;
		auto c = classify(e, a, a.block(), h);
		CATEGORY(a) = c;
		if(c == PE)
			RELATIVE_TO(a) = h;
	}
	
	void processEnum(Edge *e, Access& a) {
		
		// prepare according to all blocks
		Block *fh = nullptr;
		Block *h;
		auto c = NO_CAT;
		for(auto cb: a.blocks()) {
			auto nc = classify(e, a, cb, h);
			if(c == NO_CAT)
				c = nc;
			else if(c != nc) {
				c = NC;
				break;
			}
			if(nc == PE && c == PE) {
				if(fh == nullptr)
					fh = h;
				else {
					auto fl = Loop::of(fh);
					auto l = Loop::of(h);
					if(fl->includes(l))
						fl = l;
				}
			}
		}
		
		// build the event
		CATEGORY(a) = c;
		if(c == PE)
			RELATIVE_TO(a) = fh;
	}
	
	void processDirect(Edge *e, Access& a) {
		CATEGORY(a) = AM;
	}

	/**
	 * Build category for the given access. There is a specific optimization 
	 * for multiple access instruction to T address: as the accesses are considered
	 * as sequential, the number of NC events is: roundup(access size * access/
	 * block size) to the worst case time. Subsequent accesses in the same
	 * instruction can be ignored in this case.
	 * @param e		Current edge.
	 * @param a		Access to build event for.
	 * @return		True if a multiple to T has been managed, false else.
	 */
	void processAccess(Edge *e, Access& a) {

		// build the event
		switch(a.action()) {
		case NO_ACCESS:
			break;

		case DIRECT_LOAD:
		case DIRECT_STORE:
			processDirect(e, a);
			break;
			
		case PURGE:
			break;

		case LOAD:
		case STORE:
			switch(a.kind()) {
			case ANY:
				processAny(e, a);
				break;
			case BLOCK:
				processBlock(e, a);
				break;
			case ENUM:
				processEnum(e, a);
				break;
			case RANGE:
				processAny(e, a);
				break;
			default:
				ASSERT(false);
				break;
			}
			break;
		}
	}
	
	void processBB(WorkSpace *ws, CFG *g, Block *b_) override {
		if(!b_->isBasic())
			return;
		auto b = b_->toBasic();

		// set events
		for(auto e: b->inEdges())
			for(auto& a: *ACCESSES(b))
				processAccess(e, a);
	}
	
	void dumpBB(Block *v, io::Output& out) override {
		for(auto e: v->inEdges()) {
			out << "\t\talong " << e << io::endl;
			for(const auto& a: *ACCESSES(e)) {
				auto c = CATEGORY(a);
				out << "\t\t\t" << a << ": " << c;
				if(c == PE)
					out << " (" << *RELATIVE_TO(a) << ")";
				out << io::endl;
			}
		}
	}

private:

	AgeInfo *must, *may, *pers;
	MultiAgeInfo *mpers;
	const hard::Memory *mem;
	int A;
	int cnt[CAT_CNT];
	const hard::Cache *cache;
};


///
p::declare CategoryBuilder::reg = p::init("otawa::dcache::CategoryBuilder", Version(1, 0, 0))
	.require(MUST_FEATURE)
	.require(EXTENDED_LOOP_FEATURE)
	.require(hard::MEMORY_FEATURE)
	.require(ACCESS_FEATURE)
	.extend<BBProcessor>()
	.make<CategoryBuilder>();


/**
 * Assign to each data cache access a category representing its cache behaviour.
 * 
 * Properties:
 *  @ref otawa::dcache::CATEGORY
 * 
 * Default implementation: @ref otawa::dcache::CategoryBuilder
 * 
 * @ingroup dcache
 */
p::feature CATEGORY_FEATURE("otawa::dcache::CATEGORY_FEATURE", p::make<CategoryBuilder>());

/**
 * Property hooked to data cache access (@ref dcache::Access) recording the access category
 * (@ref dcache::category_t).
 * 
 * @par Hooks
 * * @ref dcache::Access
 * 
 * @par Features
 * * @ref dcache::CATEGORY_FEATURE
 */
p::id<category_t> CATEGORY("otawa::dcache::CATEGORY", NO_CAT);


/**
 * Supplement to @ref dcache::CATEGORY feature giving the loop header when
 * a catagory is of type @ref dcache::PE.
 * 
 * @par Hooks
 * * @ref dcache::Access
 * 
 * @par Features
 * * @ref dcache::CATEGORY_FEATURE
 */
p::id<Block *> RELATIVE_TO("otawa::dcache::RELATIVE_TO", nullptr);


///
io::Output& operator<<(io::Output& out, category_t c) {
	cstring labs[] = {
		"NO_CAT",
		"AH",
		"AM",
		"PE",
		"NC",
	};
	if(c >= CAT_CNT)
		out << "<unknown>";
	else
		out << labs[c];
	return out;
}

}}	// otawa::dcache

