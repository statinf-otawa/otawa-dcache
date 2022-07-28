/*
 *	PERS Analysis class
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
#include "otawa/dcache/PERS.h"
#include "otawa/dcache/features.h"

namespace otawa { namespace dcache {

/**
 * Implements the PERS instruction cache analysis.
 * @ingroup dcache
 */
class PERSAnalysis: public Analysis, public AgeInfo, public GCManager {
public:
	static p::declare reg;
	PERSAnalysis(): Analysis(reg), A(0), gc(*this) { }

	void collect(AbstractGC& gc) override {
		Analysis::collect([&](ai::State *s) { gc.mark(s, sizeof(ACS)); });
	}

	void clean(void *p) override {
		reinterpret_cast<ACS *>(p)->~ACS();
	}

	void *interfaceFor(const AbstractFeature& f) override {
		if(&f == &PERS_FEATURE)
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
		return new PERS(coll, set, A, gc);
	}

	int A;
	ListMap<ACS *, int> kept;
	ListGC gc;
};

///
p::declare PERSAnalysis::reg = p::init("otawa::dcache::PERSAnalysis", Version(1, 0, 0))
	.make<PERSAnalysis>()
	.extend<Analysis>()
	.provide(PERS_FEATURE);

/**
 * Provides result of the instruction cache PERS analysis.
 */
p::interfaced_feature<AgeInfo> PERS_FEATURE("otawa::dcache::PERS_FEATURE", p::make<PERSAnalysis>());


/**
 * @class PERS
 * Provides the implementation of the domain for the PERS analysis.
 * @ingroup dcache
 */

///
PERS::PERS(const SetCollection& collection, int set, int assoc, ListGC& gc):
	ACSDomain(collection, set, assoc, assoc, gc),
	EMPTY(make(ACS::BOT))
	{ }

///
ai::State *PERS::entry() {
	return EMPTY;
}

///
void PERS::collect(ai::state_collector_t f) {
	ACSDomain::collect(f);
	f(EMPTY);
}

///
ai::State *PERS::join(ai::State *_1, ai::State *_2) {
	auto s1 = acs(_1), s2 = acs(_2);
	if(s1 == BOT)
		return s2;
	else if(s2 == BOT)
		return s1;
	else {
		auto s = make(N);
		int cnt = 0, sum = 0;
		for(int i = 0; i < N; i++) {
			if(s1->age[i] == ACS::BOT)
				s->age[i] = s2->age[i];
			else if(s2->age[i] == ACS::BOT)
				s->age[i] = s1->age[i];
			else
				s->age[i] = max(s1->age[i], s2->age[i]);
			sum += s->age[i];
			if(s->age[i] < A && s->age[i] != ACS::BOT)
				cnt++;
		}
		if(cnt <= A && sum != sumA)
			return s;
		else
			return TOP;
	}
}

///
ai::State *PERS::update(Block *v, ai::State *s) {
	auto os = acs(s);
	if(os != BOT) {
		for(const auto& a: *ACCESSES(v))
			if(a.access(S))
				os = acs(update(a, os));
	}
	return os;
}

///
ai::State *PERS::update(const Access& a, ai::State *s_) {
	auto s = acs(s_);

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
ACS *PERS::access(ACS *is, int b) const {
	auto os = make(N);
	auto ba = is->age[b];
	if(ba == ACS::BOT)
		ba = A;
	for(int i = 0; i < N; i++)
		if(is->age[i] <= ba && is->age[i] != A && is->age[i] != ACS::BOT)
			os->age[i] = is->age[i] + 1;
		else
			os->age[i] = is->age[i];
	os->age[b] = 0;
	return os;
}


///
ACS *PERS::purge(ACS *is, int b) const {
	auto os = copy(is);
	os->age[b] = A;
	return os;
}

///
ACS *PERS::accessAny(ACS *is) const {
	auto os = make(N);
	for(int i = 0; i < N; i++)
		if(is->age[i] != ACS::BOT)
			os->age[i] = min(int(A), is->age[i] + 1);
		else
			os->age[i] = is->age[i];
	return os;
}

} };	// otawa::dcache
