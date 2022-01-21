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

#include <elm/array.h>
#include <elm/data/ListMap.h>
#include <otawa/dcache/Analysis.h>
#include <otawa/dcache/MUST.h>

namespace otawa { namespace dcache {

/**
 * Implements the MUST instruction cache analysis.
 * @ingroup dcache
 */
class MUSTAnalysis: public Analysis, public AgeInfo, public GCManager {
public:
	static p::declare reg;
	MUSTAnalysis(): Analysis(reg), A(0), gc(*this) { }

	void *interfaceFor(const AbstractFeature& f) override {
		if(&f == &MUST_FEATURE)
			return static_cast<AgeInfo *>(this);
		else
			return nullptr;
	}

	int wayCount() override {
		return A;
	}

	int age(otawa::Block *v, const Access& a, const CacheBlock *b) override {
		auto s = acs(at(v, a, b->set()));
		auto r = s->age[b->id()];
		Analysis::release(s);
		return r;
	}

	int age(Edge *e, const Access& a, const CacheBlock *b) override {
		auto s = acs(at(e, a, b->set()));
		auto r = s->age[b->id()];
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

	void collect(AbstractGC& gc) override {
		Analysis::collect([&](ai::State *s) { gc.mark(s, sizeof(ACS)); });
	}

	void clean(void *p) override {
		reinterpret_cast<ACS *>(p)->~ACS();
	}

protected:

	void setup(WorkSpace *ws) override {
		A = dcache::actualAssoc(ACCESS_FEATURE.get(ws)->cache());
		Analysis::setup(ws);
	}

	void cleanup(WorkSpace *ws) {
		gc.runGC();
		Analysis::cleanup(ws);
	}

	Domain *domainFor(const SetCollection& coll, int set) override {
		return new MUST(coll, set, A, gc);
	}

	int A;
	ListMap<ACS *, int> kept;
	ListGC gc;
};

///
p::declare MUSTAnalysis::reg = p::init("otawa::dcache::MUSTAnalysis", Version(1, 0, 0))
	.make<MUSTAnalysis>()
	.extend<Analysis>()
	.provide(MUST_FEATURE);


/**
 * Provides result of the instruction cache MUST analysis.
 */
p::interfaced_feature<AgeInfo> MUST_FEATURE("otawa::dcache::MUST_FEATURE", p::make<MUSTAnalysis>());


/**
 * @class MUST
 * Provides the implementation of the domain for the MUST analysis.
 * @ingroup dcache
 */

///
MUST::MUST(const SetCollection& collection, int set, int assoc, ListGC& gc):
	ACSDomain(collection, set, assoc, assoc, gc)
	{ }


///
ai::State *MUST::join(ai::State *_1, ai::State *_2) {
	auto s1 = acs(_1), s2 = acs(_2);
	if(s1 == BOT)
		return s2;
	else if(s2 == BOT)
		return s1;
	else if(s1 == TOP || s2 == TOP)
		return TOP;
	else {
		os = make(N);
		auto s = 0;
		for(int i = 0; i < N; i++) {
			os->age[i] = max(s1->age[i], s2->age[i]);
			s += os->age[i];
		}
		if(s == sumA)
			return TOP;
		return os;
	}
}

///
ai::State *MUST::update(Edge *e, ai::State *s) {
	os = acs(s);
	for(const auto& a: *ACCESSES(e->sink()))
		if(a.access(S))
			os = acs(update(a, os));
	return os;
}

///
ai::State *MUST::update(const Access& a, ai::State *s_) {
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
ACS *MUST::access(ACS *is, int b) {
	if(is == BOT)
		return is;
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
ACS *MUST::preaccess(ACS *is, int b) {
	os = make();
	auto ba = is->age[b];
	for(int i = 0; i < N; i++)
		if(is->age[i] <= ba)
			os->age[i] = is->age[i] + 1;
		else
			os->age[i] = is->age[i];
	return os;
}

///
ACS *MUST::purge(ACS *is, int b) {
	os = copy(is);
	os->age[b] = A;
	if(sum(os) == sumA)
		return TOP;
	return os;
}

///
ACS *MUST::accessAny(ACS *is) {
	os = make();
	auto s = 0;
	for(int i = 0; i < N; i++) {
		os->age[i] = min(int(A), is->age[i] + 1);
		s += os->age[i];
	}
	if(s == sumA)
		return TOP;
	return os;
}

} };	// otawa::dcache
