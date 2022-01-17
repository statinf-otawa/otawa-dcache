/*
 *	Analysis class
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
#ifndef OTAWA_DCACHE_ANALYSIS_H_
#define OTAWA_DCACHE_ANALYSIS_H_

#include <elm/alloc/ListGC.h>
#include <elm/avl/Map.h>
#include <otawa/ai/CFGAnalyzer.h>
#include <otawa/cfg/features.h>
#include <otawa/hard/Cache.h>
#include <otawa/proc/Processor.h>

#include "features.h"

namespace otawa { namespace dcache {

class ACS;

class Domain: public ai::Domain {
public:
	inline Domain(int set): S(set) { }
	virtual ai::State *update(const Access& a, ai::State *s) = 0;
	virtual void collect(ai::state_collector_t f);

	bool implementsCodePrinting() override;
	void printCode(otawa::Block *b, io::Output& out) override;

protected:
	int S;
};

class Analysis: public Processor {
public:
	static p::declare reg;
	Analysis(p::declare& reg);

	void configure(const PropList& props) override;

	ai::State *before(Edge *e, int set);
	ai::State *after(Edge *e, int set);
	ai::State *before(otawa::Block *v, int set);
	ai::State *after(otawa::Block *v, int set);
	ai::State *at(otawa::Block *v, const Access& a, int set);
	ai::State *at(Edge *e, const Access& a, int set);
	void release(ai::State *s);

protected:

	void setup(WorkSpace *ws) override;
	void destroy(WorkSpace *ws) override;
	void processWorkSpace(WorkSpace *ws) override;
	void dump(WorkSpace *ws, Output& out) override;

	virtual Domain *domainFor(const SetCollection& coll, int set) = 0;

	void collect(ai::state_collector_t f);

private:
	ai::State *at(otawa::Block *v, const Access& a, ai::State *s, int set);
	void process(WorkSpace *ws, int set);
	void dump(WorkSpace *ws, int set, Output& out);

	const SetCollection *coll;
	const CFGCollection *cfgs;
	int n;
	AllocArray<Domain *> doms;
	AllocArray<ai::CFGAnalyzer *> anas;
	avl::Map<ai::State *, int> uses;
	Vector<int> only_sets;
};

extern p::id<int> ONLY_SET;

} }		// otawa::dcache

#endif /* OTAWA_DCACHE_ANALYSIS_H_ */
