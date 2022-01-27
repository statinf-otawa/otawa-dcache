/*
 *	ACS class interface
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
#ifndef OTAWA_DCACHE_ACS_H_
#define OTAWA_DCACHE_ACS_H_

#include <elm/alloc/ListGC.h>
#include "Analysis.h"
#include "features.h"

namespace otawa { namespace dcache {

class GCState: public ai::State {
public:
	virtual ~GCState();
	virtual void mark(AbstractGC& gc) = 0;
};

class ACS: public GCState {
public:
	typedef t::uint8 age_t;
	static const age_t BOT = 255;
	inline ACS(int N): age(new age_t[N]) { }
	inline ACS(int N, age_t a): age(new age_t[N]) { for(int i = 0; i < N; i++) age[i] = a; }
	inline ACS(int N, const ACS& a): age(new age_t[N]) { array::copy(age, a.age, N); }
	inline ~ACS() { delete [] age; }
	age_t *age;
	void print(const dcache::SetCollection& collection, int set, io::Output& out);
	void save(int N, io::OutStream *out);
	void load(int N, io::InStream *out);
	bool equals(int N, ACS *a);
	void mark(AbstractGC& gc) override;
};
inline ACS *acs(ai::State *s) { return static_cast<ACS *>(s); }

class ACSDomain: public dcache::Domain {
public:
	ACSDomain(const dcache::SetCollection& coll, int set, int assoc, int top, ListGC& gc_);

	ai::State *bot() override;
	ai::State *top() override;
	ai::State *entry() override;
	bool equals(ai::State *s1, ai::State *s2) override;

	bool implementsPrinting() override;
	void print(ai::State *s, io::Output& out) override;

	bool implementsIO() override;
	void save(ai::State *s, io::OutStream *out) override;
	ai::State *load(io::InStream *in) override;

	void collect(ai::state_collector_t f) override;

protected:
	const dcache::SetCollection& coll;
	ListGC& gc;
	int N;
	ACS::age_t A, sumA;
	ACS *BOT, *TOP, *os;
	inline ACS *make(int i = ACS::BOT) const { return new(gc.alloc<ACS>()) ACS(N, i); }
	inline ACS *copy(ACS *a) const { return new(gc.alloc<ACS>()) ACS(N, *a); }
	inline int sum(ACS *a) const
		{ int s = 0; for(int i = 0; i < N; i++) s += a->age[i]; return s; }
};

} }		// otawa::dcache

#endif /* OTAWA_DCACHE_ACS_H_ */



