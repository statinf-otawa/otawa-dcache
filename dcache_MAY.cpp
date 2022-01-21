/*
 *	MAY class implementation
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

#include <elm/array.h>
#include <elm/data/ListMap.h>
#include "otawa/dcache/Analysis.h"
#include "otawa/dcache/MAY.h"

namespace otawa { namespace dcache {

/**
 * Implements the MAY instruction cache analysis.
 * @ingroup dcache
 */
class MAYAnalysis: public Analysis, public AgeInfo, public GCManager {
public:
	static p::declare reg;
	MAYAnalysis(): Analysis(reg), A(0), gc(*this) { }

	void collect(AbstractGC& gc) override {
		Analysis::collect([&](ai::State *s) { gc.mark(s, sizeof(ACS)); });
	}

	void clean(void *p) override {
		reinterpret_cast<ACS *>(p)->~ACS();
	}

	void *interfaceFor(const AbstractFeature& f) override {
		if(&f == &MAY_FEATURE)
			return static_cast<AgeInfo *>(this);
		else
			return nullptr;
	}

	int wayCount() override {
		return A;
	}

	int age(Block *b, const Access& a, const CacheBlock *cb) override {
		auto s = acs(at(b, a, cb->set()));
		auto r = s->age[cb->id()];
		Analysis::release(s);
		return r;
	}

	int age(Edge *e, const Access& a, const CacheBlock *cb) override {
		auto s = acs(at(e, a, cb->set()));
		auto r = s->age[cb->id()];
		Analysis::release(s);
		return r;
	}

	ACS *acsBefore(Block *b, int s) override {
		return acs(before(b, s));
	}

	ACS *acsAfter(Block *b, int s) override {
		return acs(after(b, s));
	}

	ACS *acsAfter(Edge *e, int s) override {
		return acs(after(e, s));
	}

	void release(ACS *a) override {
		Analysis::release(a);
	}

protected:

	void setup(WorkSpace *ws) override {
		A = actualAssoc(ACCESS_FEATURE.get(ws)->cache());
		Analysis::setup(ws);
	}

	void cleanup(WorkSpace *ws) override {
		gc.runGC();
		Analysis::cleanup(ws);
	}

	Domain *domainFor(const SetCollection& coll, int set) override {
		return new MAY(coll, set, A, gc);
	}

	int A;
	ListGC gc;
};

///
p::declare MAYAnalysis::reg = p::init("otawa::dcache::MAYAnalysis", Version(1, 0, 0))
	.make<MAYAnalysis>()
	.extend<Analysis>()
	.provide(MAY_FEATURE);


/**
 * Provides result of the instruction cache MUST analysis.
 * @ingroup dcache
 */
p::interfaced_feature<AgeInfo> MAY_FEATURE("otawa::dcache::MAY_FEATURE", p::make<MAYAnalysis>());


/**
 * @class MAY
 * Provides the implementation of the domain for the MAY analysis.
 * @ingroup dcache
 */

///
MAY::MAY(const SetCollection& collection, int set, int assoc, ListGC& gc):
	ACSDomain(collection, set, assoc, 0, gc),
	EMPTY(make(0))
{
	ASSERT(A > 0);
}

///
ai::State *MAY::entry() {
	return EMPTY;
}

///
ai::State *MAY::join(ai::State *_1, ai::State *_2) {
	auto s1 = acs(_1), s2 = acs(_2);
	if(s1 == BOT)
		return s2;
	else if(s2 == BOT)
		return s1;
	else if(s1 == TOP || s2 == TOP)
		return TOP;
	else {
		os = make();
		auto s = 0;
		for(int i = 0; i < N; i++) {
			os->age[i] = min(s1->age[i], s2->age[i]);
			s += os->age[i];
		}
		if(s == sumA)
			return TOP;
		return os;
	}
}

///
ai::State *MAY::update(Edge *e, ai::State *s) {
	os = acs(s);
	for(const auto& a: *ACCESSES(e->sink()))
		if(a.access(S))
			os = acs(update(a, os));
	return os;
}

///
ai::State *MAY::update(const Access& a, ai::State *s_) {
	auto s = acs(s_);
	if(!a.access(S))
		return s;
	if(s == BOT)
		return s;

	switch(a.action()) {

	case NO_ACCESS:
		break;

	case LOAD:
	case STORE:		
		switch(a.kind()) {
		case ANY:	return accessAny(s);		
		case BLOCK:	return access(s, a.block()->id());
		case ENUM:	return access(s, a.blockIn(S)->id());
		case RANGE:	return accessAny(s);
		}
		break;

	case PURGE:
		
		switch(a.kind()) {
		case ANY:	return TOP;
		case BLOCK:	return purge(s, a.block()->id());
		case ENUM:	return purge(s, a.block()->id());
		case RANGE:	return TOP;
		}
		break;
	
	default:
		break;
	}

	return s;
}

///
ACS *MAY::access(ACS *is, int b) {
	os = make();
	auto ba = is->age[b];
	for(int i = 0; i < N; i++)
		if(is->age[i] <= ba && is->age[i] != A)
			os->age[i] = is->age[i] + 1;
		else
			os->age[i] = is->age[i];
	os->age[b] = 0;
	return os;
}

///
ACS *MAY::purge(ACS *is, int b) {
	os = copy(is);
	os->age[b] = A;
	if(sum(os) == sumA)
		return TOP;
	return os;
}

///
ACS *MAY::accessAny(ACS *is) {
	return is;
}

} };	// otawa::icat
