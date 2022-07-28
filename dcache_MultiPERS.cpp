/*
 *	MultiPERS Domain implementation
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

#include <otawa/cfg/Loop.h>
#include  "otawa/dcache/MultiPERS.h"

namespace otawa { namespace dcache {
	
/**
 * @class MultiACS
 * Implements the multi-persistence analysis: the ACS is a stack of ACS where
 * each element corresponds to a loop level. This lets the persistence
 * analysis to apply to inner loops and to improve its precision.
 * @ingroup dcache
 */

/**
 * Build a multi-ACS with the given depth.
 * @param D		Depth of the ACS.
 * @param I		Initial value of each ACS (optional).
 */
MultiACS::MultiACS(int D, ACS *I): as(D, new ACS *[D]) {
	for(int i = 0; i < D; i++)
		as[i] = I;
}

/**
 * Build an ACS by duplicating the given one.
 * @param a		Duplicated ACS.
 */
MultiACS::MultiACS(MultiACS *a): as(a->as.count(), new ACS *[a->as.count()]) {
	for(int i = 0; i < as.count(); i++)
		as[i] = a->as[i];
}

/**
 * Build a multi-ACS by copying the given but with a different depth.
 * If the depth is less than the depth of a, the copy is just truncated.
 * Else the additional ACS is set to i.
 * @param a		Multi-ACS to copy.
 * @param D		Resulting ACS depth.
 * @param i		ACS to initialize non-copied part.
 */
MultiACS::MultiACS(MultiACS *a, int D, ACS *i): MultiACS(a, a->as.count(), D, i) {
}


/**
 * Build a multi-ACS by copying the given one but with a different depth
 * only a part of the original ACS.
 * 
 * The OD ACS of original multi-ACS are copied as is and ACS in depth OD+1 to ND
 * (if any) are initialized with i.
 * @param a		Multi-ACS to copy.
 * @param OD	Original ACS depth to preserve (must be less than a depth).
 * @param ND	Resulting ACS depth.
 * @param i		ACS to initialize non-copied part.
 */
MultiACS::MultiACS(MultiACS *a, int OD, int ND, ACS *i): as(ND, new ACS *[ND]) {
	ASSERT(OD <= a->as.count());
	int b = min(OD, ND);
	for(int j = 0; j < b; j++)
		as[j] = a->as[j];
	for(int j = b; j < ND; j++)
		as[j] = i;
}


///
void MultiACS::mark(AbstractGC& gc) {
	gc.mark(this, sizeof(MultiACS));
	for(auto p: as)
		if(p != nullptr)
			gc.mark(p, sizeof(ACS));
}


/**
 * @class MultiPERS
 *
 */
MultiPERS::MultiPERS(const SetCollection& coll, int set, int assoc, ListGC& gc_):
	Domain(set),
	pers(coll, set, assoc, gc_),
	gc(gc_),
	BOT(make(1, acs(pers.bot()))),
	TOP(make(1, acs(pers.top()))),
	os(BOT)
{
}

///
ai::State *MultiPERS::bot() {
	return BOT;
}

///
ai::State *MultiPERS::top() {
	return TOP;
}

///
ai::State *MultiPERS::entry() {
	return TOP;
}

static inline MultiACS *multi(ai::State *s) { return static_cast<MultiACS *>(s); }

///
bool MultiPERS::equals(ai::State *s1_, ai::State *s2_) {
	auto s1 = multi(s1_), s2 = multi(s2_);
	if(s1->as.count() != s2->as.count())
			return false;
	for(int i = 0; i < s1->as.count(); i++)
		if(!pers.equals(s1->as[i], s2->as[i]))
			return false;
	return true;
}

///
ai::State *MultiPERS::join(ai::State *s1_, ai::State *s2_) {
	auto s1 = multi(s1_), s2 = multi(s2_);
	if(s1 == BOT)
		return s2;
	else if(s2 == BOT)
		return s1;
	else {
		if(s1->as.count() < s2->as.count())
			swap(s1, s2);
		os = copy(s1);
		for(int i = 0; i < s2->as.count(); i++)
			os->as[i] = acs(pers.join(s1->as[i], s2->as[i]));
		return os;
	}
}


///
ai::State *MultiPERS::update(Edge *e, ai::State *s_) {
	auto s = multi(s_);
	if(LOOP_EXIT(e))
		os = copy(s, s->as.count() + Loop::of(e->sink())->depth() - Loop::of(e->source())->depth(), acs(pers.entry()));
	else if(LOOP_ENTRY(e))
		os = copy(s, s->as.count() + 1, acs(pers.entry()));	
	else if(!e->source()->isSynth())
		os = s;
	else {
		int d = ds.get(e->source(), -1);
		if(d == -1)
			return BOT;
		else if(d == s->as.count())
			os = copy(s);
		else
			os = copy(s, d, acs(pers.entry()));		
	}
	return os;
}


///
ai::State *MultiPERS::update(Block *v, ai::State *s_) {
	auto s = multi(s_);

	// BOT case
	if(s == BOT)
		return BOT;
	
	// function call case
	if(v->isSynth())
		ds.put(v, s->as.count());

	// update the state
	for(const auto& a: *ACCESSES(v))
		if(a.access(S))
			for(int i = 0; i < os->as.count(); i++)
				os->as[i] = acs(pers.update(a, os->as[i]));
	return os;
}

///
bool MultiPERS::implementsPrinting() {
	return true;
}

///
void MultiPERS::print(ai::State *s_, io::Output& out) {
	auto s = multi(s_);
	out << "{ ";
	for(int i = 0; i < s->as.count(); i++) {
		if(i != 0)
			out << ", ";
		out << "L" << i << ": ";
		pers.print(s->as[i], out);
	}
	out << " }";
}

///
bool MultiPERS::implementsIO() {
	return true;
}

///
void MultiPERS::save(ai::State *s_, io::OutStream *out) {
	auto s = multi(s_);
	t::int32 c = s->as.count();
	if(out->write(reinterpret_cast<char *>(c), sizeof(c)) != sizeof(c))
		throw io::IOException(out->lastErrorMessage());
	for(int i = 0; i < c; i++)
		pers.save(s->as[i], out);
}

///
ai::State *MultiPERS::load(io::InStream *in) {
	t::int32 c;
	if(in->read(&c, sizeof(c)) != sizeof(c))
		throw io::IOException(in->lastErrorMessage());
	os = make(c, acs(pers.bot()));
	for(int i = 0; i < c; i++)
		os->as[i] = acs(pers.load(in));
	return os;
}

///
ai::State *MultiPERS::update(const Access& a, ai::State *s_) {
	auto s = multi(s_);
	if(s == BOT)
		return BOT;
	os = make(s->as.count(), acs(pers.bot()));
	for(int i = 0; i < s->as.count(); i++)
		os->as[i] = acs(pers.update(a, s->as[i]));
	return os;
}

///
void MultiPERS::collect(ai::state_collector_t f) {
	f(BOT);
	f(TOP);
	f(os);
	pers.collect(f);
}

/**
 * Default implementation of MultiPERSAnalsis.
 * @ingroup icat
 */
class MultiPERSAnalysis: public Analysis, public MultiAgeInfo, public GCManager {
public:

	static p::declare reg;
	MultiPERSAnalysis(): Analysis(reg), A(0), gc(*this) { }

	void collect(AbstractGC& gc) override {
		Analysis::collect([&](ai::State *s) { static_cast<GCState *>(s)->mark(gc); });
	}

	void clean(void *p) override {
		reinterpret_cast<ACS *>(p)->~ACS();
	}

	void *interfaceFor(const AbstractFeature& f) override {
		if(&f == &MULTI_PERS_FEATURE)
			return static_cast<MultiAgeInfo *>(this);
		else
			return nullptr;
	}

	int wayCount() override {
		return A;
	}

	int level(Block *b, const Access& a, const CacheBlock *cb) override {
		auto s = multi(at(b, a, cb->set()));
		int i = s->as.length() - 1;
		while(i >= 0 && s->as[i]->age[cb->id()] < A)
			i--;
		Analysis::release(s);
		return s->as.length() - 1 - i;
	}

	int level(Edge *e, const Access& a, const CacheBlock *cb) override {
		auto s = multi(at(e, a, cb->set()));
		int i = s->as.length() - 1;
		while(i >= 0 && s->as[i]->age[cb->id()] < A)
			i--;
		Analysis::release(s);
		return s->as.length() - 1 - i;
	}

	MultiACS *acsAfter(Block *b,int s) override {
		return multi(after(b, s));
	}

	MultiACS *acsBefore(Edge *e, int s) override {
		return acsAfter(e->source(), s);
	}

	MultiACS *acsBefore(Block *b, int s) override {
		return multi(before(b, s));
	}

	MultiACS *acsAfter(Edge *e, int s) override {
		return multi(after(e, s));
	}

	void release(MultiACS *a) {
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
		return new MultiPERS(coll, set, A, gc);
	}

private:
	int A;
	ListGC gc;
};


///
p::declare MultiPERSAnalysis::reg = p::init("otawa::dcache::MultiPERSAnalysis", Version(1, 0, 0))
	.make<MultiPERSAnalysis>()
	.extend<Analysis>()
	.require(otawa::EXTENDED_LOOP_FEATURE)
	.provide(MULTI_PERS_FEATURE);

/**
 * Implements multi-level persistence analysis: this analysis is able to
 * qualify the accesses relatively to the loop level and therefore to
 * provide more precise estimation of persistence.
 *
 * **interface:** MultiAgeInfo
 *
 * **default implementation:** MultiPERSAnalysis
 *
 * @ingroup icat
 */
p::interfaced_feature<MultiAgeInfo> MULTI_PERS_FEATURE("otawa::dcache::MULTI_PERS_FEATURE", p::make<MultiPERSAnalysis>());

} }	// otawa::icat
